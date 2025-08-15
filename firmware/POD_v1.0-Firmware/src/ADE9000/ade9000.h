#ifndef ADE9000_H_
#define ADE9000_H_

#include <defines.h>
#include <drivers/ADE9000/ade9000_register.h>
#include <drivers/ADE9000/ADE9000Calibration.h>

#include <zephyr/drivers/spi.h>


extern const struct device *_ade9000;
extern const struct device *_ade9000_cs_gpio_dev;
extern struct k_work_delayable _ade9000_worker;

#define CS_PIN                      POWER_METER_IC_CS
#define CS_PORT                     POWER_METER_IC_GPIO_PORT



// /****************************************************************************************************************
//  Structures and Global Variables
// ****************************************************************************************************************/

// Struct of an ADC reading
typedef struct{
  int32_t adc[7];// = { 0 };
} CurrentADC;

struct CurrentRMSRegs
{
    int32_t CurrentRMSReg_A;
    int32_t CurrentRMSReg_B;
    int32_t CurrentRMSReg_C;
    int32_t CurrentRMSReg_N;
};

struct VoltageRMSRegs
{
    int32_t VoltageRMSReg_A;
    int32_t VoltageRMSReg_B;
    int32_t VoltageRMSReg_C;
};

struct FundCurrentRMSRegs
{
    int32_t FundCurrentRMSReg_A;
    int32_t FundCurrentRMSReg_B;
    int32_t FundCurrentRMSReg_C;
};

struct FundVoltageRMSRegs
{
    int32_t FundVoltageRMSReg_A;
    int32_t FundVoltageRMSReg_B;
    int32_t FundVoltageRMSReg_C;
};

struct ActivePowerRegs
{
    int32_t ActivePowerReg_A;
    int32_t ActivePowerReg_B;
    int32_t ActivePowerReg_C;
};
 
struct ReactivePowerRegs
{
    int32_t ReactivePowerReg_A;
    int32_t ReactivePowerReg_B;
    int32_t ReactivePowerReg_C;
};
 
struct ApparentPowerRegs
{
    int32_t ApparentPowerReg_A;
    int32_t ApparentPowerReg_B;
    int32_t ApparentPowerReg_C;
};

struct FundActivePowerRegs
{
    int32_t FundActivePowerReg_A;
    int32_t FundActivePowerReg_B;
    int32_t FundActivePowerReg_C;
}; 

struct FundReactivePowerRegs
{
    int32_t FundReactivePowerReg_A;
    int32_t FundReactivePowerReg_B;
    int32_t FundReactivePowerReg_C;
}; 

struct FundApparentPowerRegs
{
    int32_t FundApparentPowerReg_A;
    int32_t FundApparentPowerReg_B;
    int32_t FundApparentPowerReg_C;
};


struct Energy {
    double active; 
    double fundamental;
    double reactive; 
    double apparent; 
    uint32_t validMillis;  
};

enum PhaseVarIndex_t
{
    PHASE_VAR_INSTANTANEOUS_VOLTAGE,
    PHASE_VAR_INSTANTANEOUS_CURRENT,
    PHASE_VAR_RMS_CURRENT,
    PHASE_VAR_RMS_VOLTAGE,
    PHASE_VAR_TOTAL_ACTIVE_POWER,
    PHASE_VAR_TOTAL_APPARENT_POWER,
    PHASE_VAR_TOTAL_REACTIVE_POWER,
    PHASE_VAR_FUNDAMENTAL_REACTIVE_POWER,
    PHASE_VAR_POWER_FACTOR,
    PHASE_VAR_VOLTAGE_THD,
    PHASE_VAR_CURRENT_THD,
    PHASE_VAR_FUNDAMENTAL_ACTIVE_POWER,
    PHASE_VAR_FUNDAMENTAL_APPARENT_POWER,
    PHASE_VAR_FUNDAMENTAL_CURRENT_RMS,
    PHASE_VAR_FUNDAMENTAL_VOLTAGE_RMS,
    PHASE_VAR_CURRENT_FAST_RMS,
    PHASE_VAR_VOLTAGE_FAST_RMS,
    PHASE_VAR_CURRENT_FAST10_RMS,
    PHASE_VAR_VOLTAGE_FAST10_RMS,

