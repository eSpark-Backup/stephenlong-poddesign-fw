#pragma once

#include <defines.h>

extern const struct device *_afe3010_port_gpio_dev;
extern struct gpio_callback _afe3010_cb_data;

#define BUFFER_SIZE                     12048//7230//6024//200      // buffer value set such that it is filled in 2secs
#define AFE3010_ALARM_SAMPLE_TIME       166           // uS; 3KHz freq to be sampled, Nyquiste rate determines sampling freq to be twice the freq to be sampled
#define AFE3010_NO_OF_PHASES            3
#define SCR_ACTIVE_WINDOW_MS            10             // time window prior to which scr was triggerred
#define DEBOUNCE_MS                     50            // debounce on scr pin  (do we even need it?)

#define AFE3010_PORT        GROUND_FAULT_GPIO_PORT
// Phase A pins
#define AFE3010_ALARM_VA    GROUND_FAULT_ALARM_VA
#define AFE3010_PTT_VA      GROUND_FAULT_PTT_VA
#define AFE3010_SCR_SIG_VA  GROUND_FAULT_SCR_SIG_VA
// Phase B pins
#define AFE3010_ALARM_VB    GROUND_FAULT_ALARM_VB
#define AFE3010_PTT_VB      GROUND_FAULT_PTT_VB
#define AFE3010_SCR_SIG_VB  GROUND_FAULT_SCR_SIG_VB
// Phase B pins
#define AFE3010_ALARM_VC    GROUND_FAULT_ALARM_VC
#define AFE3010_PTT_VC      GROUND_FAULT_PTT_VC
#define AFE3010_SCR_SIG_VC  GROUND_FAULT_SCR_SIG_VC

enum PHASES_e
{
    PHASE_A,
    PHASE_B,
    PHASE_C,
};

void shared_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

void analyze_alarm_pattern(enum PHASES_e phase);

void init_afe3010(void);