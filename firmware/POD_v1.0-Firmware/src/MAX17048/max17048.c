#include "max17048.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(MAX17048_POD, MAX17048_POD_LOG_LEVEL);

const struct device *_max17048 = NULL;
const struct device *_max17048_alert_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(ALERT_PORT));
struct gpio_callback _max17048_alert_cb_data;

static struct k_timer _max17048_timer;
static struct k_work _max17048_worker;

fuel_gauge_prop_t props[] = {
    // FUEL_GAUGE_RUNTIME_TO_EMPTY,
    // FUEL_GAUGE_RUNTIME_TO_FULL,
    FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
    FUEL_GAUGE_VOLTAGE,
};
union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];
enum worker_tasks_e task = NO_TASK;

void max17048_i2c_write(uint8_t reg_addr, const uint8_t *data, size_t data_len)
{
    const struct i2c_dt_spec *cfg = _max17048->config;
    uint8_t buffer[1 + data_len];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, data_len);

    int ret = i2c_write_dt(cfg, buffer, sizeof(buffer));
    if (ret < 0) {
        LOG_ERR("Failed to write register: 0x%x  with error: %d", reg_addr, ret);
        return;
    }
}

void max17048_i2c_read(uint8_t reg_addr, const uint8_t *data, size_t data_len)
{
    const struct i2c_dt_spec *cfg = _max17048->config;
    int ret = i2c_write_read_dt(cfg, &reg_addr, sizeof(reg_addr), (void*)data, data_len);
    if (ret < 0) {
        LOG_ERR("Failed to read register: 0x%x  with error: %d", reg_addr, ret);
        return;
    }
}

void max17048_work_handler(struct k_work *work)
{
    LOG_DBG("Worker triggerred. %d", task);
    
    // unsigned int key = irq_lock();
    
    switch(task)
    {
        case INTERRUPT_TASK:
            // while(1)
            // {
            //     uint8_t temp[2];
                uint8_t status_raw[2];
                uint8_t config_reg[2];
        
                max17048_i2c_read(MAX17048_REG_STATUS, status_raw, sizeof(status_raw));
                LOG_DBG("STATUS reg: 0x%04x", ((status_raw[1] << 8) | status_raw[0]));
                status_raw[0] &= 0x40;
                
                // status_raw[0] ^= status_raw[1];
                // status_raw[1] ^= status_raw[0];
                // status_raw[0] ^= status_raw[1];
                LOG_DBG("STATUS reg: 0x%04x", ((status_raw[1] << 8) | status_raw[0]));
                max17048_i2c_write(MAX17048_REG_STATUS, status_raw, sizeof(status_raw));
                // max17048_i2c_read(MAX17048_REG_STATUS, temp, sizeof(temp));
        
                // if( (temp[0] != status_raw[0]) || (temp[1] != status_raw[1]))
                // {
                //     LOG_ERR("MAX17048_REG_STATUS values dont match");
                //     LOG_ERR("temp[0]: %x\ttemp[1]: %x", temp[0], temp[1]);
                //     LOG_ERR("status_raw[0]: %x\tstatus_raw[1]: %x", status_raw[0], status_raw[1]);
                // }
                
                
                max17048_i2c_read(MAX17048_REG_CONFIG, config_reg, sizeof(config_reg));
                LOG_DBG("CONFIG before clearing ALRT: 0x%04x", ((config_reg[1] << 8) |config_reg[0]));
                // Clear ALRT bit (bit 5)
                // config_reg[1] &= ~(1 << 5);
                config_reg[1] &= 0xDF;
                
                LOG_DBG("CONFIG after clearing ALRT: 0x%04x", ((config_reg[1] << 8) |config_reg[0]));
                // config_reg[0] ^= config_reg[1];
                // config_reg[1] ^= config_reg[0];
                // config_reg[0] ^= config_reg[1];
                max17048_i2c_write(MAX17048_REG_CONFIG, config_reg, sizeof(config_reg));
                // max17048_i2c_read(MAX17048_REG_CONFIG, temp, sizeof(temp));
        
                // if( (temp[0] != config_reg[1]) || (temp[1] != config_reg[0]))
                // {
                //     LOG_ERR("MAX17048_REG_CONFIG values dont match");
                //     LOG_ERR("temp[0]: %x\ttemp[1]: %x", temp[0], temp[1]);
                //     LOG_ERR("config_reg[0]: %x\tconfig_reg[1]: %x", config_reg[0], config_reg[1]);
                // }
        
            //     k_sleep(K_MSEC(100));
            // }

            (INTERRUPT_TASK != task) ? k_work_submit(&_max17048_worker) : (task = NO_TASK);

            break;
        case DATA_ACQUIRE_TASK:
            int ret = fuel_gauge_get_props(_max17048, props, vals, ARRAY_SIZE(props));
            if (ret < 0) {
                LOG_ERR("Failed to fetch data from MAX17048 sensor: %d", ret);
            }

            (DATA_ACQUIRE_TASK != task) ? k_work_submit(&_max17048_worker) : (task = NO_TASK);

            break;
        default:
            task = NO_TASK;
    }
}

