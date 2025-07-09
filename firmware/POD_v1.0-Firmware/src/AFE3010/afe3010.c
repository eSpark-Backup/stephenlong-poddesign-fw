#include "afe3010.h"

#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(AFE3010_POD, AFE3010_LOG_LEVEL);

#define SCR_PIN_MASK            BIT(AFE3010_SCR_SIG_VA) | BIT(AFE3010_SCR_SIG_VB) | BIT(AFE3010_SCR_SIG_VC)
#define ALARM_PIN_MASK          BIT(AFE3010_ALARM_VA) | BIT(AFE3010_ALARM_VB) | BIT(AFE3010_ALARM_VC)

const struct device *_afe3010_port_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(AFE3010_PORT));
struct gpio_callback _afe3010_cb_data;
static struct k_timer _alarm_pin_sample_timer;

static uint32_t scr_time_stamp[AFE3010_NO_OF_PHASES][3] = {0};          // three timestamps for each phase; falling edge timestamp, rising edge timestamp, debounce timestamp
static uint8_t alarm_samples[AFE3010_NO_OF_PHASES][BUFFER_SIZE];
static uint16_t sample_index[AFE3010_NO_OF_PHASES] = {0};
static bool timer_running = false;

static const char* phase_names[] = {
    [PHASE_A] = "PHASE_A",
    [PHASE_B] = "PHASE_B",
    [PHASE_C] = "PHASE_C",
};

typedef enum {
    PHASE_IDLE,
    PHASE_SAMPLING,
    PHASE_ANALYZING
} phase_state_t;

static phase_state_t phase_state[AFE3010_NO_OF_PHASES] = {
    PHASE_IDLE, PHASE_IDLE, PHASE_IDLE
};

// need to modify this function to account for individual phase sampling, right now it stops after buffers fills up for one of the phase
// DONE
void alarm_sample_handler(struct k_timer *dummy)
{
    bool all_done = true;

    for (int phase = 0; phase < AFE3010_NO_OF_PHASES; phase++) {
        if (phase_state[phase] != PHASE_SAMPLING)
            continue;

        all_done = false;  // At least one phase still sampling

        uint8_t pin = AFE3010_ALARM_VA + phase;
        int val = gpio_pin_get(_afe3010_port_gpio_dev, pin);

        if (sample_index[phase] < BUFFER_SIZE) {
            alarm_samples[phase][sample_index[phase]] = (val > 0) ? 1 : 0;
            sample_index[phase]++;
        }

        // Finish sampling if buffer full
        if (sample_index[phase] >= BUFFER_SIZE) {
            LOG_DBG("Sampling done for phase %s", phase_names[phase]);
            analyze_alarm_pattern(phase);

            // Reconfigure to interrupt mode
            uint32_t intType = GPIO_INT_EDGE_RISING;
            configure_pin(GPIO_INPUT, pin, true, &intType);

            phase_state[phase] = PHASE_IDLE;
        }
    }

    if (all_done) {
        k_timer_stop(&_alarm_pin_sample_timer);
        timer_running = false;
    }
}

void start_alarm_sampling(enum PHASES_e phase, uint8_t pin)
{
    if (phase_state[phase] == PHASE_SAMPLING)
        return; // Already sampling

    phase_state[phase] = PHASE_SAMPLING;
    sample_index[phase] = 0;

    configure_pin(GPIO_INPUT, pin, false, NULL);

    if (!timer_running) {
        k_timer_start(&_alarm_pin_sample_timer, K_NO_WAIT, K_USEC(AFE3010_ALARM_SAMPLE_TIME));
        timer_running = true;
    }

    LOG_DBG("Sampling started for phase %s", phase_names[phase]);
}

