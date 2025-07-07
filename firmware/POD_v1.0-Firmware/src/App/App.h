#pragma once

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

// Hardware nodes
extern const struct device *tempSensor;         // SHTC3 temprature sensor
extern const struct device *fuelGuage;          // MAX17048 fuel gauge ic
extern const struct device *acdcMeter;          // ADS1115 ADC for voltage and ammeter sensing
extern const struct device *imu;                // ADXL345 IMU for impact detection
extern const struct device *powerMeter;         // ADE9000 pwoer processor


void app_init(void);
void app_loop(void);