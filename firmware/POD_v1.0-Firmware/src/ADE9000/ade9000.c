#include "CRC-16-CCITT.h"  

#include <stdint.h>
#include <math.h>

#include "ade9000.h"

LOG_MODULE_REGISTER(ADE9000_POD, ADE9000_LOG_LEVEL);

#define SPI_MANUAL_TRANSCEIVE(tx_buf_ptr, tx_len, rx_buf_ptr, rx_len) ({ \
    int _ret; \
    struct spi_buf _tx_buf = { .buf = (tx_buf_ptr), .len = (tx_len) }; \
    struct spi_buf _rx_buf = { .buf = (rx_buf_ptr), .len = (rx_len) }; \
    struct spi_buf_set _tx_set = { .buffers = &_tx_buf, .count = 1 }; \
    struct spi_buf_set _rx_set = { .buffers = &_rx_buf, .count = 1 }; \
    cs_low(); \
    _ret = spi_transceive((_ade9000), (&spi_cfg), &_tx_set, &_rx_set); \
    cs_high(); \
    if (_ret) { \
        LOG_ERR("SPI_MANUAL_TRANSCEIVE failed: %d", _ret); \
    } \
    _ret; \
})

#define SPI_MANUAL_WRITE(tx_buf_ptr, tx_len) ({ \
    int _ret; \
    struct spi_buf _tx_buf = { .buf = (tx_buf_ptr), .len = (tx_len) }; \
    struct spi_buf_set _tx_set = { .buffers = &_tx_buf, .count = 1 }; \
    cs_low(); \
    _ret = spi_write((_ade9000), (&spi_cfg), &_tx_set); \
    cs_high(); \
    if (_ret) { \
        LOG_ERR("SPI_MANUAL_WRITE failed: %d", _ret); \
    } \
    _ret; \
})

const struct device *_ade9000 = NULL;
const struct device *_ade9000_cs_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(CS_PORT));

// Register names from 0x680
static const char *phaseVarNames[PHASE_VAR_COUNT] =
{
    "Instantaneous Voltage",
    "Instantaneous Current",
    "RMS Current",
    "RMS Voltage",
    "Total Active Power",
    "Total Apparent Power",
    "Total Reactive Power",
    "Fundamental Reactive Power",
    "Power Factor",
    "Voltage THD",
    "Current THD",
    "Fundamental Active Power",
    "Fundamental Apparent Power",
    "Fundamental Current RMS",
    "Fundamental Voltage RMS",
    "Current Fast RMS",
    "Voltage Fast RMS",
    "Current Fast10 RMS",
    "Voltage Fast10 RMS"
};

#define ADE_READ_INTERVAL  500
#define ADE_WQ_PRIORITY    5
#define ADE_WQ_STACK_SIZE  1024
K_THREAD_STACK_DEFINE(ade_stack_area, 1024);
struct k_work_q         _ade9000_work_q;
struct k_work_delayable _ade9000_worker;

struct PhaseBurstData_t phaseData[PHASE_MAX];

// struct ade9000_spi ade9000;
static struct spi_config spi_cfg = {
    .frequency = SPI_SPEED,
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
    .cs = NULL,  // manual CS control
};


uint8_t gainI; // Current gain of all Channel A B C
uint8_t gainV;
    int netfreq;

    void (*_logFunc)(const char * msg, ...);

//   private:
    float _calibration[6];
    float _adcToI_L1, _adcToI_L2, _adcToI_L3;
    float _adcToV_L1, _adcToV_L2, _adcToV_L3;
    float _adcToIRMS_L1,_adcToIRMS_L2,_adcToIRMS_L3;
    float _adcToVRMS_L1,_adcToVRMS_L2,_adcToVRMS_L3;
    float _adcToPower_L1,_adcToPower_L2,_adcToPower_L3;

// Class members
    bool _spi_inited = false;
    
    // const uint8_t _PM1_PIN;
    // const uint8_t _DREADY_PIN;
    // const uint8_t _RESET_PIN;
    // int8_t _CS;

    int _samplingrate;
    void (*_interruptHandler)(void);
    void (*_interruptHandler2)(void*);
    bool _burst_en = false;


    uint8_t _rx_array[9];
    uint8_t _burst_read_tx_array [31];
    uint8_t _burst_read_rx_array [31];

    const uint16_t addr32k = 0x500;  // 32k Register @ addr 0x500(0)
    const uint16_t addr8k = 0x510;  // 8k Register @ addr 0x510(0)



static void cs_low(void)
{
    gpio_pin_set(_ade9000_cs_gpio_dev, CS_PIN, 0);
}

static void cs_high(void)
{
    gpio_pin_set(_ade9000_cs_gpio_dev, CS_PIN, 1);
}

static void configure_ade9000_cs_pin(void)
{
    if (!_ade9000_cs_gpio_dev) {
        LOG_ERR("Error: Failed to bind to GPIO port");
        return;
    }

    if (!device_is_ready(_ade9000_cs_gpio_dev)) {
      LOG_ERR("CS GPIO not ready");
          return;
    }

    int ret = gpio_pin_configure(_ade9000_cs_gpio_dev, CS_PIN, GPIO_OUTPUT_ACTIVE);
    if (ret) {
        LOG_ERR("Failed to configure CS GPIO");
        return;
    }

    cs_high();

    LOG_DBG("ADE9000 CS configured on %s.%d", _ade9000_cs_gpio_dev->name, CS_PIN);
}


