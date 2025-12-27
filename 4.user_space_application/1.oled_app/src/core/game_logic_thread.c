// Purpose: Compute game state updates, physics, collision detection, score, and rules.

#include <stdio.h>
#include <unistd.h>
#include "game_logic_thread.h"
#include "button.h"
#include "utils.h"

/* mutex declaration */
pthread_mutex_t mutex_game_logic = PTHREAD_MUTEX_INITIALIZER;

eGameState game_state = GAME_STATE_READY;
bool render_flag = true;

struct stBirdInfo bird;
struct stColumnInfo col_bottom_info;
struct stColumnInfo col_top_info;
struct stGameInfo game_info;
int column_spacing = 30;
uint8_t game_speed = 5;

void *game_logic_thread_func(void *arg)
{
    PRINTF_INFO("Game logic thread started!\n");
    bool render_flag_local;
    uint8_t game_state_local = -1;
    uint8_t stop_count = 10;
    uint8_t wait_count = 5;
    bool check_move = false;
    while (1)
    {
        get_render_flag_and_state(&render_flag_local, &game_state_local);
        PRINTF_INFO("game_state_local = %d\n", game_state_local);
        if (game_state_local == GAME_STATE_READY)
        {
            // Wait for user input to start the game
            if (button_is_pressed(BUTTON_LINE_OFFSET))
            {
                PRINTF_INFO("Game started by button press!\n");
                game_state_local = GAME_STATE_PLAYING;
                render_flag_local = true;
            }
        }
        else if (game_state_local == GAME_STATE_PLAYING)
        {
            // Update bird position
            pthread_mutex_lock(&mutex_game_logic);
            PRINTF_INFO("Start playing!\n");
            if (button_is_pressed(BUTTON_LINE_OFFSET))
            {
                if (move_up(&bird) == APP_RET_ERR_INVALID_BUFFER_INDEX)
                {
                    game_state_local = GAME_STATE_OVER;
                    PRINTF_INFO("Out boundary!\n");
                    stop_count = 5;
                }
                check_move = true;
            }
            else
            {
                check_move = false;
            }
            // Update columns
            if (wait_count > 0)
            {
                wait_count--;
            }
            else
            {
                update_column(&col_top_info);
                update_column(&col_bottom_info);
                // Update score
                if (col_top_info.column_x > 90 + game_speed)
                {
                    increase_point(&game_info);
                    PRINTF_INFO("Point increased: %d\n", game_info.points);
                }
                if (!check_move)
                {
                    if (move_down(&bird) == APP_RET_ERR_INVALID_BUFFER_INDEX)
                    {
                        game_state_local = GAME_STATE_OVER;
                        PRINTF_INFO("Out boundary!\n");
                        stop_count = 5;
                    }
                }
                // wait_count = 5;
            }

            // Check for collisions
            if (check_bird_collision(&bird, &col_bottom_info) != 0 ||
                check_bird_collision(&bird, &col_top_info) != 0)
            {
                game_state_local = GAME_STATE_OVER;
                PRINTF_INFO("COLLISION CHECK!\n");
                delay_ms(2000);
                stop_count = 5;
            }
            render_flag_local = true;

            pthread_mutex_unlock(&mutex_game_logic);
        }
        else if (game_state_local == GAME_STATE_OVER)
        {
            // Handle game over state
            if (stop_count > 0)
            {
                stop_count--;
            }
            if (button_is_pressed(BUTTON_LINE_OFFSET) && stop_count == 0)
            {
                init_game(game_speed);
                game_state_local = GAME_STATE_READY;
                render_flag_local = true;
            }
        }
        set_game_state_and_render_flag(game_state_local, render_flag_local);
        delay_ms(100);
    }
    return NULL;
}

e_app_return_t move_up(struct stBirdInfo *bird)
{
    if (bird->bird_y - bird->bird_acceleration > 0)
    {
        bird->bird_y -= bird->bird_acceleration;
        return APP_RET_OK;
    }
    return APP_RET_ERR_INVALID_BUFFER_INDEX;
}

e_app_return_t move_down(struct stBirdInfo *bird)
{
    if (bird->bird_y < 64 - bird->bird_acceleration)
    {
        bird->bird_y += bird->bird_acceleration;
        return APP_RET_OK;
    }

    return APP_RET_ERR_INVALID_BUFFER_INDEX;
}

int check_bird_collision(struct stBirdInfo *bird, struct stColumnInfo *col_bottom_info)
{
    if (bird == NULL || col_bottom_info == NULL)
    {
        return -1;
    }
    if ((bird->bird_x + bird->bird_width >= col_bottom_info->column_x))
    {
        // Check Y overlap
        if ((bird->bird_y + bird->bird_height >= col_bottom_info->column_top_y && bird->bird_y + bird->bird_height <= col_bottom_info->column_bottom_y) ||
            (bird->bird_y <= col_bottom_info->column_bottom_y && bird->bird_y >= col_bottom_info->column_top_y))
        {
            return 1; // Collision detected
        }
    }
    return 0;
}

static stColumnInfo init_column(uint8_t column_x, uint8_t column_top_y, uint8_t column_bottom_y)
{
    stColumnInfo column = {.column_x = column_x, .column_top_y = column_top_y, .column_bottom_y = column_bottom_y};
    return column;
}

void create_column()
{
    // Create a new column and add it to the column list
    struct stColumnInfo new_top_column = init_column(100, 0, 20);
    struct stColumnInfo new_bottom_column = init_column(100, 44, 64);
    col_bottom_info = new_top_column;
    col_bottom_info = new_bottom_column;
}

void update_column(struct stColumnInfo *col_info)
{
    // Move column
    col_info->column_x -= game_speed;
    // Check if column out then create new column
    if (col_info->column_x < game_speed)
    {
        col_info->column_x = 100;
    }
}

void increase_point(struct stGameInfo *game_point)
{
    game_point->points++;
}

stBirdInfo init_bird(uint8_t bird_x, uint8_t bird_y, uint8_t bird_h, uint8_t bird_w, uint8_t bird_accel)
{
    stBirdInfo bird;
    bird.bird_x = bird_x;
    bird.bird_y = bird_y;
    bird.bird_height = bird_h;
    bird.bird_width = bird_w;
    bird.bird_acceleration = bird_accel;
    return bird;
}

void init_game_info(struct stGameInfo *game_info)
{
    game_info->points = 0;
    game_info->status = GAME_STATE_READY;
    game_info->time_play = 0;
}

void init_game(uint8_t game_speed)
{
    PRINTF_INFO("Initialize game with speed: %d\n", game_speed);
    // Init bird
    bird = init_bird(20, 30, 10, 15, 5);

    // Init column
    col_top_info = init_column(100, 0, 10);
    col_bottom_info = init_column(100, 54, 64);

    // Init game status
    init_game_info(&game_info);
    PRINTF_INFO("Game initialized successfully!\n");
}

void update_game_play(int fd)
{
    // Update game play status if needed
    if (game_state == GAME_STATE_PLAYING)
    {
        // e.g., increase time played
        game_info.time_play++;
    }
}

void get_render_flag_and_state(bool *flag, uint8_t *state)
{
    pthread_mutex_lock(&mutex_game_logic);
    *flag = render_flag;
    *state = game_state;
    pthread_mutex_unlock(&mutex_game_logic);
}

void set_game_state_and_render_flag(uint8_t new_state, bool flag)
{
    // Lock mutex if multithreaded environment
    pthread_mutex_lock(&mutex_game_logic);
    game_state = new_state;
    render_flag = flag;
    pthread_mutex_unlock(&mutex_game_logic);
}