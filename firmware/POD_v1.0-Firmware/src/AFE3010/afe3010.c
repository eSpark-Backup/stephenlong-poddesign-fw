#include "afe3010.h"

#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(AFE3010_POD, AFE3010_LOG_LEVEL);

#define SCR_PIN_MASK            BIT(AFE3010_SCR_SIG_VA) | BIT(AFE3010_SCR_SIG_VB) | BIT(AFE3010_SCR_SIG_VC)
#define ALARM_PIN_MASK          BIT(AFE3010_ALARM_VA) | BIT(AFE3010_ALARM_VB) | BIT(AFE3010_ALARM_VC)

#define BURST_LEN_TO_MS(x)      ((x*AFE3010_ALARM_SAMPLE_TIME)/1000.0f)
#define MAX_MEMBERS_TO_CHECK    10

const struct device *_afe3010_port_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(AFE3010_PORT));
struct gpio_callback _afe3010_cb_data;
static struct k_timer _alarm_pin_sample_timer;

volatile static uint32_t scr_time_stamp[AFE3010_NO_OF_PHASES][3] = {0};          // three timestamps for each phase; falling edge timestamp, rising edge timestamp, debounce timestamp
volatile static uint8_t alarm_samples[AFE3010_NO_OF_PHASES][BUFFER_SIZE];
volatile static uint16_t sample_index[AFE3010_NO_OF_PHASES] = {0};
volatile static bool timer_running = false;

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

enum STATES_e
{
    LOW,
    HIGH,
    STATE_TIMESTAMP,
};

static phase_state_t phase_state[AFE3010_NO_OF_PHASES] = {
    PHASE_IDLE, PHASE_IDLE, PHASE_IDLE
};

