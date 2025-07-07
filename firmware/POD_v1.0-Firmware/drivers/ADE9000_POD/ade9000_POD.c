// ade9000_POD.c - Driver for ADE9000 power meter over SPI
// AUTHOR: Syed Farhan Ahmed

#define DT_DRV_COMPAT adi_ade9000

#include <drivers/ADE9000/ade9000_POD.h>
// #include "ade9000_POD.h"
// #include <ade9000_POD.h>

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ade9000, CONFIG_ADE9000_LOG_LEVEL);

static int ade9000_pod_read_register(const struct device *dev, uint32_t reg, uint32_t *val)
{
	const struct ade9000_pod_config *cfg = dev->config;
	if (!device_is_ready(cfg->spi.bus)) {
		return -ENODEV;
	}

	gpio_pin_set_dt(&cfg->cs_gpio, 0);

	uint8_t tx[7] = {
		(uint8_t)((reg >> 16) | 0x80),
		(uint8_t)(reg >> 8),
		(uint8_t)(reg),
		0, 0, 0, 0
	};
	uint8_t rx[7] = {0};

	struct spi_buf tx_buf = {.buf = tx, .len = sizeof(tx)};
	struct spi_buf rx_buf = {.buf = rx, .len = sizeof(rx)};
	struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
	struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};

	int ret = spi_transceive_dt(&cfg->spi, &tx_set, &rx_set);

	gpio_pin_set_dt(&cfg->cs_gpio, 1);

	if (ret == 0) {
		*val = ((uint32_t)rx[3] << 24) | ((uint32_t)rx[4] << 16) |
		       ((uint32_t)rx[5] << 8) | rx[6];
	}
	return ret;
}

static int ade9000_pod_write_register(const struct device *dev, uint32_t reg, uint32_t val)
{
	const struct ade9000_pod_config *cfg = dev->config;
	if (!device_is_ready(cfg->spi.bus)) {
		return -ENODEV;
	}

	gpio_pin_set_dt(&cfg->cs_gpio, 0);

	uint8_t tx[7] = {
		(uint8_t)((reg >> 16) & 0x7F),
		(uint8_t)(reg >> 8),
		(uint8_t)(reg),
		(uint8_t)(val >> 24),
		(uint8_t)(val >> 16),
		(uint8_t)(val >> 8),
		(uint8_t)(val)
	};
	struct spi_buf tx_buf = {.buf = tx, .len = sizeof(tx)};
	struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
	int ret = spi_write_dt(&cfg->spi, &tx_set);

	gpio_pin_set_dt(&cfg->cs_gpio, 1);

	return ret;
}

static int ade9000_get_rms_voltage(const struct device *dev, uint32_t *val)
{
	return ade9000_pod_read_register(dev, ADE9000_REG_AVRMS, val);
}

static int ade9000_get_rms_current(const struct device *dev, uint32_t *val)
{
	return ade9000_pod_read_register(dev, ADE9000_REG_AIRMS, val);
}

static int ade9000_get_active_power(const struct device *dev, int32_t *val)
{
	uint32_t raw;
	int ret = ade9000_pod_read_register(dev, ADE9000_REG_AWATT, &raw);
	if (ret == 0) {
		*val = (int32_t)raw;
	}
	return ret;
}

static int ade9000_pod_init(const struct device *dev)
{
	const struct ade9000_pod_config *cfg = dev->config;

	if (!device_is_ready(cfg->spi.bus)) {
		LOG_ERR("SPI bus not ready");
		return -ENODEV;
	}

	if (!device_is_ready(cfg->cs_gpio.port)) {
		LOG_ERR("CS GPIO not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&cfg->cs_gpio, GPIO_OUTPUT_HIGH);

	return 0;
}

static const struct ade9000_driver_api ade9000_pod_driver_api = {
// static DEVICE_API(ade9000, ade9000_pod_driver_api) = {
	.read_register = ade9000_pod_read_register,
	.write_register = ade9000_pod_write_register,
	.get_rms_voltage = ade9000_get_rms_voltage,
	.get_rms_current = ade9000_get_rms_current,
	.get_active_power = ade9000_get_active_power,
};

#define ADE9000_NODE DT_NODELABEL(ade9000)

#define ADE9000_POD_DEFINE(inst) \
	// static struct ade9000_pod_data ade9000_pod_data_##inst; \
	// static const struct ade9000_pod_config ade9000_pod_config_##inst = { \
	// 	.spi = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA, 0), \
	// 	.cs_gpio = GPIO_DT_SPEC_INST_GET(inst, cs_gpios), \
	// }; 
	static struct ade9000_pod_data ade9000_pod_data; \
	static const struct ade9000_pod_config ade9000_pod_config = { \
		.spi = SPI_DT_SPEC_GET(ADE9000_NODE, SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA, 0), \
		.cs_gpio = GPIO_DT_SPEC_GET(ADE9000_NODE, cs_gpios), \
	}; \
	// DEVICE_DT_INST_DEFINE(inst, ade9000_pod_init, NULL, \
	// 		&ade9000_pod_data_##inst, &ade9000_pod_config_##inst, \
	// 		POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
	// 		&ade9000_pod_driver_api);
	DEVICE_DT_DEFINE(ADE9000_NODE, ade9000_pod_init, NULL, \
			&ade9000_pod_data, &ade9000_pod_config, \
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			&ade9000_pod_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADE9000_POD_DEFINE)
