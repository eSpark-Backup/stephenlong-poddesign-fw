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
const struct device *acdcMeter = DEVICE_DT_GET_ANY(ti_ads1115);
// const struct device *imu = NULL;
// const struct device *powerMeter = NULL;


void app_init(void)
{
	init_ads1115(acdcMeter);
	init_shtc6(tempSensor);
	init_max17048(fuelGuage);

	while (1) {

		double temp_c = get_temp();
		double hum_perc = get_humidity();
		double current = get_dc_current();
		int soc = get_batt_soc();
		int batt_volt = get_batt_voltage();

        // Extract integer and fractional parts
		int temp_int = (int)temp_c;
		int temp_frac = (int)((temp_c - temp_int) * 100);

		int hum_int = (int)hum_perc;
		int hum_frac = (int)((hum_perc - hum_int) * 100);

		int hum_current_int = (int)current;
		int hum_current_frac = (int)((current - hum_current_int) * 100);

		int voltage = batt_volt;
		int volts = voltage / 1000000;
		int millivolts = (voltage % 1000000) / 10000;  // Get 2 decimal digits

		LOG_INF("Temperature: %d.%02d °C", temp_int, temp_frac);
		LOG_INF("Humidity: %d.%02d %%", hum_int, hum_frac);
		LOG_INF("DC Voltage: %d mV", get_dc_voltage());
		// LOG_INF("DC Current: %d mA", get_dc_current());
		LOG_INF("DC Current: %d.%02dA", hum_current_int, hum_current_frac);

		// LOG_INF("Time to empty %d minutes", vals[0].runtime_to_empty);
		// LOG_INF("Time to full %d minutes", vals[1].runtime_to_full);
		LOG_INF("Charge %d%%", soc);//vals[2].relative_state_of_charge);
		LOG_INF("Voltage %d.%02d V", volts, millivolts);

        k_sleep(K_SECONDS(5));
    }

    return;
}