// _____________________________________________________________________________
int8_t ADE9000_init(int fNet)
{
    // _spi = new SPIClass(spi_bus);
    _burst_en = false;
    _interruptHandler = NULL;
    _interruptHandler2= NULL;
    _samplingrate = -1;
    _spi_inited = false;

    netfreq = fNet;

    // Init arrays with zeros
    for (uint8_t i = 0; i < 31; i++) {
        _burst_read_tx_array[i] = 0;
        _burst_read_rx_array[i] = 0;
    }

    gainI = STANDARD_CURRENT_GAIN;
    gainV = STANDARD_VOLTAGE_GAIN;


    // for (uint8_t i = 0; i < 6; i++) _calibration[i] = 1.0;

    // ADE9000_calcConstants();

    _logFunc = NULL;

    k_sleep(K_MSEC(10));

    uint32_t chip_id = ADE9000__read_32(ADDR_ID);
    if(!(chip_id & ADE9000_ADE9000_ID))
    {
        LOG_ERR("ADE id not match. chip_id: 0x%X", chip_id);
        return -1;
    }
    LOG_DBG("Chip ID: 0x%08lX (%" PRIu32 ")", (unsigned long int)chip_id, chip_id);


    ADE9000__write_16(ADDR_PGA_GAIN,ADE9000_PGA_GAIN);
    ADE9000__write_32(ADDR_CONFIG0,ADE9000_CONFIG0); 
    // ADE9000__write_16(ADDR_CONFIG1,ADE9000_CONFIG1);
    ADE9000__write_16(ADDR_CONFIG1, (ADE9000__read_16(ADDR_CONFIG1) | (ADE9000_BURST_EN)));

    ADE9000__write_16(ADDR_CONFIG2,ADE9000_CONFIG2); 
    ADE9000__write_16(ADDR_CONFIG3,ADE9000_CONFIG3); 
    if (netfreq == 60) {
        ADE9000__write_16(ADDR_ACCMODE,ADE9000_ACCMODE_60);
    } else {
        ADE9000__write_16(ADDR_ACCMODE,ADE9000_ACCMODE);
    }
    ADE9000__write_16(ADDR_TEMP_CFG,ADE9000_TEMP_CFG);
    ADE9000__write_16(ADDR_ZX_LP_SEL,ADE9000_ZX_LP_SEL); 
    ADE9000__write_32(ADDR_MASK0,ADE9000_MASK0); 
    ADE9000__write_32(ADDR_MASK1,ADE9000_MASK1); 
    ADE9000__write_32(ADDR_EVENT_MASK,ADE9000_EVENT_MASK); 
    ADE9000__write_16(ADDR_WFB_CFG,ADE9000_WFB_CFG);

    // ADE9000__write_16(ADDR_WFB_CFG, 0);  // Ensure WF_CAP_EN = 0
    // ADE9000__write_16(ADDR_WFB_CFG,
    //     (ADE9000_WF_SRC & BIT(9)) |            // choose waveform source
    //     (1 << ADDR_WFB_CFG) |   // fixed rate
    //     ADDR_WFB_CFG |  // continuous filling
    //     (ADE9000_WF_IN_EN) |     // include IN channel if needed
    //     ADDR_WFB_CFG        // e.g. 0b0000 for all channels
    // );
    // ADE9000__write_16(ADDR_WFB_CFG, ADE9000__read_16(ADDR_WFB_CFG) | ADE9000_WF_CAP_EN);  // Enable buffer

    ADE9000__write_32(ADDR_ACT_NL_LVL,0); 
    ADE9000__write_32(ADDR_REACT_NL_LVL,0); 
    ADE9000__write_32(ADDR_APP_NL_LVL,0); 

    ADE9000__write_32(ADDR_DICOEFF,ADE9000_DICOEFF); 
    ADE9000__write_32(ADDR_VLEVEL,ADE9000_VLEVEL); 
    ADE9000__write_16(ADDR_EGY_TIME,ADE9000_EGY_TIME); 
    ADE9000__write_16(ADDR_EP_CFG,ADE9000_EP_CFG);		//Energy accumulation ON 
    // if (_logFunc) _logFunc("Set Run On");
    ADE9000__write_16(ADDR_RUN,ADE9000_RUN_ON);		//DSP ON

    return 0;
}


void ADE9000_setCalibration(float *values)
{
    for (int i = 0; i < 6; i++) _calibration[i] = values[i];
}

void get_PGA_gain(uint8_t *currentGain, uint8_t *voltageGain) {
    uint16_t pgaGainRegister;

    pgaGainRegister = ADE9000__read_16(ADDR_PGA_GAIN);
    uint8_t temp = pgaGainRegister & 0x0003;
    *currentGain = (temp == 0) ? 1.0 : (temp == 1.0) ? 2.0 : 4.0;
    temp = (pgaGainRegister >> 8) & 0x0003;
    *voltageGain = (temp == 0) ? 1.0 : (temp == 1.0) ? 2.0 : 4.0;
}

void ADE9000_calcConstants() {
    double _adcToI, _adcToV, _adcToIRMS, _adcToVRMS, _adcToPower;
    get_PGA_gain(&gainI, &gainV);

    if (_samplingrate == 8000) {
        _adcToI = 1.0 / (double)((double)CURRENT_TRANSFER_FUNCTION * (double)ADE9000_8K_FULL_SCALE_CODES * 1000.0); // 1000 for mA
        _adcToV = 1.0 / (double)((double)VOLTAGE_TRANSFER_FUNCTION * (double)ADE9000_8K_FULL_SCALE_CODES); 
    } else {
        _adcToI = 1.0 / (double)((double)CURRENT_TRANSFER_FUNCTION * (double)ADE9000_32K_FULL_SCALE_CODES * 1000.0); // 1000 for mA
        _adcToV = 1.0 / (double)((double)VOLTAGE_TRANSFER_FUNCTION * (double)ADE9000_32K_FULL_SCALE_CODES); 
    }
    _adcToIRMS = (ADC_FS_RMS(gainI) / ((double)CURRENT_TRANSFER_FUNCTION * ((double)ADE9000_RMS_FULL_SCALE_CODES))) * 1000.0; // 1000 for mA
    _adcToVRMS = ADC_FS_RMS(gainV) / ((double)VOLTAGE_TRANSFER_FUNCTION * ((double)ADE9000_RMS_FULL_SCALE_CODES));
    _adcToPower = 1.0 / (double)((((double)CURRENT_TRANSFER_FUNCTION*(double)VOLTAGE_TRANSFER_FUNCTION)) * ((double)ADE9000_WATT_FULL_SCALE_CODES) * 2.0);


    _adcToI = (double)(_adcToI/gainI);
    _adcToV = (double)(_adcToV/gainV);
    _adcToPower = (double)(_adcToPower/(gainI*gainV));


    _adcToI_L1 = _adcToI*(double)_calibration[1];
    _adcToI_L2 = _adcToI*(double)_calibration[3];
    _adcToI_L3 = _adcToI*(double)_calibration[5];
    _adcToV_L1 = _adcToV*(double)_calibration[0];
    _adcToV_L2 = _adcToV*(double)_calibration[2];
    _adcToV_L3 = _adcToV*(double)_calibration[4];
    _adcToIRMS_L1 = _adcToIRMS*(double)_calibration[1];
    _adcToIRMS_L2 = _adcToIRMS*(double)_calibration[3];
    _adcToIRMS_L3 = _adcToIRMS*(double)_calibration[5];
    _adcToVRMS_L1 = _adcToVRMS*(double)_calibration[0];
    _adcToVRMS_L2 = _adcToVRMS*(double)_calibration[2];
    _adcToVRMS_L3 = _adcToVRMS*(double)_calibration[4];
    _adcToPower_L1 = _adcToPower*(double)_calibration[0]*(double)_calibration[1];
    _adcToPower_L2 = _adcToPower*(double)_calibration[2]*(double)_calibration[3];
    _adcToPower_L3 = _adcToPower*(double)_calibration[4]*(double)_calibration[5];

    // LOG_WRN("VTF=%f, CTF=%f", VOLTAGE_TRANSFER_FUNCTION, CURRENT_TRANSFER_FUNCTION);
    // LOG_WRN("_calibration[] data");
    // for (int i = 0; i < 6; i++) LOG_WRN("{%d: %f} ", i, _calibration[i]);

    // LOG_WRN("_adcToI_L1: %f, _adcToI_L2: %f, _adcToI_L3: %f, _adcToV_L1: %f, _adcToV_L2: %f, _adcToV_L3: %f, _adcToIRMS_L1: %f, _adcToIRMS_L2: %f, "
    //         "_adcToIRMS_L3: %f, _adcToVRMS_L1: %f, _adcToVRMS_L2: %f, _adcToVRMS_L3: %f, _adcToPower_L1: %f, _adcToPower_L2: %f, _adcToPower_L3: %f",
    //         _adcToI_L1,_adcToI_L2,_adcToI_L3,_adcToV_L1,_adcToV_L2,_adcToV_L3,_adcToIRMS_L1,_adcToIRMS_L2,_adcToIRMS_L3,
    //         _adcToVRMS_L1,_adcToVRMS_L2,_adcToVRMS_L3,_adcToPower_L1,_adcToPower_L2,_adcToPower_L3);

    // TODO:
    // For some reason, according to the calibration xlx, the adcToEnergy is not adcToPower/3600.0
    // However it is not by only a slight margin
    // XLX: 0.000108688956; adcToPower/3600.0 = 0.000106141558889 
}

