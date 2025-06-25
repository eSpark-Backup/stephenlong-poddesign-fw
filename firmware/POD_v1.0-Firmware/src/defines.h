#pragma once

#include <zephyr/logging/log.h>

#define MAX17048_POD_LOG_LEVEL      4
#define ADS1115_POD_LOG_LEVEL       4
#define SHTC6_POD_LOG_LEVEL         4
#define APP_LOG_LEVEL               4


#define FUEL_GAUGE_IC_ADDR 0x36
#define FUEL_GUAGE_MIN_THRESH       3.4
#define FUEL_GUAGE_MAX_THRESH       4.4

#define DC_VOLTAGE_RANGE            48      // 0-48V
#define DC_CURRENT_RANGE            50      // 0-50A

// GPIOs
#define GPIO_PORT_0     gpio0//"GPIO_0"
#define GPIO_PORT_1     gpio1//"GPIO_1"

#define FLASH_IC_GPIO_PORT          GPIO_PORT_0
#define FLASH_IC_CS                 17
#define FLASH_IC_CLK                19
#define FLASH_IC_QSPI0              20
#define FLASH_IC_QSPI1              21
#define FLASH_IC_QSPI2              22
#define FLASH_IC_QSPI3              23

#define POWER_STATUS_GPIO_PORT      GPIO_PORT_0
#define POWER_STATUS_GPIO           8

#define FUEL_GAUGE_GPIO_PORT        GPIO_PORT_1
#define FUEL_GAUGE_GPIO             8

#define ACCEL_GPIO_PORT             GPIO_PORT_1
#define ACCEL_INT                   9
#define ACCEL_CS                    11

#define GROUND_FAULT_GPIO_PORT      GPIO_PORT_1
#define GROUND_FAULT_ALARM_VA       1
#define GROUND_FAULT_ALARM_VB       2
#define GROUND_FAULT_ALARM_VC       3
#define GROUND_FAULT_PTT_VA         4
#define GROUND_FAULT_PTT_VB         5
#define GROUND_FAULT_PTT_VC         6
#define GROUND_FAULT_SCR_SIG_VA     12
#define GROUND_FAULT_SCR_SIG_VB     13
#define GROUND_FAULT_SCR_SIG_VC     14

static inline float map_value(float val, float in_min, float in_max, float out_min, float out_max) {
    return out_min + ((val - in_min) * (out_max - out_min) / (in_max - in_min));
}