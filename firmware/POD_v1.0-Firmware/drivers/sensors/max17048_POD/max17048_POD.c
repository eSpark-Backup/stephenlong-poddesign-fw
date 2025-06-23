/* max17048_pod.c - Driver for MAX17048 battery fuel gauge (POD Variant) */
// AUTHOR : Syed Farhan Ahmed

#define DT_DRV_COMPAT maxim_max17048_pod

#include "max17048_POD.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(MAX17048_POD, CONFIG_FUEL_GAUGE_LOG_LEVEL);

struct max17048_pod_data {
    /* Charge as percentage */
	uint8_t charge;
    /* Voltage as uV */
	uint32_t voltage;
    /* Time in minutes */
	uint16_t time_to_full;
	uint16_t time_to_empty;
    /* True if battery charginG, false if discharging */
	bool charging;

	struct gpio_callback alert_cb;
	struct k_work alert_work;
	const struct device *dev;
	// uint16_t last_alert_status;
	union fuel_gauge_prop_val last_alert_status;
};

/**
 * I2C communication
 * The way we read a value is first writing the address we want to read and then
 * wait for 2 bytes containing the data.
 */
int max17048_pod_read_register(const struct device *dev, uint8_t registerId, uint16_t *response)
{
	uint8_t buf[2];
	const struct max17048_pod_config *cfg = dev->config;
	int ret = i2c_write_read_dt(&cfg->i2c, &registerId, sizeof(registerId), buf, sizeof(buf));
    if (ret != 0) {
		LOG_ERR("Unable to read register, error %d", ret);
		return ret;
	}

	*response = sys_get_be16(buf);
	return 0;
}

/**
 * I2C communication
 * The way we send a value is first writing the address we want to write and then
 * send 2 bytes containing the data.
 */
int max17048_pod_write_register(const struct device *dev, uint8_t reg, uint16_t val)
{
	const struct max17048_pod_config *cfg = dev->config;
	uint8_t buf[3] = {reg, val >> 8, val & 0xFF};
	return i2c_write_dt(&cfg->i2c, buf, 3);
}

void max17048_pod_handle_alert(struct k_work *work)
{
	struct max17048_pod_data *data = CONTAINER_OF(work, struct max17048_pod_data, alert_work);
	uint16_t status;
	if (max17048_pod_read_register(data->dev, REGISTER_STATUS, &status) == 0) {
		LOG_INF("MAX17048_POD ALERT: STATUS = 0x%04X", status);
		// data->last_alert_status = status;
		data->last_alert_status.fg_status = status;
        status &= REGISTER_TABLE;  // clear alerts
		max17048_pod_write_register(data->dev, REGISTER_STATUS, status);
	}
}

void max17048_pod_alert_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct max17048_pod_data *data = CONTAINER_OF(cb, struct max17048_pod_data, alert_cb);
	k_work_submit(&data->alert_work);
}

int max17048_pod_get_prop(const struct device *dev, fuel_gauge_prop_t prop, union fuel_gauge_prop_val *val)
{
	struct max17048_pod_data *data = dev->data;
	uint16_t raw;
	int rc;

	if (prop == FUEL_GAUGE_STATUS) {
		*val = data->last_alert_status;
		return 0;
	}

	if (prop == FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE) {
		rc = max17048_pod_read_register(dev, REGISTER_SOC, &raw);
		if (rc == 0) data->last_alert_status.relative_state_of_charge = raw >> 8;
        val->relative_state_of_charge = data->last_alert_status.relative_state_of_charge;
		return rc;
	}
	if (prop == FUEL_GAUGE_VOLTAGE) {
		rc = max17048_pod_read_register(dev, REGISTER_VCELL, &raw);
		if (rc == 0) data->voltage = (uint32_t)(raw >> 4) * 125000;
		// if (rc == 0) data->voltage = (uint32_t)(raw) * 78.125;
        val->voltage = data->voltage;
		return rc;
	}
	if (prop == FUEL_GAUGE_VOLTAGE) {
		rc = max17048_pod_read_register(dev, REGISTER_CRATE, &raw);
		if (rc == 0)
        {
            uint16_t crate = raw * 208;
            if (crate != 0)
            {
                data->charging = crate > 0;
                if (data->charging) {
                    uint8_t percentage_pending = 100 - data->charge;
                    uint32_t hours_pending = percentage_pending * 1000000 / crate;

                    data->time_to_empty = 0;
                    data->time_to_full = hours_pending * 60 / 1000;
                } else {
                    /* Discharging */
                    uint32_t hours_pending = data->charge * 1000000 / -crate;

                    data->time_to_empty = hours_pending * 60 / 1000;
                    data->time_to_full = 0;
                }
            }
            else
            {
                data->charging = false;
                data->time_to_full = 0;
                data->time_to_empty = 0;
            }
        }
		return rc;
	}
	return -ENOTSUP;
}

