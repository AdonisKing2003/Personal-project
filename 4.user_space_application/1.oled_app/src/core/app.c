// This file code game flappy bird in the oled
#include <stdio.h>
#include <pthread.h>
#include "app.h"
#include "ssd1306.h"
#include "input_thread.h"
#include "game_logic_thread.h"
#include "render_thread.h"
#include "seven_segment.h"
#include "button.h"
#include "utils.h"
#include "buzzer.h"

int fd;

void *input_thread_func(void*);
void *game_logic_thread_func(void*);
void *render_thread_func(void*);

void app_run(void)
{
    pthread_t input_thread, logic_thread, render_thread, buzzer_thread;
    fd = ssd1306_init();
    if( fd < 0 )
    {
        PRINTF_ERROR("SSD1306 initialize failed\n ");
        return;
    }
    init_game(5);
    if ( button_init(20) < 0) // HARD coded button GPIO 20
    {
        printf("[ERROR]: Cannot initialize button GPIO %d !\n", 20);
        return;
    }
    pthread_create(&input_thread, NULL, input_thread_func, NULL);
    pthread_create(&logic_thread, NULL, game_logic_thread_func, NULL);
    pthread_create(&render_thread, NULL, render_thread_func, &fd);
    pthread_create(&buzzer_thread, NULL, buzzer_thread_func, NULL);

    pthread_join(input_thread, NULL);
    pthread_join(logic_thread, NULL);
    pthread_join(render_thread, NULL);
}