    PHASE_VAR_COUNT // total number of variables
};

struct PhaseBurstData_t
{
    uint32_t value[PHASE_VAR_COUNT];
};

/*------Structure definition for STATUS0 register ADE9000--------------*/
/*  Row:     0                                                            */
/*  Address: 0x402                                                        */
/*  Name:    STATUS0[31:0]                                            */
/*  Read(R) Write(W) Latch(L): RW                                         */
/*  Default: 0x00                                                      */
typedef union {
    struct {
        unsigned EGYRDY : 1;
        unsigned REVAPA : 1;
        unsigned REVAPB : 1;
        unsigned REVAPC : 1;
        unsigned REVRPA : 1;
        unsigned REVRPB : 1;
        unsigned REVRPC : 1;
        unsigned REVPSUM1: 1;
        unsigned REVPSUM2: 1;
        unsigned REVPSUM3: 1;
        unsigned REVPSUM4: 1;
        unsigned CF1: 1;
        unsigned CF2: 1;
        unsigned CF3: 1;
        unsigned CF4: 1;
        unsigned DREADY: 1;
        unsigned WFB_TRIG_IRQ: 1;
        unsigned PAGE_FULL: 1;
        unsigned PWRRDY: 1;
        unsigned RMSONERDY: 1;
        unsigned RMS1012RDY: 1;
        unsigned THD_PF_RDY: 1;
        unsigned WFB_TRIG: 1;
        unsigned COH_WFB_FULL: 1;
        unsigned MISMTCH: 1;
        unsigned TEMP_RDY: 1;
    };
    struct {
        unsigned LSW: 16;
        unsigned MSW: 16;
    };
} STATUS0_Reg;


// class ADE9000 {
  // Threshold for bit shift at SPI speed
  #define SPI_THRES 15000000
  #define SPI_SPEED 5000000
  /****************************************************************************************************************
   Definitions
  ****************************************************************************************************************/
  /*Configuration registers*/
  #define ADE9000_PGA_GAIN 0x00//0x15ff    	    /*PGA@0x0000. Gain of all channels=1*/
  #define ADE9000_CONFIG0 0x00000000		/*Integrator disabled*/
  #define ADE9000_CONFIG1	0x0002			/*CF3/ZX pin outputs Zero crossing */
  #define ADE9000_CONFIG2	0x0C00			/*Default High pass corner frequency of 1.25Hz*/
  #define ADE9000_CONFIG3	0x0000			/*Peak and overcurrent detection disabled*/
  #define ADE9000_ACCMODE 0x0000			/*60Hz operation, 3P4W Wye configuration, signed accumulation*/
                      /*Clear bit 8 i.e. ACCMODE=0x00xx for 50Hz operation*/
  #define ADE9000_ACCMODE_60 0x0100			/*60Hz operation, 3P4W Wye configuration, signed accumulation*/
                      /*Clear bit 8 i.e. ACCMODE=0x00xx for 50Hz operation*/
                      /*ACCMODE=0x0x9x for 3Wire delta when phase B is used as reference*/	
  #define ADE9000_TEMP_CFG 0x000F//0x000C			/*Temperature sensor enabled*/
  #define ADE9000_ZX_LP_SEL 0x001E		/*Line period and zero crossing obtained from combined signals VA,VB and VC*/	
  #define ADE9000_MASK0 (ADE9000_MASK0_EGYRDY)	    /*Enable EGYRDY interrupt*/
