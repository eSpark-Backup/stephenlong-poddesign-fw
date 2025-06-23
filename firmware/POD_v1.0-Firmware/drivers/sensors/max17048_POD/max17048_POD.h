/*
 * Copyright (c) 2023 Alvaro Garcia Gomez <maxpowel@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef POD_DRIVERS_SENSOR_MAX17048_MAX17048_H_
#define POD_DRIVERS_SENSOR_MAX17048_MAX17048_H_

#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#define REGISTER_VCELL      0x02
#define REGISTER_SOC        0x04
#define REGISTER_MODE       0x06
#define REGISTER_VERSION    0x08
#define REGISTER_HIBRT      0x0A
#define REGISTER_CONFIG     0x0C
#define REGISTER_VALRT      0x14
#define REGISTER_CRATE      0x16
#define REGISTER_VRESET     0x18
#define REGISTER_CHIP_ID    0x19
#define REGISTER_STATUS     0x1A
#define REGISTER_TABLE      0x40
#define REGISTER_COMMAND    0xFE

#define RESET_COMMAND       0x5400
#define QUICKSTART_MODE     0x4000

struct max17048_pod_config {
	struct i2c_dt_spec i2c;
    struct gpio_dt_spec alrt_gpio;
};

int max17048_pod_read_register(const struct device *dev, uint8_t registerId, uint16_t *response);
int max17048_pod_write_register(const struct device *dev, uint8_t reg, uint16_t val);
void max17048_pod_handle_alert(struct k_work *work);
void max17048_pod_alert_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins);
int max17048_pod_get_prop(const struct device *dev, fuel_gauge_prop_t prop, union fuel_gauge_prop_val *val);
int max17048_pod_set_prop(const struct device *dev, fuel_gauge_prop_t prop, union fuel_gauge_prop_val *val);
int max17048_pod_init(const struct device *dev);

#endif /* ZEPHYR_DRIVERS_SENSOR_MAX17048_MAX17048_H_ */