// _____________________________________________________________________________
void ADE9000_reset(void){
    LOG_DBG("RESET PIN not available in hardware");
    // // Remove interrupt
    // detachInterrupt(digitalPinToInterrupt(_DREADY_PIN));
    // // Reset sequence
    // digitalWrite(_RESET_PIN, LOW);
    // delay(50);
    // digitalWrite(_RESET_PIN, HIGH);
    // delay(1000);
}

// _____________________________________________________________________________
void ADE9000_resetEnergy() {                    // invalid write on read-only registers?
    ADE9000__write_32(ADDR_AWATTHR_HI,0);
    ADE9000__write_32(ADDR_BWATTHR_HI,0);
    ADE9000__write_32(ADDR_CWATTHR_HI,0);
}

// _____________________________________________________________________________
void ADE9000_startSampling(int samplingrate) {
    _samplingrate = samplingrate;
    // Set gain
    ADE9000__write_16(ADDR_PGA_GAIN, 0x15ff); // Gain is 2 for voltage and 4 for current
    uint16_t addr = addr32k;
    if (samplingrate <= 8000) {
        addr = addr8k;
        // DREADY = Sinc4 + IIR LPF output at 8 kSPS.
        ADE9000__write_16(ADDR_WFB_CFG, 0x0200);
    } else {
        // DREADY = Sinc4 + IIR LPF output at 32 kSPS.
        ADE9000__write_16(ADDR_WFB_CFG, 0x0000);
    }
    _burst_read_tx_array[0] = addr >> 8;    // addr [15 - 8]
    _burst_read_tx_array[1] = addr | 0x8;   // addr [7 - 4], read-bit[3]

    // select which function to output on:Here DREADY
    ADE9000__write_16(ADDR_CONFIG1, 0x000C);
    // enable spi burst read
    ADE9000__burst_read_en(true);


    // TODO:
    if (_interruptHandler != NULL) {
        // attachInterrupt(digitalPinToInterrupt(_DREADY_PIN), _interruptHandler, RISING);
    } else if (_interruptHandler2 != NULL) {
        LOG_INF("Init pin interrupt ESP Style\n");
        // const gpio_num_t int_pin = GPIO_NUM_34;
        // // Configure interrupt for low level
        // gpio_config_t config = {
        // .pin_bit_mask = 1ull << int_pin,
        // .mode = GPIO_MODE_INPUT,
        // .pull_up_en = GPIO_PULLUP_ENABLE,
        // .pull_down_en = GPIO_PULLDOWN_DISABLE,
        // .intr_type = GPIO_INTR_DISABLE
        // };
        // ESP_ERROR_CHECK(gpio_config(&config));
        // esp_err_t err;
        
        // err = gpio_set_intr_type(int_pin, GPIO_INTR_POSEDGE);
        // if (err != ESP_OK) {
        // if (_logFunc) _logFunc("set intr type failed with error 0x%x", err);
        // }


        // err = gpio_isr_handler_add(int_pin, _interruptHandler2, (void *) nullptr);
        // if (err != ESP_OK) {
        // if (_logFunc) _logFunc("handler add failed with error 0x%x", err);
        // }
    }
}

// _____________________________________________________________________________
void ADE9000_stopSampling() {
    // Remove interrupt
    if (_interruptHandler != NULL) {
        // detachInterrupt(_DREADY_PIN);
    } else if (_interruptHandler2 != NULL) {
        // const gpio_num_t int_pin = GPIO_NUM_34;
        // gpio_isr_handler_remove(int_pin);
    }
    ADE9000__burst_read_en(false);
}

// _____________________________________________________________________________
void ADE9000_leavoutMeasurement() {
    ADE9000__burst_write_SPI(_burst_read_tx_array, 31);  // is this correct here?
    ADE9000__burst_read_SPI_Buffer(_burst_read_rx_array, 31);
}

// _____________________________________________________________________________
void ADE9000_readRawMeasurement(CurrentADC *adc) {
    // where to read from
    ADE9000__burst_write_SPI(_burst_read_tx_array, 31);  // is this correct here?
    ADE9000__burst_read_SPI_Buffer(_burst_read_rx_array, 31);
    ADE9000__dataBitShift(_burst_read_rx_array, 31);
    // ADE9000__burst_write_SPI(_burst_read_tx_array, 31);  // is this correct here?
    for (uint8_t i = 0; i < 7; i++) {
        adc->adc[i]  = _burst_read_rx_array[ 2+i*4 ] << 24;
        adc->adc[i] |= _burst_read_rx_array[ 3+i*4 ] << 16;
        adc->adc[i] |= _burst_read_rx_array[ 4+i*4 ] << 8;
        adc->adc[i] |= _burst_read_rx_array[ 5+i*4 ];
    }
}

// _____________________________________________________________________________
void ADE9000_convertRawMeasurement(CurrentADC *adc, float *values) {
    // Until here 20us passed
    // Converting takes around 1us
    values[1] = (float)(_adcToI_L1*(float)(int32_t)adc->adc[0]);
    values[3] = (float)(_adcToI_L2*(float)(int32_t)adc->adc[2]);
    values[5] = (float)(_adcToI_L3*(float)(int32_t)adc->adc[4]);
    values[6] = (float)(_adcToI_L1*(float)(int32_t)adc->adc[6]);

    values[0] = (float)(_adcToV_L1*(float)(int32_t)adc->adc[1]);
    values[2] = (float)(_adcToV_L2*(float)(int32_t)adc->adc[3]);
    values[4] = (float)(_adcToV_L3*(float)(int32_t)adc->adc[5]);
}