void max17048_alert_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // LOG_DBG("MAX17048 ALERT triggered!");
    task = INTERRUPT_TASK;
    k_work_submit(&_max17048_worker);
}

void max17048_timer_handler(struct k_timer *timer)
{
    task = DATA_ACQUIRE_TASK;
    k_work_submit(&_max17048_worker);
}

void configure_max17048_alert_pin(void)
{
    if (!_max17048_alert_gpio_dev) {
        LOG_ERR("Error: Failed to bind to GPIO port");
        return;
    }

    gpio_pin_configure(_max17048_alert_gpio_dev, ALERT_PIN,
                       GPIO_INPUT);
    gpio_pin_interrupt_configure(_max17048_alert_gpio_dev, ALERT_PIN,
                                 GPIO_INT_EDGE_FALLING);

    gpio_init_callback(&_max17048_alert_cb_data, max17048_alert_handler,
                       BIT(ALERT_PIN));
    gpio_add_callback(_max17048_alert_gpio_dev, &_max17048_alert_cb_data);

    LOG_DBG("MAX17048 ALERT interrupt configured on %s.%d", _max17048_alert_gpio_dev->name, ALERT_PIN);
}

int max17048_set_valrt(float min, float max)
{
    uint8_t high_byte = (uint8_t)((max * 1000.0f) / VALRT_1LSB_THRESH);
    uint8_t low_byte  = (uint8_t)((min * 1000.0f) / VALRT_1LSB_THRESH);

    uint16_t valrt = ((uint16_t)high_byte << 8) | low_byte;
    uint8_t buf[2];

    buf[0] = valrt & 0xFF;          // LSB first (MIN)
    buf[1] = (valrt >> 8) & 0xFF;   // MSB (MAX)
    max17048_i2c_write(MAX17048_REG_VALRT, buf, sizeof(buf));

    LOG_DBG("VALRT configured: MIN = (%d)%dmV, MAX = (%d)%dmV",
             (low_byte), (uint32_t)(low_byte*VALRT_1LSB_THRESH), (high_byte), (uint32_t)(high_byte*VALRT_1LSB_THRESH));
    return 0;
}

int get_batt_voltage(void)
{
    int voltage = vals[1].voltage;

    // int volts = voltage / 1000000;
    // int microvolts_rem = voltage % 1000000;
    // LOG_ERR("voltage: %d\tvolts: %d\tmicrovolts_rem: %d", voltage, volts, microvolts_rem);

    // double value = volts + (microvolts_rem / 1000000.0f);
    // int whole = (int)value;
    // int frac = (int)((value - whole) * 1000000);
    // LOG_ERR("final_volts: %d.%06d", whole, frac);

    // return volts + (microvolts_rem / 1000000.0f);
    return voltage;
}

uint8_t get_batt_soc(void)
{
    return vals[0].relative_state_of_charge;
}

void init_max17048(const struct device* max17048)
{
    _max17048 = max17048;

    if ((_max17048 == NULL)) {
		LOG_ERR("I2C device not found.");
		return;
	}

	if (!device_is_ready(_max17048)) {
        LOG_ERR("MAX17048 device not ready");
        return;
    }

    max17048_set_valrt(MAX17048_MIN_VOLT_THRESH, MAX17048_MAX_VOLT_THRESH);
	configure_max17048_alert_pin();

    k_work_init(&_max17048_worker, max17048_work_handler);

    k_timer_init(&_max17048_timer, max17048_timer_handler, NULL);
    k_timer_start(&_max17048_timer, K_SECONDS(5), K_SECONDS(10));

    LOG_DBG("MAX17048 sensor initialized");
}
