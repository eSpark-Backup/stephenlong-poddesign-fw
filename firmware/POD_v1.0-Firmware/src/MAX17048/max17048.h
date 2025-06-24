#pragma once

#include "defines.h"

// #include <zephyr/device.h>
// #include <zephyr/devicetree.h>

// #define MAX17048_NODE DT_NODELABEL(max17048)

extern const struct device *_max17048;;
extern const struct device *alert_gpio_dev;
extern struct gpio_callback alert_cb_data;

#define MAX17048_ADDR       FUEL_GAUGE_IC_ADDR
#define MAX17048_REG_VALRT  0x14
#define MAX17048_REG_STATUS 0x1A
#define MAX17048_REG_CONFIG 0x0C

#define ALERT_PIN           FUEL_GAUGE_GPIO
#define ALERT_PORT          FUEL_GAUGE_GPIO_PORT

#define VALRT_1LSB_THRESH               20.0               // 1LSB = 20mV
#define MAX17048_MIN_VOLT_THRESH        FUEL_GUAGE_MIN_THRESH
#define MAX17048_MAX_VOLT_THRESH        FUEL_GUAGE_MAX_THRESH

void max17048_alert_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void configure_max17048_alert_pin(void);
int  max17048_set_valrt(float min, float max);

void init_max17048(const struct device* max17048);
