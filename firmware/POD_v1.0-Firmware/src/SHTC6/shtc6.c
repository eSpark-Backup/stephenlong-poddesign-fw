#include "shtc6.h"

// #include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(SHTC6_POD, ADS1115_POD_LOG_LEVEL);

const struct device *_shtc6;

static struct k_timer _shtc6_timer;
static struct k_work _shtc6_worker;

void shtc6_work_handler(struct k_work *work)
{
    LOG_DBG("Worker triggerred.");

    int ret = sensor_sample_fetch(_shtc6);
    if (ret < 0) {
        LOG_ERR("Failed to fetch data from SHTCx sensor: %d", ret);
    }
}

void shtc6_timer_interrupt(struct k_timer *timer)
{
    k_work_submit(&_shtc6_worker);
}

double get_temp(void)
{
    struct sensor_value temp;
    sensor_channel_get(_shtc6, SENSOR_CHAN_AMBIENT_TEMP, &temp);

    return sensor_value_to_double(&temp);
}

double get_humidity(void)
{
    struct sensor_value humidity;
    sensor_channel_get(_shtc6, SENSOR_CHAN_HUMIDITY, &humidity);

    return sensor_value_to_double(&humidity);
}

void init_shtc6(const struct device* shtc6)
{
    _shtc6 = shtc6;

    if ((_shtc6 == NULL)) {
		LOG_ERR("I2C device not found.");
		return;
	}

	if (!device_is_ready(_shtc6)) {
        LOG_ERR("SHTCx device not ready");
        return;
    }

    k_work_init(&_shtc6_worker, shtc6_work_handler);
    k_work_submit(&_shtc6_worker);

    k_timer_init(&_shtc6_timer, shtc6_timer_interrupt, NULL);
    k_timer_start(&_shtc6_timer, K_SECONDS(5), K_SECONDS(5));

    LOG_DBG("SHTCx sensor initialized");
}