int max17048_pod_set_prop(const struct device *dev, fuel_gauge_prop_t prop, union fuel_gauge_prop_val *val)
{
	struct max17048_pod_data *data = dev->data;
	uint16_t raw;
	int rc;

    if (prop == FUEL_GAUGE_BATTERY_CUTOFF) {
    // hypothetical: write to config register to enable cutoff
    return max17048_pod_write_register(dev, 0x0C, val->cutoff ? 0x0001 : 0x0000);
	}
	if (prop == FUEL_GAUGE_STATUS) {
		return 0;
	}
	if (prop == FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE) {
		return rc;
	}
	if (prop == FUEL_GAUGE_VOLTAGE) {
		return rc;
	}
	if (prop == FUEL_GAUGE_VOLTAGE) {
		return rc;
	}
	return -ENOTSUP;
}

int max17048_pod_init(const struct device *dev)
{
	const struct max17048_pod_config *cfg = dev->config;
	struct max17048_pod_data *data = dev->data;

	data->dev = dev;
	k_work_init(&data->alert_work, max17048_pod_handle_alert);

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}
	if (!device_is_ready(cfg->alrt_gpio.port)) {
		LOG_ERR("ALRT GPIO not ready");
		return -ENODEV;
	}
	gpio_pin_configure_dt(&cfg->alrt_gpio, GPIO_INPUT);
	gpio_init_callback(&data->alert_cb, max17048_pod_alert_isr, BIT(cfg->alrt_gpio.pin));
	gpio_add_callback(cfg->alrt_gpio.port, &data->alert_cb);
	gpio_pin_interrupt_configure_dt(&cfg->alrt_gpio, GPIO_INT_EDGE_FALLING);

	uint16_t version;
	uint16_t status;
	int rc = max17048_pod_read_register(dev, REGISTER_VERSION, &version);
	if (rc < 0 || (version & 0xFFF0) != 0x0010) {
		LOG_ERR("Invalid MAX17048 version: 0x%04X", version);
		return -ENODEV;
	}
	rc = max17048_pod_read_register(dev, REGISTER_STATUS, &status);
	if (status & 0x0001) {
		LOG_ERR("IC Reset flag set: 0x%04X", status);
		LOG_ERR("Initializing model");
		max17048_pod_write_register(dev, REGISTER_MODE, QUICKSTART_MODE);
	}
	return 0;
}

static const struct fuel_gauge_driver_api max17048_pod_driver_api = {
	.get_property = max17048_pod_get_prop,
	.set_property = max17048_pod_set_prop,
// 	.get_buffer_property = NULL,
// 	.battery_cutoff = NULL,
};

#define MAX17048_POD_DEFINE(inst) \
	static struct max17048_pod_data max17048_pod_data_##inst; \
	static const struct max17048_pod_config max17048_pod_config_##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst), \
		.alrt_gpio = GPIO_DT_SPEC_INST_GET(inst, alrt_gpios), \
	}; \
	DEVICE_DT_INST_DEFINE(inst, max17048_pod_init, NULL, \
			&max17048_pod_data_##inst, \
			&max17048_pod_config_##inst, \
			POST_KERNEL, CONFIG_FUEL_GAUGE_INIT_PRIORITY, \
			&max17048_pod_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAX17048_POD_DEFINE)

