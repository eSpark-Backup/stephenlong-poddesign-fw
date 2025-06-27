#include "defines.h"


#define NUM_CHANNELS            4
#define ADC_RESOLUTION          16

#define ADS1115_GAIN            ADC_GAIN_1_2
#define ADS1115_LEAST_COUNT     0.125       // mV, least count of ADC with FSR = +/- 4.096V
#define DC_CURRENT_CHANNEL      0
#define DC_VOLTAGE_CHANNEL      1

#define VOLTAGE_RANGE_MIN       0
#define VOLTAGE_RANGE_MAX       DC_VOLTAGE_RANGE
#define CURRENT_RANGE_MIN       0
#define CURRENT_RANGE_MAX       DC_CURRENT_RANGE

#define HALLCURRENT8120A_QUIESCENT_VOLTAGE      1.65f

extern const struct device *_ads1115;

float get_dc_voltage(void);
float get_dc_current(void);

void init_ads1115(const struct device* ads1115);