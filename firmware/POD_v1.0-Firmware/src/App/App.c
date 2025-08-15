#include <defines.h>
#include "App.h"
#include "shtc6.h"
#include "max17048.h"
#include "ads1115.h"
#include "adxl345.h"
#include "ade9000.h"
#include "afe3010.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

// #include "drivers/sensor/ADE_9000_POD/ade9000.h"

LOG_MODULE_REGISTER(APP, APP_LOG_LEVEL);

const struct device *tempSensor = DEVICE_DT_GET_ANY(sensirion_shtcx);
const struct device *fuelGuage = DEVICE_DT_GET_ANY(maxim_max17048);
const struct device *acdcMeter = DEVICE_DT_GET_ANY(ti_ads1115);
const struct device *imu = DEVICE_DT_GET_ANY(adi_adxl345);
const struct device *powerMeter = DEVICE_DT_GET(DT_NODELABEL(spi1));
// const struct device *powerMeter = DEVICE_DT_GET(DT_BUS(DT_NODELABEL(ade9000)));
// const struct device *powerMeter = DEVICE_DT_GET(DT_NODELABEL(ade9000));
// const struct device *powerMeter = DEVICE_DT_GET_ANY(adi_ade9000);


void app_init(void)
{
	LOG_DBG("Initializing sensor drivers");
	
	init_ade9000(powerMeter);
	init_adxl345(imu);
	init_ads1115(acdcMeter);
	init_shtc6(tempSensor);
	init_max17048(fuelGuage);
	init_afe3010();

	LOG_DBG("Initialization complete");

	while (1)
	{
		int64_t now = k_uptime_get();

		double temp_c = get_temp();
		double hum_perc = get_humidity();
		int soc = get_batt_soc();
		int batt_volt = get_batt_voltage();

        // Extract integer and fractional parts
		int temp_int = (int)temp_c;
		int temp_frac = (int)((temp_c - temp_int) * 100);

		int hum_int = (int)hum_perc;
		int hum_frac = (int)((hum_perc - hum_int) * 100);

		int voltage = batt_volt;
		int volts = voltage / 1000000;
		int millivolts = (voltage % 1000000) / 10000;  // Get 2 decimal digits


		float data[7] = {0.0};
    	float vRMS[3], activPOW[3], period[3], phase[3] = {0.0};
		CurrentADC adc;
        // ADE9000_readRawMeasurement(&adc);
        // ADE9000_convertRawMeasurement(&adc, data);
        ADE9000_readCurrentRMS(data);

        struct VoltageRMSRegs voltRMS;
        struct ActivePowerRegs activePOW;
        ADE9000_readVoltageRMSRegs(&voltRMS);
        ADE9000_convertVoltageRMSRegs(&voltRMS, vRMS);
        ADE9000_readActivePowerRegs(&activePOW);
        ADE9000_convertActivePowerRegs(&activePOW, activPOW);
        
        ADE9000_readLinePeriod(period);
        ADE9000_readPhaseAngle(phase);

		analyze_ground_faults((uint32_t)now);

        static int64_t last_log = 0;
        
        if(now - last_log >= 1000)
        {
            last_log = now;
			LOG_INF("Temperature: %d.%02d °C", temp_int, temp_frac);
			LOG_INF("Humidity: %d.%02d %%", hum_int, hum_frac);
			LOG_INF("DC Voltage: %2.3f V", (double)get_dc_voltage());
			LOG_INF("DC Current: %2.3f A", (double)get_dc_current());

			// LOG_INF("Time to empty %d minutes", vals[0].runtime_to_empty);
			// LOG_INF("Time to full %d minutes", vals[1].runtime_to_full);
			LOG_INF("Charge %d%%", soc);//vals[2].relative_state_of_charge);
			LOG_INF("Voltage %d.%02d V", volts, millivolts);

            LOG_INF("Power Data");
            // LOG_INF("V - L1: %.2fV\tL2: %.2fV\tL3: %.2fV", (double)data[0], (double)data[2], (double)data[4]);
            // LOG_INF("I - L1: %.2fmA\tL2: %.2fmA\tL3: %.2fmA\tN: %.2fmA", (double)data[1], (double)data[3], (double)data[5], (double)data[6]);
            LOG_INF("I - L1: %.2fmA\tL2: %.2fmA\tL3: %.2fmA\tN: %.2fmA", (double)data[0], (double)data[1], (double)data[2], (double)data[3]);
            LOG_INF("vRMS - L1: %.2fV(%.2fHz)(%.2f°)\tL2: %.2fV(%.2fHz)(%.2f°)\tL3: %.2fV(%.2fHz)(%.2f°)", (double)vRMS[0], (double)period[0], (double)phase[0], (double)vRMS[1], (double)period[0], (double)phase[0], (double)vRMS[2], (double)period[0], (double)phase[0]);
            LOG_INF("activPOW - L1: %.2f Watt\tL2: %.2f Watt\tL3: %.2f Watt\n", (double)activPOW[0], (double)activPOW[1], (double)activPOW[2]);
        }

        k_sleep(K_MSEC(1));
    }

    return;
}
