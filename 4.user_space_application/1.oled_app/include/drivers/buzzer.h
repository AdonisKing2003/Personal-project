#ifndef BUZZER_H
#define BUZZER_H

#include "utils.h"
#include "pwm_control.h"

#define BUZZER_GPIO 12 // PWM0

typedef enum {
    BUZZER_MODE_OFF      = 0,
    BUZZER_MODE_ON,
    BUZZER_MODE_BEEP,
    BUZZER_MODE_PLAY_MELODY
} e_buzzer_mode;

void *buzzer_thread_func(void *arg);

e_app_return_t buzzer_init(uint8_t buzzer_gpio);

e_app_return_t buzzer_mode();

e_app_return_t buzzer_control(int duty_cycle, int period);

e_app_return_t buzzer_deinit(uint8_t buzzer_gpio);

#endif // BUZZER_H