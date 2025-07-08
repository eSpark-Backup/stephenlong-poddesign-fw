#include "afe3010.h"

#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(AFE3010_POD, AFE3010_LOG_LEVEL);

#define SCR_PIN_MASK            BIT(AFE3010_SCR_SIG_VA) | BIT(AFE3010_SCR_SIG_VB) | BIT(AFE3010_SCR_SIG_VC)
#define ALARM_PIN_MASK          BIT(AFE3010_ALARM_VA) | BIT(AFE3010_ALARM_VB) | BIT(AFE3010_ALARM_VC)

const struct device *_afe3010_port_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(AFE3010_PORT));
struct gpio_callback _afe3010_cb_data;
static struct k_timer _alarm_pin_sample_timer;

static uint8_t alarm_samples[AFE3010_NO_OF_PHASES][BUFFER_SIZE];
static int sample_index = 0;

static const char* phase_names[] = {
    [PHASE_A] = "PHASE_A",
    [PHASE_B] = "PHASE_B",
    [PHASE_C] = "PHASE_C",
};

void alarm_sample_handler(struct k_timer *dummy)
{
    int val = gpio_pin_get(_afe3010_port_gpio_dev, AFE3010_ALARM_VA);
    alarm_samples[PHASE_A][sample_index] = (val > 0) ? 1 : 0;
    val = gpio_pin_get(_afe3010_port_gpio_dev, AFE3010_ALARM_VB);
    alarm_samples[PHASE_B][sample_index] = (val > 0) ? 1 : 0;
    val = gpio_pin_get(_afe3010_port_gpio_dev, AFE3010_ALARM_VC);
    alarm_samples[PHASE_C][sample_index] = (val > 0) ? 1 : 0;

    sample_index++;

    if (sample_index >= BUFFER_SIZE) {
        sample_index = 0;
        LOG_DBG("Sampling done, analyzing pattern...");
        analyze_alarm_pattern(PHASE_A);
        analyze_alarm_pattern(PHASE_B);
        analyze_alarm_pattern(PHASE_C);
    }
}

void shared_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(AFE3010_SCR_SIG_VA)) {
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VA PIN");
        gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 1);
    }
    if (pins & BIT(AFE3010_SCR_SIG_VB)) {
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VB PIN");
        gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VB, 1);
    }
    if (pins & BIT(AFE3010_SCR_SIG_VC)) {
        LOG_DBG("Interrupt from AFE3010_SCR_SIG_VC PIN");
        gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VC, 1);
    }

    if (pins & BIT(AFE3010_ALARM_VA)) {
        LOG_DBG("Interrupt from AFE3010_ALARM_VA PIN");
    }
    if (pins & BIT(AFE3010_ALARM_VB)) {
        LOG_DBG("Interrupt from AFE3010_ALARM_VB PIN");
    }
    if (pins & BIT(AFE3010_ALARM_VC)) {
        LOG_DBG("Interrupt from AFE3010_ALARM_VC PIN");
    }
}

void analyze_alarm_pattern(enum PHASES_e phase)
{
    int high_count = 0;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        if (alarm_samples[phase][i]) high_count++;
    }

    bool scr_active = gpio_pin_get(_afe3010_port_gpio_dev, (phase + GROUND_FAULT_SCR_SIG_VA)) == 0;

    // LOG_INF("Phase %s: SCR %s, high_count %d", phase_names[phase], scr_active ? "ACTIVE" : "INACTIVE", high_count);

    if (scr_active) {
        // If SCR is active and high count is high → ground fault
        if (high_count > 2000) {
            LOG_DBG("Detected: Ground fault on phase %s", phase_names[phase]);
            return;
        } else {
            LOG_DBG("Detected: Possible noise during SCR active on phase %s", phase_names[phase]);
            return;
        }
    }

    if (!scr_active) {
        if (high_count < 100) {
            LOG_DBG("Detected: Normal operation on phase %s", phase_names[phase]);
            return;
        } else if (high_count >= 200 && high_count <= 1600) {
            LOG_DBG("Detected: Self-test failure (1Hz blink) on phase %s", phase_names[phase]);
            return;
        }
    }

    LOG_WRN("Detected: Unknown state on phase %s (SCR: %s, high_count: %d)", 
        phase_names[phase], scr_active ? "ACTIVE" : "INACTIVE", high_count);
}

void configure_pin(uint32_t pinType, uint8_t pin, bool isIntPin, uint32_t *intType)
{
    int err = gpio_pin_configure(_afe3010_port_gpio_dev, pin, pinType);
    if(isIntPin)
    {
        err |= gpio_pin_interrupt_configure(_afe3010_port_gpio_dev, pin, *intType);
    }

    if(err)
    {
        LOG_ERR("Error while configuring GPIO %s.%d as %s",
                _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"));
        return;
    }
    
    LOG_DBG("Configured %s.%d as %s",
            _afe3010_port_gpio_dev->name, pin, ((pinType & GPIO_INPUT) ? "INPUT" : "OUTPUT"));
}

void configure_ptt(void)
{
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VA, false, NULL);
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VB, false, NULL);
    configure_pin(GPIO_OUTPUT_HIGH, AFE3010_PTT_VC, false, NULL);
}

void configure_scr_sig(void)
{
    uint32_t intType = GPIO_INT_EDGE_FALLING;
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VA, false, &intType);
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VB, false, &intType);
    configure_pin(GPIO_INPUT, AFE3010_SCR_SIG_VC, false, &intType);
}

void configure_alarm(void)
{
    uint32_t intType = GPIO_INT_EDGE_RISING;
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VA, false, &intType);
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VB, false, &intType);
    configure_pin(GPIO_INPUT, AFE3010_ALARM_VC, false, &intType);
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

    // // Initialize callback on both pins
    // gpio_init_callback(&_afe3010_cb_data, shared_callback,
    //                    SCR_PIN_MASK | ALARM_PIN_MASK );
    // gpio_add_callback(_afe3010_port_gpio_dev, &_afe3010_cb_data);


    // Init and start k_timer
    k_timer_init(&_alarm_pin_sample_timer, alarm_sample_handler, NULL);
    k_timer_start(&_alarm_pin_sample_timer, K_USEC(AFE3010_ALARM_SAMPLE_TIME), K_USEC(AFE3010_ALARM_SAMPLE_TIME));

    // while(1)
    // {
    //     // LOG_INF("Toggling PTT pin");
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 0);
    //     // k_sleep(K_MSEC(10));
    //     // gpio_pin_set(_afe3010_port_gpio_dev, AFE3010_PTT_VA, 1);
        
    //     k_sleep(K_MSEC(5000));
    // }

    LOG_DBG("AFE3010 Ground Fault detection configured");
}