// _____________________________________________________________________________
void ADE9000_readMeasurement(float *values) {
    // where to read from
    // ADE9000__burst_write_SPI(_burst_read_tx_array, 31);  // is this correct here?
    ADE9000__burst_read_SPI_Buffer(_burst_read_rx_array, 31);
    ADE9000__dataBitShift(_burst_read_rx_array, 31);
    ADE9000__burst_write_SPI(_burst_read_tx_array, 31);  // is this correct here?
    CurrentADC adc;
    for (uint8_t i = 0; i < 7; i++) {
        adc.adc[i]  = _burst_read_rx_array[ 2+i*4 ] << 24;
        adc.adc[i] |= _burst_read_rx_array[ 3+i*4 ] << 16;
        adc.adc[i] |= _burst_read_rx_array[ 4+i*4 ] << 8;
        adc.adc[i] |= _burst_read_rx_array[ 5+i*4 ];
    }
    // Until here 20us passed
    // Converting takes around 1us
    ADE9000_convertRawMeasurement(&adc, values);
    // 8kHz is 125us, 32kHz is ca. 30us
}

// _____________________________________________________________________________
void ADE9000__write_16(uint16_t addr , uint16_t data ) {
	uint8_t tx_array[4] = { 0 };
    addr = addr << 4;
    tx_array[0] = addr >> 8;
    tx_array[1] = addr;
    tx_array[2] = data >> 8;
    tx_array[3] = data;
    // _spi->transferBytes( tx_array, _rx_array, 6);

    SPI_MANUAL_TRANSCEIVE(tx_array, sizeof(tx_array), _rx_array, 6);
}

// _____________________________________________________________________________
void ADE9000__write_32(uint16_t addr , uint32_t data) {
    uint8_t tx_array[6] = { 0 };
    addr = addr << 4;
    tx_array[0] = addr >> 8;
    tx_array[1] = addr;
    tx_array[2] = data >> 24;
    tx_array[3] = data >> 16;
    tx_array[4] = data >> 8;
    tx_array[5] = data;
    // _spi->transferBytes( tx_array, _rx_array, 6);

    SPI_MANUAL_TRANSCEIVE(tx_array, sizeof(tx_array), _rx_array, 6);
}

// _____________________________________________________________________________
uint16_t ADE9000__read_16(uint16_t addr) {
    uint8_t tx_array[5] = { 0 };

    addr = addr << 4;
    tx_array[0] = addr >> 8;
    tx_array[1] = addr | 0x8;

    if (spi_cfg.frequency > SPI_THRES) {
        // _spi->transferBytes(tx_array, _rx_array, 5);
        SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 6);
        ADE9000__dataBitShift(_rx_array, 5);
    } else {
        // _spi->transferBytes(tx_array, _rx_array, 4);
        SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 4);
    }
    uint16_t data = _rx_array[2] << 8 | _rx_array[3];

    return data;
}

// _____________________________________________________________________________
uint32_t ADE9000__read_32(uint16_t addr) {
	uint8_t tx_array[7] = { 0 };

    addr = addr << 4;
    tx_array[0] = addr >> 8;
    tx_array[1] = addr | 0x8;

    if (spi_cfg.frequency > SPI_THRES) {
        // _spi->transferBytes(tx_array, _rx_array, 7);
        SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 7);
        ADE9000__dataBitShift(_rx_array, 7);
    } else {
        // _spi->transferBytes(tx_array, _rx_array, 6);
        SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 6);
    }

    uint32_t data = _rx_array[2] << 24;
    data |= _rx_array[3] << 16;
    data |= _rx_array[4] << 8;
    data |= _rx_array[5];

    return data;
}

// _____________________________________________________________________________
uint32_t ADE9000_readRegister(uint16_t addr, size_t size) {
    if (size == 16) {
        return ADE9000__read_16(addr);
    }
    return ADE9000__read_32(addr);
}

// _____________________________________________________________________________
void ADE9000_writeRegister(uint16_t addr, uint32_t data, size_t size) {
    if (size == 16) {
        ADE9000__write_16(addr, (uint16_t)data);
    } else {
        ADE9000__write_32(addr, data);
    }
}

// _____________________________________________________________________________
void ADE9000__dataBitShift(uint8_t *data, uint16_t size) {
    for (uint16_t i = 0; i < size - 1; i++){
        data[i] = data[i] << 1; // shift for one bit left
        data[i] |= data[i+1] >> 7; // append bit 7 from next byte
    }
    // Shift last byte, LSB will be 0
    data[size-1] = data[size-1] << 1;
}

// _____________________________________________________________________________
uint16_t ADE9000__read_16_CRC(uint16_t addr) {
    uint8_t tx_array[7] = { 0 };

    addr = addr << 4;

    tx_array[0] = addr >> 8;    // addr [15 - 8]
    tx_array[1] = addr | 0x8;   // addr [7 - 4], read-bit[3]

    // _spi->transferBytes( tx_array, _rx_array, 7);
    SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 7);

    if (spi_cfg.frequency > SPI_THRES) ADE9000__dataBitShift(_rx_array, 7);

    uint8_t rx_data[2];
    rx_data[0] = _rx_array[2];
    rx_data[1] = _rx_array[3];

    uint16_t rx_crc = (_rx_array[4] << 8) | _rx_array[5];

    // CRC check
    if (!_burst_en) {
        if ( rx_crc != CRCCCITT(rx_data, 2, 0xffff, 0)) {
            // if (_logFunc) 
                LOG_DBG("CRC ERROR! - RX: 0x%04x calc: 0x%04x", rx_crc, CRCCCITT(rx_data, 2, 0xffff, 0));
        }
    }
    return (_rx_array[2] << 8) | _rx_array[3];
}


// _____________________________________________________________________________
uint32_t ADE9000__read_32_CRC(uint16_t addr) {
    uint8_t tx_array[9] = { 0 };

    addr = addr << 4;

    tx_array[0] = addr >> 8;    // addr [15 - 8]
    tx_array[1] = addr | 0x8;   // addr [7 - 4], read-bit[3]

    // _spi->transferBytes( tx_array, _rx_array, 9);
    SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 9);

    if (spi_cfg.frequency > SPI_THRES) ADE9000__dataBitShift(_rx_array, 9);

    uint8_t rx_data[4];
    rx_data[0] = _rx_array[2];
    rx_data[1] = _rx_array[3];
    rx_data[2] = _rx_array[4];
    rx_data[3] = _rx_array[5];

    uint16_t rx_crc = (_rx_array[6] << 8) | _rx_array[7];

    // CRC check
    if (!_burst_en) {
        if ( rx_crc != CRCCCITT(rx_data, 2, 0xffff, 0)) {
        // if (_logFunc) 
            LOG_DBG("CRC ERROR! - RX: 0x%04x calc: 0x%04x", rx_crc, CRCCCITT(rx_data, 2, 0xffff, 0));
        }
    }
    uint32_t data = _rx_array[2] << 24;
    data |= _rx_array[3] << 16;
    data |= _rx_array[4] << 8;
    data |= _rx_array[5];

    return data;
}