//   #define ADE9000_MASK0 0x00000001		/*Enable EGYRDY interrupt*/				
  #define ADE9000_MASK1 0x00000000		/*MASK1 interrupts disabled*/
  #define ADE9000_EVENT_MASK 0x00000000	/*Events disabled */
  #define ADE9000_VLEVEL	0x0022EA28		/*Assuming Vnom=1/2 of full scale. */
                      /*Refer Technical reference manual for detailed calculations.*/
  #define ADE9000_DICOEFF 0x00000000 		/* Set DICOEFF= 0xFFFFE000 when integrator is enabled*/

  /*Constant Definitions***/
  #define ADE90xx_FDSP 8000   			/*ADE9000 FDSP: 8000sps, ADE9078 FDSP: 4000sps*/
  #define ADE9000_RUN_ON 0x0001			/*DSP ON*/
  /*Energy Accumulation Settings*/
  // #define ADE9000_EP_CFG 0x0011			/*Enable energy accumulation, accumulate samples at 8ksps*/
  //                     /*latch energy accumulation after EGYRDY*/
  //                     /*If accumulation is changed to half line cycle mode, change EGY_TIME*/
  #define ADE9000_EP_CFG    (ADE9000_EGY_PWR_EN | ADE9000_EGY_LD_ACCUM | ADE9000_NOLOAD_TMR)//0x0021			/*Enable energy accumulation, accumulate samples at 8ksps*/
                      /*read with reset feature*/
                      /*add to current energy*/
  #define ADE9000_EGY_TIME 0x1F3F 				/*Accumulate 8000 samples*/

  /*Waveform buffer Settings*/
  #define ADE9000_WFB_CFG 0x1000			/*Neutral current samples enabled, Resampled data enabled*/
                      /*Burst all channels*/
  
  /*Full scale Codes referred from Datasheet.Respective digital codes are produced when ADC inputs are at full scale. Donot Change. */
  /*defined in ADE9000Calibration.h*/
  
  /*
  #define ADC_TO_V_8k 0.000013345943038
  #define ADC_TO_V_32k 0.000014822423139
  #define ADC_TO_mA_8k 0.001704837892947
  #define ADC_TO_mA_32k 0.001893446462382
  #define ADC_TO_URMS 0.000026691885979
  #define ADC_TO_IRMS 0.003409675773458
  #define ADC_TO_P 0.003053811549167
  */

//   public:
    // Constructor with reset dready or cf4 pin and spi bus (VSPI or HSPI)
    int8_t ADE9000_init(int fNet);  


    void ADE9000_calcConstants();

    void setCalibration(float *values);
    // Reset the ADE9000
    void ADE9000_reset(void);
    void ADE9000_resetEnergy();

    // Start sampling the ADE9000
    void ADE9000_startSampling(int frequency);

    // Stop sampling
    void ADE9000_stopSampling();

    // Read all measurements and store them in the passed values array
    // NOTE: Make sure values is an array with 7 floats
    void ADE9000_readMeasurement(float *values);
    
    void ADE9000_leavoutMeasurement();

    // Read all measurements in raw format and store them in the passed adc struct
    // NOTE: Make sure that passed adc is a CurrentADC struct
    void ADE9000_readRawMeasurement(CurrentADC *adc);

    // Convert the raw adc measurement into floats
    // NOTE: Make sure values is an array with 7 floats
    void ADE9000_convertRawMeasurement(CurrentADC *adc, float *values);

    void ADE9000_readActivePowerRegs(struct ActivePowerRegs *data);
    void ADE9000_convertActivePowerRegs(struct ActivePowerRegs *data, float *values);
    void ADE9000_readActivePower(float *values);

    void ADE9000_readReactivePowerRegs(struct ReactivePowerRegs *data);
    void ADE9000_convertReactivePowerRegs(struct ReactivePowerRegs *data, float *values);
    void ADE9000_readReactivePower(float *values);

    void ADE9000_readApparentPowerRegs(struct ApparentPowerRegs *data);
    void ADE9000_convertApparentPowerRegs(struct ApparentPowerRegs *data, float *values);
    void ADE9000_readApparentPower(float *values);

    void ADE9000_readVoltageRMSRegs(struct VoltageRMSRegs *data);
    void ADE9000_convertVoltageRMSRegs(struct VoltageRMSRegs *data, float *values);
    void ADE9000_readVoltageRMS(float *values);

    void ADE9000_readCurrentRMSRegs(struct CurrentRMSRegs *data);
    void ADE9000_convertCurrentRMSRegs(struct CurrentRMSRegs *data, float *values);
    void ADE9000_readCurrentRMS(float *values);

    void ADE9000_readFundActivePowerRegs(struct FundActivePowerRegs *data);
    void ADE9000_convertFundActivePowerRegs(struct FundActivePowerRegs *data, float *values);
    void ADE9000_readFundActivePower(float *values);

    void ADE9000_readFundReactivePowerRegs(struct FundReactivePowerRegs *data);
    void ADE9000_convertFundReactivePowerRegs(struct FundReactivePowerRegs *data, float *values);
    void ADE9000_readFundReactivePower(float *values);

    void ADE9000_readFundApparentPowerRegs(struct FundApparentPowerRegs *data);
    void ADE9000_convertFundApparentPowerRegs(struct FundApparentPowerRegs *data, float *values);
    void ADE9000_readFundApparentPower(float *values);

    void ADE9000_readFundVoltageRMSRegs(struct FundVoltageRMSRegs *data);
    void ADE9000_convertFundVoltageRMSRegs(struct FundVoltageRMSRegs *data, float *values);
    void ADE9000_readFundVoltageRMS(float *values);

    void ADE9000_readFundCurrentRMSRegs(struct FundCurrentRMSRegs *data);
    void ADE9000_convertFundCurrentRMSRegs(struct FundCurrentRMSRegs *data, float *values);
    void ADE9000_readFundCurrentRMS(float *values);

    // void updateEnergy();
    uint32_t ADE9000_readRegister(uint16_t addr, size_t size);
    void ADE9000_writeRegister(uint16_t addr, uint32_t data, size_t size);

    void ADE9000_readActiveEnergy(double *values);
    void ADE9000_readReactiveEnergy(double *values);
    void ADE9000_readApparentEnergy(double *values);
    void ADE9000_readFundActiveEnergy(double *values);
    void ADE9000_readFundReactiveEnergy(double *values);
    void ADE9000_readFundApparentEnergy(double *values);

    void ADE9000_readLinePeriod(float *values);
    void ADE9000_readPhaseAngle(float *values);

