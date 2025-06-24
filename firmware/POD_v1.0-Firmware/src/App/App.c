#include "defines.h"
#include "App.h"
#include "shtc6.h"
#include "max17048.h"
#include "ads1115.h"

// #include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(APP, APP_LOG_LEVEL);

const struct device *tempSensor = DEVICE_DT_GET_ANY(sensirion_shtcx);
const struct device *fuelGuage = DEVICE_DT_GET_ANY(maxim_max17048);
// const struct device *acdcMeter = DEVICE_DT_GET_ANY(ti_ads1115);
// const struct device *imu = NULL;
// const struct device *powerMeter = NULL;


void app_init(void)
{
	init_shtc6(tempSensor);
	init_max17048(fuelGuage);
	// init_ads1115(acdcMeter);


    // struct sensor_value temp, humidity;
	// fuel_gauge_prop_t props[] = {
	// 		FUEL_GAUGE_RUNTIME_TO_EMPTY,
	// 		FUEL_GAUGE_RUNTIME_TO_FULL,
	// 		FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
	// 		FUEL_GAUGE_VOLTAGE,
	// 	};
	// union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];

	while (1) {
		// int ret = fuel_gauge_get_props(fuelGuage, props, vals, ARRAY_SIZE(props));
		// if (ret < 0) {
		// 	LOG_ERR("Failed to fetch data from MAX17048 sensor: %d", ret);
		// 	k_sleep(K_SECONDS(2));
		// 	continue;
		// }

		double temp_c = get_temp();
		double hum_perc = get_humidity();

        // Extract integer and fractional parts
		int temp_int = (int)temp_c;
		int temp_frac = (int)((temp_c - temp_int) * 100);

		int hum_int = (int)hum_perc;
		int hum_frac = (int)((hum_perc - hum_int) * 100);

		// int voltage = vals[3].voltage;
		// int volts = voltage / 1000000;
		// int millivolts = (voltage % 1000000) / 10000;  // Get 2 decimal digits

		LOG_INF("Temperature: %d.%02d °C", temp_int, temp_frac);
		LOG_INF("Humidity: %d.%02d %%", hum_int, hum_frac);

		// LOG_INF("Time to empty %d minutes", vals[0].runtime_to_empty);
		// LOG_INF("Time to full %d minutes", vals[1].runtime_to_full);
		// LOG_INF("Charge %d%%", vals[2].relative_state_of_charge);
		// LOG_INF("Voltage %d.%02d V", volts, millivolts);

        k_sleep(K_SECONDS(5));
    }

    return;
}
