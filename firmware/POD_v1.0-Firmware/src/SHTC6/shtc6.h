#include "defines.h"

// #include <zephyr/drivers/sensor.h>

extern const struct device *_shtc6;

double get_temp(void);
double get_humidity(void);

void init_shtc6(const struct device* shtc6);