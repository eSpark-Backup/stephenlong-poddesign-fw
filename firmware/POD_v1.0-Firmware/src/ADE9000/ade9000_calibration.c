#include "ade9000.h"

LOG_MODULE_REGISTER(ADE9000_POD_CALIBRATION, ADE9000_LOG_LEVEL);

// Expected RMS *codes* from a physical input (Irms or Vrms)
static inline double ade_expected_rms_codes_current(double Irms, double cur_xfer_fn, double pgaI)
{
    // fraction of ADC full-scale at current channel pins:
    // IFSP = (Vadc_rms / ADC_FS_rms) = (cur_xfer_fn * Irms) / (0.707/gain)
    const double IFSP = (cur_xfer_fn * Irms) / ADC_FS_RMS(pgaI);
    return IFSP * ((double)ADE9000_RMS_FULL_SCALE_CODES);
}

static inline double ade_expected_rms_codes_voltage(double Vrms, double volt_xfer_fn, double pgaV)
{
    const double VFSP = (volt_xfer_fn * Vrms) / ADC_FS_RMS(pgaV);
    return VFSP * ((double)ADE9000_RMS_FULL_SCALE_CODES);
}

// Convert floating-point gain to register value: GAIN_REG = (GAIN - 1)*2^27
static inline int32_t ade_gain_to_reg(double gain)
{
    const double scale = (double)(1u << 27);   // 2^27
    return (int32_t)((gain - 1.0) * scale);
}

// Offset register per guide: OS = (Exp^2 - Meas^2) / 2^15
static inline int32_t ade_rmsos_to_reg(double expected_codes, double measured_codes)
{
    const double num = (expected_codes * expected_codes) - (measured_codes * measured_codes);
    return (int32_t)(num / (double)(1u << 15));
}

// Read & average a 32-bit RMS register N times
static uint32_t ade_avg_rms_reg(uint16_t addr, int samples, int delay_ms_per_sample)
{
    uint64_t acc = 0;
    for (int i = 0; i < samples; ++i) {
        acc += ADE9000__read_32(addr);
        k_msleep(delay_ms_per_sample);  // or your platform’s delay
    }
    return (uint32_t)(acc / (uint64_t)samples);
}


/*
 * Function for calibrating voltage and current offsets
 * provide valibration data along with phase to calibrate; if "PHASE_MAX" is provided as argument than all phases will be calibrated with provided data
 */
