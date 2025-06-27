#include "defines.h"
#include "App.h"
#include "shtc6.h"
#include "max17048.h"
#include "ads1115.h"
#include "adxl345.h"

// #include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(APP, APP_LOG_LEVEL);

const struct device *tempSensor = DEVICE_DT_GET_ANY(sensirion_shtcx);
const struct device *fuelGuage = DEVICE_DT_GET_ANY(maxim_max17048);
const struct device *acdcMeter = DEVICE_DT_GET_ANY(ti_ads1115);
const struct device *imu = DEVICE_DT_GET_ANY(adi_adxl345);
// const struct device *powerMeter = NULL;


void app_init(void)
{
	init_adxl345(imu);
	// init_ads1115(acdcMeter);
	// init_shtc6(tempSensor);
	// init_max17048(fuelGuage);

	// while (1) {

	// 	double temp_c = get_temp();
	// 	double hum_perc = get_humidity();
	// 	int soc = get_batt_soc();
	// 	int batt_volt = get_batt_voltage();

    //     // Extract integer and fractional parts
	// 	int temp_int = (int)temp_c;
	// 	int temp_frac = (int)((temp_c - temp_int) * 100);

	// 	int hum_int = (int)hum_perc;
	// 	int hum_frac = (int)((hum_perc - hum_int) * 100);

	// 	int voltage = batt_volt;
	// 	int volts = voltage / 1000000;
	// 	int millivolts = (voltage % 1000000) / 10000;  // Get 2 decimal digits

	// 	LOG_INF("Temperature: %d.%02d °C", temp_int, temp_frac);
	// 	LOG_INF("Humidity: %d.%02d %%", hum_int, hum_frac);
	// 	LOG_INF("DC Voltage: %2.3f V", get_dc_voltage());
	// 	LOG_INF("DC Current: %2.3f A", get_dc_current());

	// 	// LOG_INF("Time to empty %d minutes", vals[0].runtime_to_empty);
	// 	// LOG_INF("Time to full %d minutes", vals[1].runtime_to_full);
	// 	LOG_INF("Charge %d%%", soc);//vals[2].relative_state_of_charge);
	// 	LOG_INF("Voltage %d.%02d V", volts, millivolts);

    //     k_sleep(K_SECONDS(1));
    // }

    return;
}
