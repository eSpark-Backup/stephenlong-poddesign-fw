#include "w25q128.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(W25Q128_POD, W25Q128_POD_LOG_LEVEL);

const struct device *_w25q128 = NULL;

void init_w25q128(const struct device* w25q128)
{
    _w25q128 = w25q128;

    if (_w25q128 == NULL) {
		LOG_ERR("Flash device not found.");
		return;
	}

	if (!device_is_ready(_w25q128)) {
        LOG_ERR("W25Q128 device not ready");
        return;
    }

    while(1)
	{
		uint8_t buf[256];

		/* Read example */
		if (flash_read(_w25q128, 0x000000, buf, sizeof(buf)) == 0) {
			printk("QSPI Read Success\n");
		}

		/* Erase sector (typically 4KB) */
		if (flash_erase(_w25q128, 0x000000, 4096) == 0) {
			printk("QSPI Erase Success\n");
		}

		/* Write example */
		memset(buf, 0xAB, sizeof(buf));
		if (flash_write(_w25q128, 0x000000, buf, sizeof(buf)) == 0) {
			printk("QSPI Write Success\n");
		}

		k_sleep(K_SECONDS(5));
	}

    LOG_DBG("W25Q128 QSPI-NOR flash initialized");
}