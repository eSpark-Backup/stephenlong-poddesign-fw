#include "adxl345.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(ADXL345_POD, ADXL345_POD_LOG_LEVEL);

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
    uint8_t reg_data;
    adxl345_i2c_read(ADXL345_INT_SOURCE_REG, &reg_data, sizeof(reg_data));

    if(BIT(2) && reg_data)
    {
        LOG_DBG("Data ready interrupt.");
        int ret = sensor_sample_fetch(_adxl345);
        if ( ret < 0) {
            LOG_ERR("Failed to fetch sample, ret: %d", ret);
        }
    }
    if(BIT(2) && reg_data)
    {
        LOG_DBG("Impact detected.");
    }
}

void adxl345_trigger_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_ERR("ADXL345 Interrupt Triggered!");

    k_work_submit(&_adxl345_worker);
}

void configure_adxl345_alert_pin(void)
{
    if (!_adxl345_port_gpio_dev) {
        LOG_ERR("Error: Failed to bind to GPIO port");
        return;
    }

    uint8_t reg_val = 0;
    // set ADXL345 INT1 pin mode to Active low
    adxl345_i2c_read(ADXL345_DATA_FORMAT_REG, &reg_val, sizeof(reg_val));
    reg_val |= BIT(5);          // set bit 5
    adxl345_i2c_write(ADXL345_DATA_FORMAT_REG, &reg_val, sizeof(reg_val));

    // set ADXL345 INT1 mode to Free Fall and Data ready, all other interrupts are mapped to INT2
    reg_val = ~(BIT(7) | BIT(2));
    adxl345_i2c_write(ADXL345_INT_MAP_REG, &reg_val, sizeof(reg_val));
    adxl345_i2c_write(ADXL345_INT_ENABLE_REG, &reg_val, sizeof(reg_val));       // Enable set interrupts
    
    // // configure FIFO
    // reg_val = (0x02 << 6) | (0x00 << 5) | (0x1F);
    // adxl345_i2c_write(ADXL345_FIFO_CTL_REG, &reg_val, sizeof(reg_val));
    
    // Now initialize GPIO in falling edge interrupt mode
    gpio_pin_configure(_adxl345_port_gpio_dev, ACCEL_INT_PIN,
                       GPIO_INPUT);
    gpio_pin_interrupt_configure(_adxl345_port_gpio_dev, ACCEL_INT_PIN,
                                 GPIO_INT_EDGE_FALLING);

    gpio_init_callback(&_adxl345_alert_cb_data, adxl345_trigger_handler,
                       BIT(ACCEL_INT_PIN));
    gpio_add_callback(_adxl345_port_gpio_dev, &_adxl345_alert_cb_data);

    LOG_DBG("MAX17048 ALERT interrupt configured on %s.%d", _adxl345_port_gpio_dev->name, ACCEL_INT_PIN);
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

    uint8_t reg_val = 0x09;
    adxl345_i2c_write(ADXL345_FF_THRESH_REG, &reg_val, sizeof(reg_val));
    reg_val = 0x14;
    adxl345_i2c_write(ADXL345_FF_TIME_REG, &reg_val, sizeof(reg_val));
    
    configure_adxl345_alert_pin();

    k_work_init(&_adxl345_worker, adxl345_work_handler);

    // gpio_pin_configure(_adxl345_port_gpio_dev, ACCEL_CS_PIN, GPIO_OUTPUT);
    // gpio_pin_set(_adxl345_port_gpio_dev, ACCEL_CS_PIN, 1);

    // Configure sensor trigger for motion detection
    struct sensor_trigger trig = {
        // .type = SENSOR_TRIG_STATIONARY,
        // .type = SENSOR_TRIG_MOTION,
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    struct sensor_value val[3] = {0, 0};

    while(1)
    {
        // int ret = sensor_sample_fetch(_adxl345);
        // if ( ret < 0) {
        //     LOG_ERR("Failed to fetch sample, ret: %d", ret);
        //     continue;
        // }

        int ret = sensor_channel_get(_adxl345, SENSOR_CHAN_ACCEL_XYZ, val);
        if ( ret < 0) {
            LOG_ERR("Failed to fetch sample, ret: %d", ret);
            continue;
        }

        LOG_INF("[m/s^2]:\t\t(%12.6f, %12.6f, %12.6f)",
                 (sensor_value_to_double(&val[0])),//*0.004*9.80665f),
                 (sensor_value_to_double(&val[1])),//*0.004*9.80665f),
                 (sensor_value_to_double(&val[2])));//*0.004*9.80665f));

        k_sleep(K_MSEC(100));
    }

    // int ret = sensor_trigger_set(_adxl345, &trig, adxl345_trigger_handler);
    // if ( ret < 0) {
    //     LOG_ERR("Failed to set trigger, ret: %d", ret);
    //     return;
    // }

    LOG_DBG("ADXL345 sensor initialized");
}