void alarm_sample_handler(struct k_timer *dummy)
{
    bool all_done = true;
    static uint32_t phase_timestamp[AFE3010_NO_OF_PHASES] = {0};
    static uint32_t samples_skipped[AFE3010_NO_OF_PHASES] = {0};
    uint32_t now = k_uptime_get();

    for (int phase = PHASE_A; phase < AFE3010_NO_OF_PHASES; phase++) {
        if (phase_state[phase] != PHASE_SAMPLING)
        {
            samples_skipped[phase]++;
            if((now - phase_timestamp[phase]) > 500)
            {
                LOG_WRN("Skipping sample on %s, samples_skipped:%d", phase_names[phase], samples_skipped[phase]);
                phase_timestamp[phase] = now;
                samples_skipped[phase] = 0;
            }
            continue;
        }

        all_done = false;  // At least one phase is still sampling

        uint8_t pin = AFE3010_ALARM_VA + phase;
        int val = gpio_pin_get(_afe3010_port_gpio_dev, pin);

        if (sample_index[phase] < BUFFER_SIZE) {
            alarm_samples[phase][sample_index[phase]] = (val > 0) ? 1 : 0;
            sample_index[phase]++;
        }

        // Finish sampling if buffer full
        if (sample_index[phase] >= BUFFER_SIZE) {
            LOG_DBG("Sampling done for phase %s", phase_names[phase]);
            // analyze_alarm_pattern(phase);            // do not analyze buffer in ISR context 

            // Reconfigure to interrupt mode
            uint32_t intType = GPIO_INT_EDGE_RISING;
            configure_pin(GPIO_INPUT, pin, true, &intType);

            phase_state[phase] = PHASE_ANALYZING;
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
        k_timer_start(&_alarm_pin_sample_timer, K_USEC(AFE3010_ALARM_SAMPLE_TIME), K_USEC(AFE3010_ALARM_SAMPLE_TIME));
        timer_running = true;
    }

    LOG_DBG("Sampling started for %s", phase_names[phase]);
}

void shared_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t now = k_uptime_get();
    uint32_t relevant_pins = pins & ( BIT(AFE3010_SCR_SIG_VA) | BIT(AFE3010_SCR_SIG_VB) | BIT(AFE3010_SCR_SIG_VC) |
                                      BIT(AFE3010_ALARM_VA)   | BIT(AFE3010_ALARM_VB)   | BIT(AFE3010_ALARM_VC)   );
    if (__builtin_popcount(relevant_pins) > 1) {         // check if callback was triggerred with multiple pins as argument; is it even applicable?
        LOG_ERR("Multiple pins triggered callback. pinCount:%d", relevant_pins);
        return;
    }

    int val = gpio_pin_get(dev, pins);

    // taking scr_pin timestamps for later use
    if (pins & BIT(AFE3010_SCR_SIG_VA)) {
        scr_time_stamp[PHASE_A][val] = now;
        scr_time_stamp[PHASE_A][STATE_TIMESTAMP] = now;
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VA PIN - edge: %s", (val ? "RISING" : "FALLING"));
    }
    if (pins & BIT(AFE3010_SCR_SIG_VB)) {
        scr_time_stamp[PHASE_B][val] = now;
        scr_time_stamp[PHASE_B][STATE_TIMESTAMP] = now;
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VB PIN - edge: %s", (val ? "RISING" : "FALLING"));
}
    if (pins & BIT(AFE3010_SCR_SIG_VC)) {
        scr_time_stamp[PHASE_C][val] = now;
        scr_time_stamp[PHASE_C][STATE_TIMESTAMP] = now;
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VC PIN - edge: %s", (val ? "RISING" : "FALLING"));
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

void analyze_alarm_pattern(enum PHASES_e phase)
{
    uint32_t now = k_uptime_get();
    int high_count = 0;
    int transitions = 0;
    int last_val = alarm_samples[phase][0];
    int burst_start = -1;
    int burst_lengths[32] = {0};
    int burst_count = 0;

    // log the buffer for manual checking
    // LOG_DBG("%s buffer contents: ", phase_names[phase]);
    // LOG_HEXDUMP_DBG(&alarm_samples[phase][6777], BUFFER_SIZE/16, "");

    
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
        bool is_signal_present = false;
        uint8_t members_to_check = MIN(BUFFER_SIZE - i, MAX_MEMBERS_TO_CHECK);
        for (uint8_t index = 0; index < members_to_check; index++)
        {
            is_signal_present |= alarm_samples[phase][i+index];
        }
        if (!is_signal_present && burst_start != -1) {
            int len = i - burst_start;
                burst_lengths[burst_count++] = len;
            burst_start = -1;
        }
    }

    bool scr_active = false;
    int scr_val = gpio_pin_get(_afe3010_port_gpio_dev, (phase + GROUND_FAULT_SCR_SIG_VA));

    if ((scr_val == LOW) || (now - scr_time_stamp[phase][LOW]) < SCR_ACTIVE_WINDOW_MS) {
        scr_active = true;
    }

    int avg_burst_len = -1;
    char burst_str[768];
    int offset = 0;
    if (burst_count) {
        int total = 0;                          // for average burst count calculation
        offset += snprintf(burst_str + offset, sizeof(burst_str) - offset, "bursts = %d [lengths: ", burst_count);
        for (int i = 0; i < burst_count; i++) {
            offset += snprintf(burst_str + offset, sizeof(burst_str) - offset, "{%d: %d (%0.2f ms)} ", i+1, burst_lengths[i], (double)(BURST_LEN_TO_MS(burst_lengths[i])));
            total += burst_lengths[i];          // for average burst count calculation
        }
        avg_burst_len = total / burst_count;    // for average burst count calculation
        snprintf(burst_str + offset, sizeof(burst_str) - offset, "]");
    } else {
        snprintf(burst_str, sizeof(burst_str), "No bursts");
    }

    LOG_INF("%s: %s, high_count = %d, scr = %s", 
            phase_names[phase], burst_str, high_count, scr_active ? "ACTIVE" : "INACTIVE");

    if (scr_active) {
        if (high_count > 4000) {
            LOG_DBG("Detected: Ground fault on %s", phase_names[phase]);
            return;
        } else {
            LOG_DBG("Detected: Possible noise during SCR active on %s", phase_names[phase]);
            return;
        }
    }
    else {
        if (high_count < 100 && transitions < 200) {
            LOG_DBG("Detected: Normal operation on %s", phase_names[phase]);
            return;
        }

        if (burst_count > 1 && BURST_LEN_TO_MS(avg_burst_len) >= 500 && BURST_LEN_TO_MS(avg_burst_len) <= 1500) {
            LOG_DBG("Detected: Self-test failure (1Hz blink) on %s", phase_names[phase]);
            return;
        }

        if (transitions >= 6000 && high_count >= 3000) {
            LOG_DBG("Detected: CT Disconnected (1.2s ON/OFF 3kHz blink) on %s", phase_names[phase]);
            return;
        }
    }

    LOG_WRN("Unclassified signal on %s: bursts = %d, avg_len ≈ %0.2f ms, high_count = %d", 
        phase_names[phase], burst_count, (double)BURST_LEN_TO_MS(avg_burst_len), high_count);
}


void configure_pin(uint32_t pinType, uint8_t pin, bool isIntPin, uint32_t *intType)
{
    const char* int_str = "";

    int err = gpio_pin_configure(_afe3010_port_gpio_dev, pin, pinType);
    if(isIntPin)
    {
        err |= gpio_pin_interrupt_configure(_afe3010_port_gpio_dev, pin, *intType);

        if (*intType == GPIO_INT_EDGE_BOTH) {
            int_str = "(interrupt enabled RISING/FALLING)";
        } else if (*intType == GPIO_INT_EDGE_RISING) {
            int_str = "(interrupt enabled RISING)";
        } else if (*intType == GPIO_INT_EDGE_FALLING) {
            int_str = "(interrupt enabled FALLING)";
        } else {
            int_str = "(UNKNOWN interrupt setting)";
        }
    }
    else
    {
        gpio_pin_interrupt_configure(_afe3010_port_gpio_dev, pin, GPIO_INT_DISABLE);
    }

    if(err)
    {
        LOG_ERR("Error while configuring GPIO %s.%d as %s %s",
                _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"),
                ((true == isIntPin) ? int_str : "") );
        return;
    }
    
    LOG_DBG("Configured %s.%d as %s %s",
            _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"),
            ((true == isIntPin) ? int_str : "") );
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

void analyze_ground_faults(uint32_t now)
{
    static uint32_t timeStamp = 0;
    if((now - timeStamp) > 1)
    {
        for(int phase = PHASE_A; phase < AFE3010_NO_OF_PHASES; phase++)
        {
            if (phase_state[phase] == PHASE_ANALYZING)
            {
                analyze_alarm_pattern(phase);
                phase_state[phase] = PHASE_IDLE;
            }
        }
        timeStamp = now;
    }
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
    
    // uint32_t timeStampLog = 0;
    // while(1)
    // {
    //     uint32_t now = k_uptime_get();
    //     // LOG_INF("Toggling PTT pin");
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 0);
    //     // k_sleep(K_MSEC(10));
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 1);


    //     if((now - timeStampLog) > 1000)
    //     {
    //         LOG_INF("Phase A:[Falling:%d Rising:%d] Phase B:[Falling:%d Rising:%d] Phase C:[Falling:%d Rising:%d]",
    //             scr_time_stamp[PHASE_A][LOW], scr_time_stamp[PHASE_A][HIGH],
    //             scr_time_stamp[PHASE_B][LOW], scr_time_stamp[PHASE_B][HIGH],
    //             scr_time_stamp[PHASE_C][LOW], scr_time_stamp[PHASE_C][HIGH]);
    //         timeStampLog = now;
    //     }
        
    //     analyze_ground_faults(now);
        
    //     k_sleep(K_MSEC(1));
    // }

    LOG_DBG("AFE3010 Ground Fault detection configured");
}