void shared_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int val;
    uint32_t now = k_uptime_get();

    if (pins & BIT(AFE3010_SCR_SIG_VA)) {
        int val = gpio_pin_get(dev, AFE3010_SCR_SIG_VA);
        if ((now - scr_time_stamp[PHASE_A][2]) >= DEBOUNCE_MS) {
            scr_time_stamp[PHASE_A][val] = now;
            scr_time_stamp[PHASE_A][2] = now;
            LOG_DBG("Interrupt from AFE3010_SCR_SIG_VA PIN - edge: %s", (val ? "RISING" : "FALLING"));
        }
    }
    if (pins & BIT(AFE3010_SCR_SIG_VB)) {
        int val = gpio_pin_get(dev, AFE3010_SCR_SIG_VB);
        if ((now - scr_time_stamp[PHASE_B][2]) >= DEBOUNCE_MS) {
            scr_time_stamp[PHASE_B][val] = now;
            scr_time_stamp[PHASE_B][2] = now;
            LOG_DBG("Interrupt from AFE3010_SCR_SIG_VB PIN - edge: %s", (val ? "RISING" : "FALLING"));
        }
    }
    if (pins & BIT(AFE3010_SCR_SIG_VC)) {
        int val = gpio_pin_get(dev, AFE3010_SCR_SIG_VC);
        if ((now - scr_time_stamp[PHASE_C][2]) >= DEBOUNCE_MS) {
            scr_time_stamp[PHASE_C][val] = now;
            scr_time_stamp[PHASE_C][2] = now;
            LOG_DBG("Interrupt from AFE3010_SCR_SIG_VC PIN - edge: %s", (val ? "RISING" : "FALLING"));
        }
    }

    // ALARM edge detection
    if ((pins & BIT(AFE3010_ALARM_VA)) && phase_state[PHASE_A] == PHASE_IDLE) {
        LOG_DBG("Rising edge: ALARM_VA");
        start_alarm_sampling(PHASE_A, AFE3010_ALARM_VA);
    }
    if ((pins & BIT(AFE3010_ALARM_VB)) && phase_state[PHASE_B] == PHASE_IDLE) {
        LOG_DBG("Rising edge: ALARM_VB");
        start_alarm_sampling(PHASE_B, AFE3010_ALARM_VB);
    }
    if ((pins & BIT(AFE3010_ALARM_VC)) && phase_state[PHASE_C] == PHASE_IDLE) {
        LOG_DBG("Rising edge: ALARM_VC");
        start_alarm_sampling(PHASE_C, AFE3010_ALARM_VC);
    }
}

// void analyze_alarm_pattern(enum PHASES_e phase)
// {
//     const uint8_t* buf = alarm_samples[phase];

//     int high_transitions = 0;
//     int low_transitions = 0;
//     // int cycles = 0;
//     int last_val = buf[0];
//     int burst_start = -1;
//     int burst_lengths[10] = {0}; // support up to 10 bursts
//     int burst_count = 0;

//     for (int i = 1; i < BUFFER_SIZE; ++i) {
//         int val = buf[i];

//         if (val != last_val) {
//             if (val == 1) high_transitions++;
//             else          low_transitions++;

//             if (val == 1 && burst_start == -1) {
//                 burst_start = i;
//             }

//             if (val == 0 && burst_start != -1) {
//                 int len = i - burst_start;
//                 if (len > 300) {
//                     if (burst_count < 10) {
//                         burst_lengths[burst_count++] = len;
//                     }
//                 }
//                 burst_start = -1;
//             }

//             last_val = val;
//         }
//     }

//     // Evaluate SCR state
//     bool scr_active = false;
//     uint32_t now = k_uptime_get();
//     int current_val = gpio_pin_get(_afe3010_port_gpio_dev, (phase + GROUND_FAULT_SCR_SIG_VA));
//     if (current_val == 0 || (now - scr_time_stamp[phase][0]) < SCR_ACTIVE_WINDOW_MS) {
//         scr_active = true;
//     }

//     int high_count = 0;
//     for (int i = 0; i < BUFFER_SIZE; i++) {
//         if (buf[i]) high_count++;
//     }

//     LOG_INF("Phase %s: bursts = %d, high_count = %d, scr = %s",
//         phase_names[phase], burst_count, high_count, scr_active ? "ACTIVE" : "INACTIVE");

