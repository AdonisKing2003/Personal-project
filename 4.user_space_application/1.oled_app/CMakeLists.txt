cmake_minimum_required(VERSION 3.10)
project(ssd1306_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE is not specified!")
endif()

find_package(Threads REQUIRED)

# Include directories for headers
include_directories(
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/drivers
  ${PROJECT_SOURCE_DIR}/include/core
  ${PROJECT_SOURCE_DIR}/include/utils
)

# Driver source ( HAL )
set(DRIVER_SOURCES
  ${PROJECT_SOURCE_DIR}/src/drivers/ssd1306.c
  ${PROJECT_SOURCE_DIR}/src/drivers/button.c
  ${PROJECT_SOURCE_DIR}/src/drivers/seven_segment.c
  ${PROJECT_SOURCE_DIR}/src/drivers/ssd1306_data.c
  ${PROJECT_SOURCE_DIR}/src/drivers/alphabet_font10.c
  ${PROJECT_SOURCE_DIR}/src/drivers/buzzer.c
  ${PROJECT_SOURCE_DIR}/src/drivers/pwm_control.c
)

# Core source
set(CORE_SOURCES
  ${PROJECT_SOURCE_DIR}/src/core/app.c
  ${PROJECT_SOURCE_DIR}/src/core/input_thread.c
  ${PROJECT_SOURCE_DIR}/src/core/game_logic_thread.c
  ${PROJECT_SOURCE_DIR}/src/core/render_thread.c
)

set(APP_SOURCES
  ${PROJECT_SOURCE_DIR}/src/core/main.c
)

set(TEST_OLED_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_device_driver/oled_write_pixel.c
)

set(TEST_BUTTON_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_HAL/test_button.c
)

set(TEST_BUZZER_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_HAL/test_buzzer.c
)

set(TEST_APP_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_app/test_app_func.c
)

# Utils source
set(UTILS_SOURCES
  ${PROJECT_SOURCE_DIR}/src/utils/utils.c
)

# Build executable from core and drivers
add_executable(ssd1306_app
  ${APP_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

add_executable(oled_write_pixel
  ${TEST_OLED_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

add_executable(test_button
  ${TEST_BUTTON_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

add_executable(test_buzzer
  ${TEST_BUZZER_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

add_executable(test_app
  ${TEST_APP_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

target_link_libraries(test_button PRIVATE Threads::Threads)
target_link_libraries(test_app PRIVATE Threads::Threads)

# Link any required libraries here if needed (e.g., pthread)
# target_link_libraries(ssd1306_app pthread)


# build:
# source ../../../3.Device_driver/SDK/environment-setup-cortexa72-poky-linux
# export OE_CMAKE_TOOLCHAIN_FILE=~/workspace/4.SDK/sysroots/x86_64-pokysdk-linux/usr/share/cmake/OEToolchainConfig.cmake
# cd /home/adonisking/Learning_Linux/4.user_space_application/1.oled_app/build
# cmake -DCMAKE_TOOLCHAIN_FILE=$OE_CMAKE_TOOLCHAIN_FILE ..
# cmake --build . --target oled_write_pixel
# cmake --build . --target ssd1306_app