void run_rms_calibration_once(CalibrationData_t* calib_Data, enum PHASES_e phaseSel)
{
    // 1) Read actual PGA gains (these are *multipliers*, e.g. 1,2,4…)
    uint8_t pgaI = 1, pgaV = 1;
    get_PGA_gain(&pgaI, &pgaV);

    // 2) Average RMS at nominal inputs (example: 1 second @ 50 Hz)
    const int samples = 50;                 // or 100
    const int delay_ms = 20;                // 50*20ms = 1s

    const uint32_t phase_addr[PHASE_MAX][6] = { {ADDR_AIRMS, ADDR_AVRMS, ADDR_AIGAIN, ADDR_AVGAIN, ADDR_AIRMSOS, ADDR_AVRMSOS},
                                                {ADDR_BIRMS, ADDR_BVRMS, ADDR_BIGAIN, ADDR_BVGAIN, ADDR_BIRMSOS, ADDR_BVRMSOS},
                                                {ADDR_CIRMS, ADDR_CVRMS, ADDR_CIGAIN, ADDR_CVGAIN, ADDR_CIRMSOS, ADDR_CVRMSOS} };

    // Decide start & end indices
    int startPhase = (phaseSel == PHASE_MAX) ? PHASE_A     : phaseSel;
    int endPhase   = (phaseSel == PHASE_MAX) ? PHASE_MAX   : (phaseSel + 1);

    for(int phase=startPhase; phase < endPhase; phase++)
    {
        const uint32_t IRMS_meas = ade_avg_rms_reg(phase_addr[phase][0], samples, delay_ms);
        const uint32_t VRMS_meas = ade_avg_rms_reg(phase_addr[phase][1], samples, delay_ms);
    
        // 3) Compute expected codes from transfer functions and PGA
        const double IRMS_exp = ade_expected_rms_codes_current(
            (double)NOMINAL_INPUT_CURRENT,      // your nominal Irms
            (double)CURRENT_TRANSFER_FUNCTION,  // V/A at ADC pins
            (double)pgaI
        );
        const double VRMS_exp = ade_expected_rms_codes_voltage(
            (double)NOMINAL_INPUT_VOLTAGE,
            (double)VOLTAGE_TRANSFER_FUNCTION,  // V/V at ADC pins
            (double)pgaV
        );
    
        // 4) Gains (floating) and registers
        const double IGAIN_f = IRMS_exp / (double)IRMS_meas;
        const double VGAIN_f = VRMS_exp / (double)VRMS_meas;
    
        const int32_t IGAIN_reg = ade_gain_to_reg(IGAIN_f);
        const int32_t VGAIN_reg = ade_gain_to_reg(VGAIN_f);
    
        ADE9000__write_32(phase_addr[phase][2], (uint32_t)IGAIN_reg);
        ADE9000__write_32(phase_addr[phase][3], (uint32_t)VGAIN_reg);
    
        // 5) Offset calibration for current (example 20 mA)
        const double Ical = calib_Data->calibrationCurrent;//0.020; // 20 mA
        const uint32_t IRMS_meas_off = ade_avg_rms_reg(phase_addr[phase][0], samples, delay_ms);
        const double IRMS_exp_off = ade_expected_rms_codes_current(Ical, (double)CURRENT_TRANSFER_FUNCTION, (double)pgaI);
        const int32_t IRMSOS_reg = ade_rmsos_to_reg(IRMS_exp_off, (double)IRMS_meas_off);
        // ADE9000__write_32(phase_addr[phase][4], (uint32_t)IRMSOS_reg);
    
        // Optional: Voltage offset (apply small Vrms and repeat)
        const double Vcal = calib_Data->calibrationVoltage;
        const uint32_t VRMS_meas_off = ade_avg_rms_reg(phase_addr[phase][1], samples, delay_ms);
        const double VRMS_exp_off = ade_expected_rms_codes_voltage(Vcal, (double)VOLTAGE_TRANSFER_FUNCTION, (double)pgaV);
        const int32_t VRMSOS_reg = ade_rmsos_to_reg(VRMS_exp_off, (double)VRMS_meas_off);
        // ADE9000__write_32(phase_addr[phase][5], (uint32_t)VRMSOS_reg);

        LOG_WRN("Performing calibration for %s", ((phase == PHASE_A) ? "PHASE_A" : ((phase == PHASE_B) ? "PHASE_B" : "PHASE_C")));
        LOG_WRN("Gains: pgaI=%d, pgaV=%d", pgaI, pgaV);
        LOG_WRN("Parameters\n\t\tMeasured average codes\n\t\t\tIRMS_meas: %d   VRMS_meas: %d\n\t\t"
                "Expected codes\n\t\t\tIRMS_exp: %f   VRMS_exp: %f\n\t\t"
                "Calculated gains\n\t\t\tIGAIN_f: %f(0x%X)   VGAIN_f: %f(0x%X)\n\t\t"
                "Current offset calibration\n\t\t\tIRMS_meas_off: %d   IRMS_exp_off: %f   IRMSOS_reg: 0x%X\n\t\t"
                "Voltage offset calibration\n\t\t\tVRMS_meas_off: %d   VRMS_exp_off: %f   VRMSOS_reg: 0x%X",
                IRMS_meas, VRMS_meas,
                IRMS_exp, VRMS_exp,
                IGAIN_f, IGAIN_reg, VGAIN_f, VGAIN_reg,
                IRMS_meas_off, IRMS_exp_off, IRMSOS_reg,
                VRMS_meas_off, VRMS_exp_off, VRMSOS_reg);
    }
}