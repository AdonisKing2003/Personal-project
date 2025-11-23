#ifndef UTILS_H
#define UTILS_H

/* definition declare*/
#define PRINTF_DEBUG(...)    printf("[DEBUG]: " __VA_ARGS__);
#define PRINTF_ERROR(...)    printf("[ERROR]: " __VA_ARGS__);
#define PRINTF_INFO(...)     printf("[INFO]: " __VA_ARGS__);

/* Enum declare */
typedef enum  {
    APP_RET_OK                          = 0,
    APP_RET_ERR_NOT_INITIALIZE          = 1,
    APP_RET_ERR_CONF                    = 2,
    APP_RET_ERR_NULL_POINTER            = 3,
    APP_RET_ERR_MUTEX_LOCK              = 4,
    APP_RET_ERR_MUTEX_UNLOCK            = 5,
    APP_RET_ERR_IO_DEV_CLOSE            = 6,
    APP_RET_ERR_IO_DEV_OPEN             = 7,
    APP_RET_ERR_IO_READ                 = 8,
    APP_RET_ERR_IO_WRITE                = 9,
    APP_RET_ERR_INVALID_BUFFER_INDEX    = 10,
    APP_RET_ERR_INVALID_PARAM           = 11,
    APP_RET_ERR_PWM_PERIOD              = 12,
    APP_RET_ERR_PWM_DUTY                = 13
} e_app_return_t;

/* variable declare*/

/* function declare*/
int clamp(int value, int min, int max);
unsigned int random_unit(unsigned int max);
void delay_ms(unsigned int milliseconds);
void string_to_upper(char *str);

#endif // UTILS_H