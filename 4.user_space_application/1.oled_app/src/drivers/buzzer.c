#include "buzzer.h"

#define DEFAULT_PERIOD 0
int fd_buzzer;

// static pthread_t buzzer_thread_func;
static e_buzzer_mode current_mode = BUZZER_MODE_OFF;


void *buzzer_thread_func(void *arg) {
    while (1) {
        switch (current_mode) {
            case BUZZER_MODE_OFF:
                buzzer_control(0, DEFAULT_PERIOD);
                break;
            case BUZZER_MODE_ON:
                buzzer_control(50, DEFAULT_PERIOD);
                usleep(200000); // 200ms
                current_mode = BUZZER_MODE_OFF;
                break;
            case BUZZER_MODE_BEEP:
                buzzer_control(50, DEFAULT_PERIOD);
                usleep(100000);
                buzzer_control(0, DEFAULT_PERIOD);
                usleep(100000);
                break;
            case BUZZER_MODE_PLAY_MELODY:
                // Loop qua mảng melody
                int melody[] = {1000, 1500, 2000}; // Hz
                for (int i = 0; i < 3; i++) {
                    buzzer_control(50, melody[i]);
                    usleep(300000); // 300ms mỗi nốt
                }
                break;
        }
    }
}

e_app_return_t buzzer_init(uint8_t buzzer_gpio)
{
    int ret = pwm_init("/dev/pwm_device");
    if( ret == APP_RET_OK)
    {
        PRINTF_INFO("buzzer initialize success!\n");
    }
    else
    {
        PRINTF_ERROR("buzzer init failed!\n");
    }
    return ret;
}

e_app_return_t buzzer_run(e_buzzer_mode mode)
{
    int count = 20;
    int while_count = 5000;
    while(while_count>0)
    {
        switch(mode)
        {
            case BUZZER_MODE_OFF:
                buzzer_control(0, 0);
                delay_ms(1000);
                mode = BUZZER_MODE_ON;
            break;
            case BUZZER_MODE_ON:
                buzzer_control(50, 0);
                delay_ms(200);
                buzzer_control(0, 0);
                delay_ms(100);
                if(count < 0)
                {
                    mode = BUZZER_MODE_BEEP;
                    count = 20;
                }
            break;
            case BUZZER_MODE_BEEP:
                    buzzer_control(50, 0);
                    delay_ms(100);
                    buzzer_control(0, 0);
                    delay_ms(100);
                    count--;
                    delay_ms(300);
                    if(count < 0)
                    {
                        mode = BUZZER_MODE_PLAY_MELODY;
                        count = 20;
                    }
            break;
            case BUZZER_MODE_PLAY_MELODY:
                int melody[] = {1000, 1500, 2000}; // Hz
                for (int i = 0; i < 3; i++) {
                    buzzer_control(50, melody[i]);
                    usleep(300000); // 300ms mỗi nốt
                }
                mode = BUZZER_MODE_OFF;
            break;
            default:
                return APP_RET_ERR_IO_DEV_CLOSE;
            break;
        }
        while_count--;
    }
    
    return APP_RET_OK;
}

e_app_return_t buzzer_control(int duty_cycle, int period)
{
    if(duty_cycle >= 0)
    {
        pwm_set_duty(duty_cycle);
    }
    if(period > 0)
    {
        pwm_set_period(period);
    }
    return APP_RET_OK;
}

e_app_return_t buzzer_deinit(uint8_t buzzer_gpio)
{
    int ret = pwm_deinit();
    if( ret == APP_RET_OK)
    {
        PRINTF_INFO("buzzer deinit success!\n");
    }
    else
    {
        PRINTF_ERROR("buzzer deinit failed!\n");
    }
    return ret;
}