//     void (*_logFunc)(const char * msg, ...);

//   private:
    // If spi speed exceeds limit, the data is shifted by one bit (slower)
    void ADE9000__dataBitShift(uint8_t *data, uint16_t size);

    uint16_t ADE9000__read_16(uint16_t addr);
    uint32_t ADE9000__read_32(uint16_t addr);
    uint16_t ADE9000__read_16_CRC(uint16_t addr);
    uint32_t ADE9000__read_32_CRC(uint16_t addr);

    void ADE9000__write_16(uint16_t addr, uint16_t data);
    void ADE9000__write_32(uint16_t addr, uint32_t data);
    void ADE9000__write_16_Check(uint16_t addr, uint16_t data);
    void ADE9000__write_32_Check(uint16_t addr, uint32_t data);
    

    void ADE9000__burst_read_en(bool enable);
    void ADE9000__burst_read(uint16_t addr, uint32_t *data, uint16_t size);

    // Unfortunately the "RX-Buffer-Full-interrupt" of the ESP-SPI does not work
    // as expected, therefore the SPI bus must be accessed close to the hardware:
    // write Data on SPI out
    void ADE9000__burst_write_SPI(uint8_t *tx_data, uint8_t bytes);

    // read RX Buffer. Returns "False" if the SPI is still busy.
    void ADE9000__burst_read_SPI_Buffer(uint8_t *rx_data, uint8_t bytes);

    // // Class members
    // const uint16_t addr32k = 0x5000;  // 32k Register @ addr 0x500(0)
    // const uint16_t addr8k = 0x5100;  // 8k Register @ addr 0x510(0)
// };

bool ade_energy_ready(void);

void run_rms_calibration_once(CalibrationData_t* calib_Data, enum PHASES_e phaseSel);
void get_PGA_gain(uint8_t *currentGain, uint8_t *voltageGain);

void init_ade9000(const struct device* ade9000_dev);


#endif  // ADE9000_H_
