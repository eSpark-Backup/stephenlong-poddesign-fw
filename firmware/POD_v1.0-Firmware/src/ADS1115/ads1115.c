#include "ads1115.h"

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>

LOG_MODULE_REGISTER(ADS1115_POD, ADS1115_POD_LOG_LEVEL);

const struct device *_ads1115 = NULL;

static struct k_timer _ads1115_timer;
static struct k_work _ads1115_worker;

struct adc_channel_cfg ch_cfg[NUM_CHANNELS];
struct adc_sequence sequence;
int16_t resultsRaw[NUM_CHANNELS];

void read_adc_values(void);
void configure_adc_channels(uint8_t i);


void ads1115_work_handler(struct k_work *work)
{
    LOG_DBG("Worker triggerred.");

    read_adc_values();
}

void ads1115_timer_interrupt(struct k_timer *timer)
{
    k_work_submit(&_ads1115_worker);
}

void configure_adc_channels(uint8_t i)
{
    ch_cfg[i].gain = ADS1115_GAIN;
    ch_cfg[i].reference = ADC_REF_INTERNAL;
    ch_cfg[i].differential = 0;
    ch_cfg[i].input_positive = i;

    if (adc_channel_setup(_ads1115, &ch_cfg[i]) != 0) {
        LOG_ERR("Error setting up channel %d", i);
    }
}

void read_adc_values(void)
{
    sequence.channels = BIT(0);// | BIT(1) | BIT(2) | BIT(3);
    sequence.buffer_size = sizeof(int16_t);
    sequence.resolution = ADC_RESOLUTION-1;
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        sequence.buffer = &resultsRaw[i];
        configure_adc_channels(i);
        if (adc_read(_ads1115, &sequence) == 0) {
            LOG_DBG("Channel %d: %d", i, resultsRaw[i]);
        } else {
            LOG_ERR("ADC read failed");
        }
    }
}

float get_dc_voltage(void)
{
    uint16_t mVolts = (resultsRaw[DC_VOLTAGE_CHANNEL] * ADS1115_LEAST_COUNT);
    float dc_mVolts= map_value(mVolts, VOLTAGE_RANGE_MIN, 3.3, VOLTAGE_RANGE_MIN, VOLTAGE_RANGE_MAX);

    // return (uint16_t)dc_mVolts;
    return dc_mVolts/1000.0f;
}

float get_dc_current(void)
{
    float mVolts = (resultsRaw[DC_CURRENT_CHANNEL] * ADS1115_LEAST_COUNT);
    mVolts /= 1000;
    mVolts -= (HALLCURRENT8120A_QUIESCENT_VOLTAGE + 0.0042f);
    float dc_mAmps = mVolts/0.01f;//map_value(mVolts, CURRENT_RANGE_MIN, 3.3, CURRENT_RANGE_MIN, CURRENT_RANGE_MAX);

    return dc_mAmps;
    // return (uint16_t)dc_mAmps;
}

void init_ads1115(const struct device* ads1115)
{
    _ads1115 = ads1115;

    if ((_ads1115 == NULL)) {
		LOG_ERR("I2C device not found. _ads1115: %p", _ads1115);
		return;
	}

	if (!device_is_ready(_ads1115)) {
        LOG_ERR("ADS1115 device not ready");
        return;
    }

    k_work_init(&_ads1115_worker, ads1115_work_handler);
    k_work_submit(&_ads1115_worker);

    k_timer_init(&_ads1115_timer, ads1115_timer_interrupt, NULL);
    k_timer_start(&_ads1115_timer, K_SECONDS(5), K_SECONDS(1));

    LOG_DBG("ADS1115 sensor initialized");
}
