#include "ads1115.h"

// #include "ads1x15.h" 
// #include "ads1115_defs.h" 

LOG_MODULE_REGISTER(ADS1115_POD, ADS1115_POD_LOG_LEVEL);

const struct device *_ads1115;

void init_ads1115(const struct device* ads1115)
{
    _ads1115 = ads1115;

    if ((_ads1115 == NULL)) {
		LOG_ERR("I2C device not found.");
		return;
	}

	if (!device_is_ready(_ads1115)) {
        LOG_ERR("ADS1115 device not ready");
        return;
    }
}