// _____________________________________________________________________________
void ADE9000__write_16_Check(uint16_t addr, uint16_t data){
    ADE9000__write_16(addr, data);
    // verify write operation
    uint16_t read_data = ADE9000__read_16(addr);
    if(read_data != data) {
        // if (_logFunc) {
        LOG_DBG("Error, write: 0x%04x, read: 0x%04x", data, read_data);
        // }
    }
}

// _____________________________________________________________________________
void ADE9000__write_32_Check(uint16_t addr, uint32_t data){
    ADE9000__write_32(addr, data);
    // verify write operation
    uint32_t read_data = ADE9000__read_32(addr);
    if(read_data != data) {
        // if (_logFunc) {
        LOG_DBG("Error, write: 0x%08x, read: 0x%08x", data, read_data);
        // }
    }
}


// _____________________________________________________________________________
void ADE9000__burst_read_en(bool enable){
    uint16_t buffer = ADE9000__read_16(ADDR_CONFIG1);
    if(enable){
        _burst_en = true;
        ADE9000__write_16(ADDR_CONFIG1, buffer | (1 << CONFIG1_BURST_EN) );
    } else {
        ADE9000__write_16(ADDR_CONFIG1, buffer & ~(1 << CONFIG1_BURST_EN) );
        _burst_en = false;
    }
}


// _____________________________________________________________________________
void ADE9000__burst_read(uint16_t addr, uint32_t *data, uint16_t size) {
    // wegen Performancegründen ist es so kompliziert
    uint8_t rx_array[ 2 + (size*4) ];
    uint8_t tx_array[ 2 + (size*4) ];// = { 0 };
    memset(rx_array, 0, sizeof(rx_array));
    memset(tx_array, 0, sizeof(tx_array));

    if (!_burst_en) {
        ADE9000__burst_read_en(true);
    }

    tx_array[0] = addr >> 8;    // addr [15 - 8]
    tx_array[1] = addr | 0x8;   // addr [7 - 4], read-bit[3]

    // _spi->transferBytes( tx_array, rx_array, 2 + size*4);
    SPI_MANUAL_TRANSCEIVE(tx_array, 2, _rx_array, 2 + size*4);

    for (uint8_t i = 0; i < size; i++){
        data[i] = rx_array[ 2+i*4 ] << 24;
        data[i] |= rx_array[ 3+i*4 ] << 16;
        data[i] |= rx_array[ 4+i*4 ] << 8;
        data[i] |= rx_array[ 5+i*4 ];
        LOG_ERR("data[%d]: %d ", i, data[i]);
    }
    LOG_ERR("\n");
}


// _____________________________________________________________________________
void ADE9000__burst_write_SPI(uint8_t *tx_data, uint8_t bytes) {
    if(bytes > 64) bytes = 64;

    // uint32_t words = (bytes + 3) / 4;//16 max

    // uint32_t wordsBuf[16] = {0,};
    // uint8_t * bytesBuf = (uint8_t *) wordsBuf;

    // memcpy(bytesBuf, tx_data, bytes);//copy data to buffer

    // _spi->bus()->dev->mosi_dlen.usr_mosi_dbitlen = ((bytes * 8) - 1);
    // _spi->bus()->dev->miso_dlen.usr_miso_dbitlen = ((bytes * 8) - 1);

    // for(int i =0; i<words; i++) {
    //     _spi->bus()->dev->data_buf[i] = wordsBuf[i];    //copy buffer to spi fifo
    // }

    // _spi->bus()->dev->cmd.usr = 1;

    SPI_MANUAL_WRITE(tx_data, bytes);
}

// _____________________________________________________________________________
void ADE9000__burst_read_SPI_Buffer(uint8_t *rx_data, uint8_t bytes) {

    if(bytes > 64) bytes = 64;

    // uint32_t words = (bytes + 3) / 4;//16 max

    // uint32_t wordsBuf[16] = {0,};
    // uint8_t * bytesBuf = (uint8_t *) wordsBuf;

    // // FIXME dont use "bytes", use (spi->dev->miso_dlen.usr_miso_dbitlen+1) / 8

    // while(_spi->bus()->dev->cmd.usr);

    // for(int i=0; i<words; i++) {
    //     wordsBuf[i] = _spi->bus()->dev->data_buf[i];//copy spi fifo to buffer
    // }
    // memcpy(rx_data, bytesBuf, bytes);//copy buffer to output

    static uint8_t tx_dummy[64] = {0}; // Send zeros to receive data (common SPI pattern)
    SPI_MANUAL_TRANSCEIVE(tx_dummy, bytes, rx_data, bytes);
}



// _____________________________________________________________________________
void ADE9000_readActivePowerRegs(struct ActivePowerRegs *data) {
	data->ActivePowerReg_A = (ADE9000__read_32(ADDR_AWATT));
	data->ActivePowerReg_B = (ADE9000__read_32(ADDR_BWATT));
	data->ActivePowerReg_C = (ADE9000__read_32(ADDR_CWATT));
}