//     if (scr_active) {
//         if (high_count > 5000) {
//             LOG_INF("Detected: Ground fault on phase %s", phase_names[phase]);
//             return;
//         } else {
//             LOG_WRN("SCR active but weak signal on phase %s", phase_names[phase]);
//             return;
//         }
//     }

//     // --- Classify based on burst pattern ---
//     if (burst_count == 1 && burst_lengths[0] >= 600 && burst_lengths[0] <= 900) {           // 3KHz signal for 250ms, 3000/4 = 750 high transitions
//         LOG_INF("Detected: POR success (250ms burst) on phase %s", phase_names[phase]);
//         return;
//     }

//     if (burst_count >= 2) {
//         int avg_len = 0;
//         for (int i = 0; i < burst_count; i++) {
//             avg_len += burst_lengths[i];
//         }
//         avg_len /= burst_count;

//         if (avg_len >= 2800 && avg_len <= 3200) {
//             LOG_INF("Detected: PTT failure (1Hz blink) on phase %s", phase_names[phase]);
//             return;
//         }

//         if (avg_len >= 5800 && avg_len <= 6400) {
//             LOG_INF("Detected: POR failure (0.5Hz blink) on phase %s", phase_names[phase]);
//             return;
//         }

//         if (avg_len >= 3400 && avg_len <= 3800) {
//             LOG_INF("Detected: CT disconnected (0.4Hz blink) on phase %s", phase_names[phase]);
//             return;
//         }
//     }

//     if (high_count < 100) {
//         LOG_INF("Detected: Normal operation (idle) on phase %s", phase_names[phase]);
//         return;
//     }

//     LOG_WRN("Unclassified signal on phase %s: bursts = %d, avg_len ≈ %d, high_count = %d",
//             phase_names[phase], burst_count,
//             (burst_count > 0 ? burst_lengths[0] : -1), high_count);
// }
void analyze_alarm_pattern(enum PHASES_e phase)
{
    uint32_t now = k_uptime_get();
    int high_count = 0;
    int transitions = 0;
    int last_val = alarm_samples[phase][0];
    int burst_start = -1;
    int burst_lengths[32] = {0};
    int burst_count = 0;

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        int val = alarm_samples[phase][i];
        if (val) high_count++;

        // Count transitions
        if (val != last_val) {
            transitions++;
            last_val = val;
        }

        // Burst start
        if (val == 1 && burst_start == -1) {
            burst_start = i;
        }

        // Burst end
        if ((val == 0 || i == BUFFER_SIZE - 1) && burst_start != -1) {
            int len = i - burst_start;
            if (len > 10) {  // Accept shorter pulses for 3kHz
                burst_lengths[burst_count++] = len;
            }
            burst_start = -1;
        }
    }

    bool scr_active = false;
    int scr_val = gpio_pin_get(_afe3010_port_gpio_dev, (phase + GROUND_FAULT_SCR_SIG_VA));

    if (scr_val == 0 || (now - scr_time_stamp[phase][0]) < SCR_ACTIVE_WINDOW_MS) {
        scr_active = true;
    }

    int avg_burst_len = -1;
    if (burst_count > 0) {
        int total = 0;
        for (int i = 0; i < burst_count; i++) total += burst_lengths[i];
        avg_burst_len = total / burst_count;
    }

    int approx_khz = (transitions / 2); // transitions = high/low edges
    LOG_INF("Phase %s: bursts = %d, high_count = %d, scr = %s", 
            phase_names[phase], burst_count, high_count, scr_active ? "ACTIVE" : "INACTIVE");

    if (scr_active) {
        if (high_count > 2000) {
            LOG_DBG("Detected: Ground fault on phase %s", phase_names[phase]);
            return;
        } else {
            LOG_DBG("Detected: Possible noise during SCR active on phase %s", phase_names[phase]);
            return;
        }
    }

    if (!scr_active) {
        if (high_count < 100 && transitions < 200) {
            LOG_DBG("Detected: Normal operation on phase %s", phase_names[phase]);
            return;
        }

        if (burst_count > 3 && avg_burst_len >= 500 && avg_burst_len <= 1500) {
            LOG_DBG("Detected: Self-test failure (1Hz blink) on phase %s", phase_names[phase]);
            return;
        }

        if (transitions >= 6000 && high_count >= 3000) {
            LOG_DBG("Detected: CT Disconnected (1.2s ON/OFF 3kHz blink) on phase %s", phase_names[phase]);
            return;
        }
    }

    LOG_WRN("Unclassified signal on phase %s: bursts = %d, avg_len ≈ %d, high_count = %d", 
        phase_names[phase], burst_count, avg_burst_len, high_count);
}


