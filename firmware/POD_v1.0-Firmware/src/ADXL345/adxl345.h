#pragma once

#include "defines.h"

extern const struct device *_adxl345;
extern const struct device *_adxl345_port_gpio_dev;
extern struct gpio_callback _adxl345_alert_cb_data;

#define ACCEL_PORT              ACCEL_GPIO_PORT
#define ACCEL_INT_PIN           ACCEL_INT
#define ACCEL_CS_PIN            ACCEL_CS        // not required here, if pulled to VDD adxl is setup in i2c mode

#define ADXL345_INT_SRC_FF_MSK  BIT(2)
#define ADXL345_INT_SRC_ACT_MSK BIT(4)
#define ADXL345_INT_SRC_DR_MSK  BIT(7)

#define ADXL345_FF_THRESH_REG   0X28
#define ADXL345_FF_TIME_REG     0X29

#define ADXL345_THRESH_ACT_REG  0x24u
#define ADXL345_ACT_CTL_REG     0x27u
#define ADXL345_INT_ENABLE_REG  0x2Eu
#define ADXL345_INT_MAP_REG     0x2Fu
#define ADXL345_INT_SOURCE_REG  0x30u
#define ADXL345_DATA_FORMAT_REG 0x31
#define ADXL345_FIFO_CTL_REG    0x38            // maybe not required?

void adxl345_trigger_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

void verify_settings(void);
void configure_adxl345_alert_pin(void);

void init_adxl345(const struct device* adxl345);