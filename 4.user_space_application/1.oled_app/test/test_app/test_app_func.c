#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ssd1306_data.h"
#include "utils.h"
#include "ssd1306.h"
#include "button.h"
#include "game_logic_thread.h"
#include "render_thread.h"

void test_column(int fd);
void test_bird(int fd);
void test_column_and_bird(int fd);

int main()
{
    int fd = open("/dev/etx_oled_device", O_WRONLY);
    if (fd < 0) {
        perror("Open device failed");
        return 1;
    }
    // test_column(fd);
    // test_bird(fd);
    test_column_and_bird(fd);

    close(fd);
    return 0;
}

void test_column(int fd)
{
    draw_line(fd, 0, 0, 127, 63, 1); // Draw border for reference
    update_oled_display(fd);
    sleep(2);
    stColumnInfo column = {.column_x=120, .column_top_y = 49, .column_bottom_y = 64};
    printf("Initial Column Position: X=%d, Top Y=%d, Bottom Y=%d\n", column.column_x, column.column_top_y, column.column_bottom_y);
    oled_clear_display(fd);
    // draw_column(fd, &column);
    // draw_bit_map(fd, 0, 0, &column_bitmap[0], 34, 16, true);
    draw_column(fd, &column);
    update_oled_display(fd);
    sleep(1);
    // Simulate moving the column
    for(int i=0; i<5; i++)
    {
        column.column_x -= 5; // Move left by 5 units
        printf("Updated Column Position: X=%d, Top Y=%d, Bottom Y=%d\n", column.column_x, column.column_top_y, column.column_bottom_y);
        update_column(&column);
        oled_clear_display(fd);
        draw_column(fd, &column);
        update_oled_display(fd);
        usleep(100000); // Pause for a second
    }
}

void test_bird(int fd)
{
    stBirdInfo bird = {.bird_x=10, .bird_y=30, .bird_height=15, .bird_width=15, .bird_acceleration=5};
    printf("Initial Bird Position: X=%d, Y=%d\n", bird.bird_x, bird.bird_y);
    oled_clear_display(fd);
    draw_bird(fd, &bird);
    update_oled_display(fd);
    usleep(500000); // Pause for half a second

    // Simulate bird movement
    for(int i=0; i<5; i++)
    {
        move_up(&bird);
        printf("Bird Position after moving up: X=%d, Y=%d\n", bird.bird_x, bird.bird_y);
        oled_clear_display(fd);
        draw_bird(fd, &bird);
        update_oled_display(fd);
        usleep(100000); // Pause for half a second

        move_down(&bird);
        printf("Bird Position after moving down: X=%d, Y=%d\n", bird.bird_x, bird.bird_y);
        oled_clear_display(fd);
        draw_bird(fd, &bird);
        update_oled_display(fd);
        usleep(100000); // Pause for half a second
    }
}

void test_column_and_bird(int fd)
{
    stColumnInfo column_top = {.column_x=50, .column_top_y = 0, .column_bottom_y = 20};
    stColumnInfo column_bottom = {.column_x=50, .column_top_y = 44, .column_bottom_y = 64};
    stBirdInfo bird = {.bird_x=10, .bird_y=10, .bird_height=15, .bird_width=15, .bird_acceleration=5};

    for(int i=0; i<10; i++)
    {
        // Update positions
        column_bottom.column_x -= 5; // Move column left
        column_top.column_x -= 5; // Move column left
        if(i % 2 == 0)
            move_up(&bird);
        else
            move_down(&bird);

        // Clear display and redraw
        oled_clear_display(fd);
        draw_column(fd, &column_top);
        draw_column(fd, &column_bottom);
        draw_bird(fd, &bird);
        update_oled_display(fd);
        if(check_bird_collision(&bird, &column_bottom))
        {
            PRINTF_ERROR("Collision detected at Bird bottom (X=%d, Y=%d) and Column(X=%d, Top Y=%d, Bottom Y=%d)\n", 
                bird.bird_x, bird.bird_y, column_bottom.column_x, 
                column_bottom.column_top_y, column_bottom.column_bottom_y);
            break;
        }
        else if (check_bird_collision(&bird, &column_top))
        {
            PRINTF_ERROR("Collision detected at Bird top (X=%d, Y=%d) and Column(X=%d, Top Y=%d, Bottom Y=%d)\n", 
                bird.bird_x, bird.bird_y, column_top.column_x, column_top.column_top_y, column_top.column_bottom_y);
            break;
        }
        
        else 
        {
            printf("[INFO]: No collision!\n");
        }
        usleep(200000); // Pause for 0.2 second
    }
}