void configure_pin(uint32_t pinType, uint8_t pin, bool isIntPin, uint32_t *intType)
{
    int err = gpio_pin_configure(_afe3010_port_gpio_dev, pin, pinType);
    if(isIntPin)
    {
        err |= gpio_pin_interrupt_configure(_afe3010_port_gpio_dev, pin, *intType);
    }
    else
    {
        gpio_pin_interrupt_configure(_afe3010_port_gpio_dev, pin, GPIO_INT_DISABLE);
    }

    if(err)
    {
        LOG_ERR("Error while configuring GPIO %s.%d as %s %s",
                _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"),
                ((true == isIntPin) ? "(interrupt enabled)" : "") );
        return;
    }
    
    LOG_DBG("Configured %s.%d as %s %s",
            _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"),
            ((true == isIntPin) ? "(interrupt enabled)" : "") );
}

void configure_ptt(void)
{
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VA, false, NULL);
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VB, false, NULL);
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VC, false, NULL);
}

void configure_scr_sig(void)
{
    uint32_t intType = GPIO_INT_EDGE_BOTH;
    // uint32_t intType = GPIO_INT_EDGE_FALLING;
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VA, true, &intType);
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VB, true, &intType);
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VC, true, &intType);
}

void configure_alarm(void)
{
    uint32_t intType = GPIO_INT_EDGE_RISING;
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VA, true, &intType);
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VB, true, &intType);
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VC, true, &intType);
}

void init_afe3010(void)
{
    if (!_afe3010_port_gpio_dev) {
        LOG_ERR("Error: Failed to bind to GPIO port");
        return;
    }

    configure_ptt();
    configure_scr_sig();
    configure_alarm();

    // Initialize callback on both pins
    gpio_init_callback(&_afe3010_cb_data, shared_callback,
                       SCR_PIN_MASK | ALARM_PIN_MASK );
    gpio_add_callback(_afe3010_port_gpio_dev, &_afe3010_cb_data);


    // Init and start k_timer
    k_timer_init(&_alarm_pin_sample_timer, alarm_sample_handler, NULL);
    // k_timer_start(&_alarm_pin_sample_timer, K_USEC(AFE3010_ALARM_SAMPLE_TIME), K_USEC(AFE3010_ALARM_SAMPLE_TIME));

    // while(1)
    // {
    //     // LOG_INF("Toggling PTT pin");
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 0);
    //     // k_sleep(K_MSEC(10));
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 1);

    //     LOG_INF("scr_time_stamp[PHASE_A] -> [FALLING]: %d    [RISING]: %d", scr_time_stamp[PHASE_A][0], scr_time_stamp[PHASE_A][1]);
    //     LOG_INF("scr_time_stamp[PHASE_B] -> [FALLING]: %d    [RISING]: %d", scr_time_stamp[PHASE_B][0], scr_time_stamp[PHASE_B][1]);
    //     LOG_INF("scr_time_stamp[PHASE_C] -> [FALLING]: %d    [RISING]: %d", scr_time_stamp[PHASE_C][0], scr_time_stamp[PHASE_C][1]);
        
    //     k_sleep(K_MSEC(1000));
    // }

    LOG_DBG("AFE3010 Ground Fault detection configured");
}