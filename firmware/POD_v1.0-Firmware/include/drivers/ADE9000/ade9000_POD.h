// ade9000_POD.h - Header for ADE9000 power meter driver
// AUTHOR: Syed Farhan Ahmed

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/toolchain.h>

#include "ade9000_register.h"

// #define DT_DRV_COMPAT adi_ade9000

#ifdef __cplusplus
extern "C" {
#endif

// Example ADE9000 register addresses
#define ADE9000_REG_AVRMS   0x2010
#define ADE9000_REG_AIRMS   0x2012
#define ADE9000_REG_AWATT   0x2000
// Add more as needed...

struct ade9000_pod_config {
	struct spi_dt_spec spi;
	struct gpio_dt_spec cs_gpio;
};

struct ade9000_pod_data {
	const struct device *dev;
};

// Custom driver API
__subsystem struct ade9000_driver_api {
	int (*read_register)(const struct device *dev, uint32_t reg, uint32_t *val);
	int (*write_register)(const struct device *dev, uint32_t reg, uint32_t val);
	int (*get_rms_voltage)(const struct device *dev, uint32_t *val);
	int (*get_rms_current)(const struct device *dev, uint32_t *val);
	int (*get_active_power)(const struct device *dev, int32_t *val);
};


// __syscall int ade9000_read_register(const struct device *dev, uint32_t reg, uint32_t *val);
// static inline int z_impl_ade9000_read_register(const struct device *dev, uint32_t reg, uint32_t *val)
// {
// 	// __ASSERT_NO_MSG(DEVICE_API_IS(ade9000, dev));

// 	// return DEVICE_API_GET(ade9000, dev)->read_register(dev, reg, val);

// 	// const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
// 	// return api->read_register(dev, reg, val);
// }

// __syscall int ade9000_write_register(const struct device *dev, uint32_t reg, uint32_t val);
// static inline int z_impl_ade9000_write_register(const struct device *dev, uint32_t reg, uint32_t val)
// {
// 	// __ASSERT_NO_MSG(DEVICE_API_IS(ade9000, dev));

// 	// return DEVICE_API_GET(ade9000, dev)->write_register(dev, reg, val);

// 	// const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
// 	// return api->write_register(dev, reg, val);
// }

// __syscall int ade9000_get_rms_voltage(const struct device *dev, uint32_t *val);
// static inline int z_impl_ade9000_get_rms_voltage(const struct device *dev, uint32_t *val)
// {
// 	// __ASSERT_NO_MSG(DEVICE_API_IS(ade9000, dev));

// 	// return DEVICE_API_GET(ade9000, dev)->get_rms_voltage(dev, val);

// 	// const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
// 	// return api->get_rms_voltage(dev, val);
// }

// __syscall int ade9000_get_rms_current(const struct device *dev, uint32_t *val);
// static inline int z_impl_ade9000_get_rms_current(const struct device *dev, uint32_t *val)
// {
// 	// __ASSERT_NO_MSG(DEVICE_API_IS(ade9000, dev));

// 	// return DEVICE_API_GET(ade9000, dev)->get_rms_current(dev, val);

// 	// const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
// 	// return api->get_rms_current(dev, val);
// }

// __syscall int ade9000_get_active_power(const struct device *dev, uint32_t *val);
// static inline int z_impl_ade9000_get_active_power(const struct device *dev, uint32_t *val)
// {
// 	// __ASSERT_NO_MSG(DEVICE_API_IS(ade9000, dev));

// 	// return DEVICE_API_GET(ade9000, dev)->get_active_power(dev, val);
	
// 	// const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
// 	// return api->get_active_power(dev, val);
// }

// Public API access macros
static inline int ade9000_read_register_api(const struct device *dev, uint32_t reg, uint32_t *val)
{
	const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
	return api->read_register(dev, reg, val);
	// return ade9000_read_register(dev, reg, val);
}

static inline int ade9000_write_register_api(const struct device *dev, uint32_t reg, uint32_t val)
{
	const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
	return api->write_register(dev, reg, val);
	// return ade9000_write_register(dev, reg, val);
}

static inline int ade9000_get_rms_voltage_api(const struct device *dev, uint32_t *val)
{
	const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
	return api->get_rms_voltage(dev, val);
	// return ade9000_get_rms_voltage(dev, val);
}

static inline int ade9000_get_rms_current_api(const struct device *dev, uint32_t *val)
{
	const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
	return api->get_rms_current(dev, val);
	// return ade9000_get_rms_current(dev, val);
}

static inline int ade9000_get_active_power_api(const struct device *dev, int32_t *val)
{
	const struct ade9000_driver_api *api = (const struct ade9000_driver_api *)dev->api;
	return api->get_active_power(dev, val);
	// return ade9000_get_active_power(dev, val);
}


// #include <syscalls/ade9000_POD.h>

#ifdef __cplusplus
}
#endif
