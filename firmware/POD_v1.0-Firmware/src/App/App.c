#include "App.h"
#include "max17048.h"
#include "defines.h"
// #include "../MAX17048/max17048.h"
// #include "../../src/defines.h"

// #include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(APP, APP_LOG_LEVEL);

const struct device *tempSensor = DEVICE_DT_GET_ANY(sensirion_shtcx);
const struct device *fuelGuage = DEVICE_DT_GET_ANY(maxim_max17048);
// const struct device *acdcMeter = NULL;
// const struct device *imu = NULL;
// const struct device *powerMeter = NULL;


void app_init(void)
{
    if ((tempSensor == NULL) || (fuelGuage == NULL)) {
		LOG_ERR("I2C device not found.");
		return;
	}

	if (!device_is_ready(tempSensor)) {
        LOG_ERR("SHTCx device not ready");
        return;
    }
	if (!device_is_ready(fuelGuage)) {
        LOG_ERR("MAX17048 device not ready");
        return;
    }

	// const struct i2c_dt_spec *cfg = fuelGuage->config;
    // LOG_ERR("cfg.bus: %d, cfg.addr: 0%X", cfg->bus, cfg->addr);

	// Setup VALRT thresholds and interrupt
	init_max17048(fuelGuage);
    // max17048_set_valrt();
	// configure_max17048_alert_pin();

    LOG_DBG("SHTCx sensor initialized");
    LOG_DBG("MAX17048 sensor initialized");

    struct sensor_value temp, humidity;
	fuel_gauge_prop_t props[] = {
			FUEL_GAUGE_RUNTIME_TO_EMPTY,
			FUEL_GAUGE_RUNTIME_TO_FULL,
			FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
			FUEL_GAUGE_VOLTAGE,
		};
	union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];

	while (1) {
        // int ret = sensor_sample_fetch(tempSensor);
        // if (ret < 0) {
        //     LOG_ERR("Failed to fetch data from SHTCx sensor: %d", ret);
        //     k_sleep(K_SECONDS(2));
        //     continue;
        // }
		// ret = fuel_gauge_get_props(fuelGuage, props, vals, ARRAY_SIZE(props));
		// if (ret < 0) {
		// 	LOG_ERR("Failed to fetch data from MAX17048 sensor: %d", ret);
		// 	k_sleep(K_SECONDS(2));
		// 	continue;
		// }

        // sensor_channel_get(tempSensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        // sensor_channel_get(tempSensor, SENSOR_CHAN_HUMIDITY, &humidity);

		// double temp_c = sensor_value_to_double(&temp);
		// double hum_perc = sensor_value_to_double(&humidity);

        // // Extract integer and fractional parts
		// int temp_int = (int)temp_c;
		// int temp_frac = (int)((temp_c - temp_int) * 100);

		// int hum_int = (int)hum_perc;
		// int hum_frac = (int)((hum_perc - hum_int) * 100);

		// int voltage = vals[3].voltage;
		// int volts = voltage / 1000000;
		// int millivolts = (voltage % 1000000) / 10000;  // Get 2 decimal digits

		// LOG_INF("Temperature: %d.%02d °C", temp_int, temp_frac);
		// LOG_INF("Humidity: %d.%02d %%", hum_int, hum_frac);

		// LOG_INF("Time to empty %d minutes", vals[0].runtime_to_empty);
		// LOG_INF("Time to full %d minutes", vals[1].runtime_to_full);
		// LOG_INF("Charge %d%%", vals[2].relative_state_of_charge);
		// LOG_INF("Voltage %d.%02d V", volts, millivolts);

        k_sleep(K_SECONDS(5));
    }

    return;
}