// _____________________________________________________________________________
void ADE9000_convertActivePowerRegs(struct ActivePowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->ActivePowerReg_A;
    values[1] = _adcToPower_L2*(float)data->ActivePowerReg_B;
    values[2] = _adcToPower_L3*(float)data->ActivePowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readActivePower(float *values) {
    struct ActivePowerRegs data;
    ADE9000_readActivePowerRegs(&data);
    ADE9000_convertActivePowerRegs(&data, values);
}


// _____________________________________________________________________________
void ADE9000_readReactivePowerRegs(struct ReactivePowerRegs *data) {
	data->ReactivePowerReg_A = (ADE9000__read_32(ADDR_AVAR));
	data->ReactivePowerReg_B = (ADE9000__read_32(ADDR_BVAR));
	data->ReactivePowerReg_C = (ADE9000__read_32(ADDR_CVAR));	
}

// _____________________________________________________________________________
void ADE9000_convertReactivePowerRegs(struct ReactivePowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->ReactivePowerReg_A;
    values[1] = _adcToPower_L2*(float)data->ReactivePowerReg_B;
    values[2] = _adcToPower_L3*(float)data->ReactivePowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readReactivePower(float *values) {
    struct ReactivePowerRegs data;
    ADE9000_readReactivePowerRegs(&data);
    ADE9000_convertReactivePowerRegs(&data, values);
}


// _____________________________________________________________________________
void ADE9000_readApparentPowerRegs(struct ApparentPowerRegs *data) {
	data->ApparentPowerReg_A = (ADE9000__read_32(ADDR_AVA));
	data->ApparentPowerReg_B = (ADE9000__read_32(ADDR_BVA));
	data->ApparentPowerReg_C = (ADE9000__read_32(ADDR_CVA));	
}

// _____________________________________________________________________________
void ADE9000_convertApparentPowerRegs(struct ApparentPowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->ApparentPowerReg_A;
    values[1] = _adcToPower_L2*(float)data->ApparentPowerReg_B;
    values[2] = _adcToPower_L3*(float)data->ApparentPowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readApparentPower(float *values) {
    struct ApparentPowerRegs data;
    ADE9000_readApparentPowerRegs(&data);
    ADE9000_convertApparentPowerRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readVoltageRMSRegs(struct VoltageRMSRegs *data) {
	data->VoltageRMSReg_A = (ADE9000__read_32(ADDR_AVRMS));
	data->VoltageRMSReg_B = (ADE9000__read_32(ADDR_BVRMS));
	data->VoltageRMSReg_C = (ADE9000__read_32(ADDR_CVRMS));	
}

// _____________________________________________________________________________
void ADE9000_convertVoltageRMSRegs(struct VoltageRMSRegs *data, float *values) {
    values[0] = _adcToVRMS_L1*(float)data->VoltageRMSReg_A;
    values[1] = _adcToVRMS_L2*(float)data->VoltageRMSReg_B;
    values[2] = _adcToVRMS_L3*(float)data->VoltageRMSReg_C;
}

// _____________________________________________________________________________
void ADE9000_readVoltageRMS(float *values) {
    struct VoltageRMSRegs data;
    ADE9000_readVoltageRMSRegs(&data);
    ADE9000_convertVoltageRMSRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readCurrentRMSRegs(struct CurrentRMSRegs *data) {
	data->CurrentRMSReg_A = (ADE9000__read_32(ADDR_AIRMS));
	data->CurrentRMSReg_B = (ADE9000__read_32(ADDR_BIRMS));
	data->CurrentRMSReg_C = (ADE9000__read_32(ADDR_CIRMS));
	data->CurrentRMSReg_N = (ADE9000__read_32(ADDR_NIRMS));
}

// _____________________________________________________________________________
void ADE9000_convertCurrentRMSRegs(struct CurrentRMSRegs *data, float *values) {
    values[0] = _adcToIRMS_L1*(float)data->CurrentRMSReg_A;
    values[1] = _adcToIRMS_L2*(float)data->CurrentRMSReg_B;
    values[2] = _adcToIRMS_L3*(float)data->CurrentRMSReg_C;
    // TODO: this is nasty....
    values[3] = _adcToIRMS_L1*(float)data->CurrentRMSReg_N;
}

// _____________________________________________________________________________
void ADE9000_readCurrentRMS(float *values) {
    struct CurrentRMSRegs data;
    ADE9000_readCurrentRMSRegs(&data);
    ADE9000_convertCurrentRMSRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readFundActivePowerRegs(struct FundActivePowerRegs *data) {
	data->FundActivePowerReg_A = (ADE9000__read_32(ADDR_AFWATT));
	data->FundActivePowerReg_B = (ADE9000__read_32(ADDR_BFWATT));
	data->FundActivePowerReg_C = (ADE9000__read_32(ADDR_CFWATT));	
}

// _____________________________________________________________________________
void ADE9000_convertFundActivePowerRegs(struct FundActivePowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->FundActivePowerReg_A;
    values[1] = _adcToPower_L2*(float)data->FundActivePowerReg_B;
    values[2] = _adcToPower_L3*(float)data->FundActivePowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readFundActivePower(float *values) {
    struct FundActivePowerRegs data;
    ADE9000_readFundActivePowerRegs(&data);
    ADE9000_convertFundActivePowerRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readFundReactivePowerRegs(struct FundReactivePowerRegs *data) {
	data->FundReactivePowerReg_A = (ADE9000__read_32(ADDR_AFVAR));
	data->FundReactivePowerReg_B = (ADE9000__read_32(ADDR_BFVAR));
	data->FundReactivePowerReg_C = (ADE9000__read_32(ADDR_CFVAR));	
}

// _____________________________________________________________________________
void ADE9000_convertFundReactivePowerRegs(struct FundReactivePowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->FundReactivePowerReg_A;
    values[1] = _adcToPower_L2*(float)data->FundReactivePowerReg_B;
    values[2] = _adcToPower_L3*(float)data->FundReactivePowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readFundReactivePower(float *values) {
    struct FundReactivePowerRegs data;
    ADE9000_readFundReactivePowerRegs(&data);
    ADE9000_convertFundReactivePowerRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readFundApparentPowerRegs(struct FundApparentPowerRegs *data) {
	data->FundApparentPowerReg_A = (ADE9000__read_32(ADDR_AFVA));
	data->FundApparentPowerReg_B = (ADE9000__read_32(ADDR_BFVA));
	data->FundApparentPowerReg_C = (ADE9000__read_32(ADDR_CFVA));	
}

// _____________________________________________________________________________
void ADE9000_convertFundApparentPowerRegs(struct FundApparentPowerRegs *data, float *values) {
    values[0] = _adcToPower_L1*(float)data->FundApparentPowerReg_A;
    values[1] = _adcToPower_L2*(float)data->FundApparentPowerReg_B;
    values[2] = _adcToPower_L3*(float)data->FundApparentPowerReg_C;
}

// _____________________________________________________________________________
void ADE9000_readFundApparentPower(float *values) {
    struct FundApparentPowerRegs data;
    ADE9000_readFundApparentPowerRegs(&data);
    ADE9000_convertFundApparentPowerRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readFundVoltageRMSRegs(struct FundVoltageRMSRegs *data) {
	data->FundVoltageRMSReg_A = (ADE9000__read_32(ADDR_AVFRMS));
	data->FundVoltageRMSReg_B = (ADE9000__read_32(ADDR_BVFRMS));
	data->FundVoltageRMSReg_C = (ADE9000__read_32(ADDR_CVFRMS));	
}

// _____________________________________________________________________________
void ADE9000_convertFundVoltageRMSRegs(struct FundVoltageRMSRegs *data, float *values) {
    values[0] = _adcToVRMS_L1*(float)data->FundVoltageRMSReg_A;
    values[1] = _adcToVRMS_L2*(float)data->FundVoltageRMSReg_B;
    values[2] = _adcToVRMS_L3*(float)data->FundVoltageRMSReg_C;
}

// _____________________________________________________________________________
void ADE9000_readFundVoltageRMS(float *values) {
    struct FundVoltageRMSRegs data;
    ADE9000_readFundVoltageRMSRegs(&data);
    ADE9000_convertFundVoltageRMSRegs(&data, values);
}

// _____________________________________________________________________________
void ADE9000_readFundCurrentRMSRegs(struct FundCurrentRMSRegs *data) {
    data->FundCurrentRMSReg_A = (ADE9000__read_32(ADDR_AIFRMS));
    data->FundCurrentRMSReg_B = (ADE9000__read_32(ADDR_BIFRMS));
    data->FundCurrentRMSReg_C = (ADE9000__read_32(ADDR_CIFRMS));	
}

// _____________________________________________________________________________
void ADE9000_convertFundCurrentRMSRegs(struct FundCurrentRMSRegs *data, float *values) {
    values[0] = _adcToIRMS_L1*(float)data->FundCurrentRMSReg_A;
    values[1] = _adcToIRMS_L2*(float)data->FundCurrentRMSReg_B;
    values[2] = _adcToIRMS_L3*(float)data->FundCurrentRMSReg_C;
}

// _____________________________________________________________________________
void ADE9000_readFundCurrentRMS(float *values) {
    struct FundCurrentRMSRegs data;
    ADE9000_readFundCurrentRMSRegs(&data);
    ADE9000_convertFundCurrentRMSRegs(&data, values);
}

// _____________________________________________________________________________
// K_W is your power scaling (codes -> Watts), e.g. _adcToPower_Lx
// EGY_TIME is the register value, i.e., 7999 for 1 s @ 8ksps
// Wh = HI * K_W * 8192 / (EGY_TIME + 1) / 3600
void ADE9000_readActiveEnergy(double *values) {
    const double k = (8192.0) / ((double)ADE9000_EGY_TIME + 1.0);

    uint32_t high = (ADE9000__read_32(ADDR_AWATTHR_HI));
    uint32_t low = (ADE9000__read_32(ADDR_AWATTHR_LO));
    uint64_t EnergyCodes = ( (uint64_t)high << 13 ) | (low & 0x1FFF );
    // _write_32(ADDR_AWATTHR_HI,0); // Reset is done via config 
    values[0] = (double)((EnergyCodes*_adcToPower_L1*k)/3600.0f);
    
    high = (ADE9000__read_32(ADDR_BWATTHR_HI));
    low = (ADE9000__read_32(ADDR_BWATTHR_LO));
    EnergyCodes = ( (uint64_t)high << 13 ) | (low & 0x1FFF );
    values[1] = (double)((EnergyCodes*_adcToPower_L2*k)/3600.0f);

    high = (ADE9000__read_32(ADDR_CWATTHR_HI));
    low = (ADE9000__read_32(ADDR_CWATTHR_LO));
    EnergyCodes = ( (uint64_t)high << 13 ) | (low & 0x1FFF );
    values[2] = (double)((EnergyCodes*_adcToPower_L3*k)/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readReactiveEnergy(double *values) {
    uint32_t high = (ADE9000__read_32(ADDR_AVARHR_HI));
    // _write_32(ADDR_AVARHR_HI,0); // Reset is done via config 
    values[0] = (double)(high*_adcToPower_L1/3600.0f);
    high = (ADE9000__read_32(ADDR_BVARHR_HI));
    values[1] = (double)(high*_adcToPower_L2/3600.0f);
    high = (ADE9000__read_32(ADDR_CVARHR_HI));
    values[2] = (double)(high*_adcToPower_L3/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readApparentEnergy(double *values) {
    uint32_t high = (ADE9000__read_32(ADDR_AVAHR_HI));
    // _write_32(ADDR_AVAHR_HI,0); // Reset is done via config 
    values[0] = (double)(high*_adcToPower_L1/3600.0f);
    high = (ADE9000__read_32(ADDR_BVAHR_HI));
    values[1] = (double)(high*_adcToPower_L2/3600.0f);
    high = (ADE9000__read_32(ADDR_CVAHR_HI));
    values[2] = (double)(high*_adcToPower_L3/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readFundActiveEnergy(double *values) {
    uint32_t high = (ADE9000__read_32(ADDR_AFWATTHR_HI));
    // _write_32(ADDR_AFWATTHR_HI,0); // Reset is done via config 
    values[0] = (double)(high*_adcToPower_L1/3600.0f);
    high = (ADE9000__read_32(ADDR_BFWATTHR_HI));
    values[1] = (double)(high*_adcToPower_L2/3600.0f);
    high = (ADE9000__read_32(ADDR_CFWATTHR_HI));
    values[2] = (double)(high*_adcToPower_L3/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readFundReactiveEnergy(double *values) {
    uint32_t high = (ADE9000__read_32(ADDR_AFVARHR_HI));
    // _write_32(ADDR_AFVARHR_HI,0); // Reset is done via config 
    values[0] = (double)(high*_adcToPower_L1/3600.0f);
    high = (ADE9000__read_32(ADDR_BFVARHR_HI));
    values[1] = (double)(high*_adcToPower_L2/3600.0f);
    high = (ADE9000__read_32(ADDR_CFVARHR_HI));
    values[2] = (double)(high*_adcToPower_L3/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readFundApparentEnergy(double *values) {
    uint32_t high = (ADE9000__read_32(ADDR_AFVAHR_HI));
    // _write_32(ADDR_AFVAHR_HI,0); // Reset is done via config 
    values[0] = (double)(high*_adcToPower_L1/3600.0f);
    high = (ADE9000__read_32(ADDR_BFVAHR_HI));
    values[1] = (double)(high*_adcToPower_L2/3600.0f);
    high = (ADE9000__read_32(ADDR_CFVAHR_HI));
    values[2] = (double)(high*_adcToPower_L3/3600.0f);
}

// _____________________________________________________________________________
void ADE9000_readLinePeriod(float *values) {
    uint32_t high = (ADE9000__read_32(ADDR_APERIOD));
    values[0] = 1.0/((high+1)/(8000.0*65536.0));
    high = (ADE9000__read_32(ADDR_BPERIOD));
    values[1] = 1.0/((high+1)/(8000.0*65536.0));
    high = (ADE9000__read_32(ADDR_CPERIOD));
    values[2] = 1.0/((high+1)/(8000.0*65536.0));
}

// _____________________________________________________________________________
void ADE9000_readPhaseAngle(float *values) {
    uint16_t high = (ADE9000__read_16(ADDR_ANGL_VA_IA));
    float consta = 0.017578125;
    if (netfreq == 60) consta = 0.02109375;
    values[0] = high*consta;
    high = (ADE9000__read_16(ADDR_ANGL_VB_IB));
    values[1] = high*consta;
    high = (ADE9000__read_16(ADDR_ANGL_VC_IC));
    values[2] = high*consta;
}

static inline bool ade_energy_ready(void)
{
    uint32_t s = ADE9000__read_32(ADDR_STATUS0);
    return (s & (ADE9000_STATUS0_EGYRDY)) != 0;     // use the proper bit mask
}

void print_phase_data(struct PhaseBurstData_t* data, uint8_t phase)
{
    LOG_DBG("=== Phase %d ===", phase);
    for (int var = 0; var < PHASE_VAR_COUNT; var++)
    {
        LOG_DBG("%-30s : %u", phaseVarNames[var], phaseData[phase].value[var]);
    }
    LOG_DBG("\n");
}

void ade9000_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    LOG_ERR("Worker triggerred.");

    // read phase data in burst mode here
    ADE9000__burst_read(ADDR_AV_PCF_2, phaseData[PHASE_A].value, PHASE_VAR_COUNT*3);
    // ADE9000__burst_read(ADDR_BV_PCF_2, phaseData[PHASE_B].value, PHASE_VAR_COUNT);
    // ADE9000__burst_read(ADDR_CV_PCF_2, phaseData[PHASE_C].value, PHASE_VAR_COUNT);

    // print_phase_data(&phaseData[PHASE_A], PHASE_A);
    // print_phase_data(&phaseData[PHASE_B], PHASE_B);
    // print_phase_data(&phaseData[PHASE_C], PHASE_C);

    // Reschedule
    k_work_reschedule(&_ade9000_worker, K_MSEC(ADE_READ_INTERVAL)); // Poll every 500ms
}

void init_ade9000(const struct device* ade9000_dev)
{
    _ade9000 = ade9000_dev;

    if ((_ade9000 == NULL)) {
    // if ((ade9000.spi_dev == NULL)) {
      LOG_ERR("SPI device not found. _ade9000: %p", _ade9000);//.spi_dev);
      return;
    }

    configure_ade9000_cs_pin();

    
    if(ADE9000_init(INPUT_FREQUENCY) < 0)
    {
        LOG_ERR("ADE9000 initialization error.");
        return;
    }
    
    ADE9000_startSampling(8000);         // need to tweak this function
    
    float calibration_factors[] = { // voltage, current
                                        1.03,    0.5,        // Phase A
                                        1.03,    0.5,        // Phase B
                                        1.03,    0.5         // Phase C
                                  };
    ADE9000_setCalibration(calibration_factors);
    ADE9000_calcConstants();

    // CalibrationData_t calData = {232.6, 11.44};      // input the calibration voltage and current provided on terminals
    // run_rms_calibration_once(&calData, PHASE_A);



    // // creating seperate work queue to isolate this thread from blocking other worker threads
    // k_work_queue_start(&_ade9000_work_q, ade_stack_area, K_THREAD_STACK_SIZEOF(ade_stack_area), ADE_WQ_PRIORITY, NULL);
    
    // k_work_init_delayable(&_ade9000_worker, ade9000_work_handler);
    // k_work_schedule_for_queue(&_ade9000_work_q, &_ade9000_worker, K_MSEC(ADE_READ_INTERVAL));

    float iRMS[4], vRMS[3], activPOW[3], reactivPOW[3], apparentPOW[3], activENERGY[3], reactivENERGY[3], apparentENERGY[3], period[3], phase[3] = {0.0};

    while(1)
    {
        static bool energyRead = false;
        static int64_t last_log = 0;
        int64_t now = k_uptime_get();
        
        ADE9000_readCurrentRMS(iRMS);
        ADE9000_readVoltageRMS(vRMS);
        ADE9000_readActivePower(activPOW);
        ADE9000_readReactivePower(reactivPOW);
        ADE9000_readApparentPower(apparentPOW);

        if(ade_energy_ready())
        {
            ADE9000_readActiveEnergy(activENERGY);
            ADE9000_readReactiveEnergy(reactivENERGY);
            ADE9000_readApparentEnergy(apparentENERGY);
            energyRead = true;
        }
        
        ADE9000_readLinePeriod(period);
        ADE9000_readPhaseAngle(phase);

        if(now - last_log >= 500)
        {
            last_log = now;
            LOG_INF("ADE Data\n\t\t\t\t==========================");
            LOG_INF("vRMS - L1: %.2fV(%.2fHz)(%.2f°)\tL2: %.2fV(%.2fHz)(%.2f°)\tL3: %.2fV(%.2fHz)(%.2f°)", (double)vRMS[PHASE_A], (double)period[PHASE_A], (double)phase[PHASE_A], (double)vRMS[PHASE_B], (double)period[PHASE_B], (double)phase[PHASE_B], (double)vRMS[PHASE_C], (double)period[PHASE_C], (double)phase[PHASE_C]);
            LOG_INF("iRMS - L1: %.2fmA\tL2: %.2fmA\tL3: %.2fmA\tN: %.2fmA", (double)iRMS[PHASE_A], (double)iRMS[PHASE_B], (double)iRMS[PHASE_C], (double)iRMS[3]);

            LOG_INF("activPower - L1: %.2f Watt \tL2: %.2f Watt \tL3: %.2f Watt ", (double)activPOW[PHASE_A], (double)activPOW[PHASE_B], (double)activPOW[PHASE_C]);
            LOG_INF("reactivPower - L1: %.2f VAR \tL2: %.2f VAR \tL3: %.2f VAR ", (double)reactivPOW[PHASE_A], (double)reactivPOW[PHASE_B], (double)reactivPOW[PHASE_C]);
            LOG_INF("apparentPower - L1: %.2f VA \tL2: %.2f VA \tL3: %.2f VA", (double)apparentPOW[PHASE_A], (double)apparentPOW[PHASE_B], (double)apparentPOW[PHASE_C]);
            
            if(energyRead)
            {
                LOG_INF("activEnergy - L1: %.2f Watt-hr\tL2: %.2f Watt-hr\tL3: %.2f Watt-hr", (double)activENERGY[PHASE_A], (double)activENERGY[PHASE_B], (double)activENERGY[PHASE_C]);
                LOG_INF("reactivEnergy - L1: %.2f VAR-hr\tL2: %.2f VAR-hr\tL3: %.2f VAR-hr", (double)reactivENERGY[PHASE_A], (double)reactivENERGY[PHASE_B], (double)reactivENERGY[PHASE_C]);
                LOG_INF("apparentEnergy - L1: %.2f VA-hr\tL2: %.2f VA-hr\tL3: %.2f VA-hr", (double)apparentENERGY[PHASE_A], (double)apparentENERGY[PHASE_B], (double)apparentENERGY[PHASE_C]);
                energyRead = false;
            }
            LOG_INF("\n");
        }

        k_sleep(K_MSEC(10));
    }

    LOG_DBG("ADE9000 power ic initialized");
}