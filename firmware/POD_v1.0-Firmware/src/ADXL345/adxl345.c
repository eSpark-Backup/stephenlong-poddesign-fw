#include "adxl345.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(ADXL345_POD, ADXL345_POD_LOG_LEVEL);

#define INTERRUPTS_MASK     (ADXL345_INT_SRC_ACT_MSK)
// #define INTERRUPTS_MASK     (ADXL345_INT_SRC_DR_MSK | ADXL345_INT_SRC_FF_MSK)

const struct device *_adxl345 = NULL;
const struct device *_adxl345_port_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(ACCEL_PORT));
struct gpio_callback _adxl345_alert_cb_data;

static struct k_work _adxl345_worker;

void adxl345_i2c_write(uint8_t reg_addr, const uint8_t *data, size_t data_len)
{
    const struct i2c_dt_spec *cfg = _adxl345->config;
    uint8_t buffer[1 + data_len];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, data_len);

    int ret = i2c_write_dt(cfg, buffer, sizeof(buffer));
    if (ret < 0) {
        LOG_ERR("Failed to write register: 0x%x  with error: %d", reg_addr, ret);
        return;
    }
}

void adxl345_i2c_read(uint8_t reg_addr, const uint8_t *data, size_t data_len)
{
    const struct i2c_dt_spec *cfg = _adxl345->config;
    int ret = i2c_write_read_dt(cfg, &reg_addr, sizeof(reg_addr), data, data_len);
    if (ret < 0) {
        LOG_ERR("Failed to read register: 0x%x  with error: %d", reg_addr, ret);
        return;
    }
}

void adxl345_work_handler(struct k_work *work)
{
    LOG_DBG("Worker triggerred.");
    uint8_t reg_data = 0;
    adxl345_i2c_read(ADXL345_INT_SOURCE_REG, &reg_data, sizeof(reg_data));
    // LOG_DBG("INT_SOURCE = 0x%02X", reg_data);

    if(ADXL345_INT_SRC_DR_MSK & reg_data)
    {
        LOG_DBG("Data ready interrupt.");
        int ret = sensor_sample_fetch(_adxl345);
        if ( ret < 0) {
            LOG_ERR("Failed to fetch sample, ret: %d", ret);
        }
    }
    if(ADXL345_INT_SRC_FF_MSK & reg_data)
    {
        LOG_DBG("Free-Fall detected.");
    }
    if(ADXL345_INT_SRC_ACT_MSK & reg_data)
    {
        LOG_DBG("Activity detected.");
    }
}

void adxl345_trigger_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_DBG("ADXL345 Interrupt Triggered!");

    k_work_submit(&_adxl345_worker);
}

void verify_settings(void)
{
    uint8_t int_en, int_map, int_src, data_format, fifo_ctl;
    adxl345_i2c_read(ADXL345_INT_ENABLE_REG, &int_en, 1);
    adxl345_i2c_read(ADXL345_INT_MAP_REG, &int_map, 1);
    adxl345_i2c_read(ADXL345_INT_SOURCE_REG, &int_src, 1);
    adxl345_i2c_read(ADXL345_DATA_FORMAT_REG, &data_format, 1);
    adxl345_i2c_read(ADXL345_FIFO_CTL_REG, &fifo_ctl, 1);

    LOG_INF("INT_ENABLE: 0x%02X, INT_MAP: 0x%02X, INT_SOURCE: 0x%02X, DATA_FORMT: 0x%02X, FIFO_CTL: 0x%02X", int_en, int_map, int_src, data_format, fifo_ctl);
}

void configure_adxl345_alert_pin(void)
{
    if (!_adxl345_port_gpio_dev) {
        LOG_ERR("Error: Failed to bind to GPIO port");
        return;
    }
    
    // Initialize GPIO in falling edge interrupt mode
    gpio_pin_configure(_adxl345_port_gpio_dev, ACCEL_INT_PIN,
                       GPIO_INPUT);
    gpio_pin_interrupt_configure(_adxl345_port_gpio_dev, ACCEL_INT_PIN,
                                 GPIO_INT_EDGE_TO_INACTIVE);

    gpio_init_callback(&_adxl345_alert_cb_data, adxl345_trigger_handler,
                       BIT(ACCEL_INT_PIN));
    gpio_add_callback(_adxl345_port_gpio_dev, &_adxl345_alert_cb_data);

    LOG_DBG("MAX17048 ALERT interrupt configured on %s.%d", _adxl345_port_gpio_dev->name, ACCEL_INT_PIN);

    uint8_t reg_val = 0;
    // set ADXL345 INT1 pin mode to Active low
    adxl345_i2c_read(ADXL345_DATA_FORMAT_REG, &reg_val, sizeof(reg_val));
    reg_val |= BIT(5);          // set bit 5
    adxl345_i2c_write(ADXL345_DATA_FORMAT_REG, &reg_val, sizeof(reg_val));

    // set ADXL345 INT1 mode to Free Fall and Data ready, all other interrupts are mapped to INT2
    reg_val = ~INTERRUPTS_MASK;
    adxl345_i2c_write(ADXL345_INT_MAP_REG, &reg_val, sizeof(reg_val));
    reg_val = INTERRUPTS_MASK;
    adxl345_i2c_write(ADXL345_INT_ENABLE_REG, &reg_val, sizeof(reg_val));       // Enable interrupts
    
    // // configure FIFO
    // reg_val = (0x02 << 6) | (0x00 << 5) | (0x1F);
    // adxl345_i2c_write(ADXL345_FIFO_CTL_REG, &reg_val, sizeof(reg_val));

    adxl345_i2c_read(ADXL345_INT_SOURCE_REG, &reg_val, sizeof(reg_val));
}

void init_adxl345(const struct device* adxl345)
{
    _adxl345 = adxl345;

    if ((_adxl345 == NULL)) {
		LOG_ERR("I2C device not found. _adxl345: %p", _adxl345);
		return;
	}

	if (!device_is_ready(_adxl345)) {
        LOG_ERR("ADXL345 device not ready");
        return;
    }

    uint8_t reg_val = 0x90;         // 9g
    adxl345_i2c_write(ADXL345_THRESH_ACT_REG, &reg_val, sizeof(reg_val));
    reg_val = 0xF0;
    adxl345_i2c_write(ADXL345_ACT_CTL_REG, &reg_val, sizeof(reg_val));
    
    k_work_init(&_adxl345_worker, adxl345_work_handler);

    configure_adxl345_alert_pin();
    verify_settings();

    // gpio_pin_configure(_adxl345_port_gpio_dev, ACCEL_CS_PIN, GPIO_OUTPUT);
    // gpio_pin_set(_adxl345_port_gpio_dev, ACCEL_CS_PIN, 1);

    // struct sensor_value val[3] = {0, 0};

    // while(1)
    // {
    //     int ret = sensor_channel_get(_adxl345, SENSOR_CHAN_ACCEL_XYZ, val);
    //     if ( ret < 0) {
    //         LOG_ERR("Failed to fetch sample, ret: %d", ret);
    //         continue;
    //     }

    //     LOG_INF("[m/s^2]:\t(%12.6f, %12.6f, %12.6f)",
    //              (sensor_value_to_double(&val[0])),//*0.004*9.80665f),
    //              (sensor_value_to_double(&val[1])),//*0.004*9.80665f),
    //              (sensor_value_to_double(&val[2])));//*0.004*9.80665f));

    //     k_sleep(K_MSEC(100));
    // }

    LOG_DBG("ADXL345 sensor initialized");
}