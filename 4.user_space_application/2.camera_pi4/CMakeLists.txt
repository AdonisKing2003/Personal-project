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
  ${PROJECT_SOURCE_DIR}/config
)

# Driver source ( HAL )
set(DRIVER_SOURCES
  ${PROJECT_SOURCE_DIR}/src/drivers/camera.c
)

# Core source
set(CORE_SOURCES
  # ${PROJECT_SOURCE_DIR}/src/core/app.c
  # ${PROJECT_SOURCE_DIR}/src/core/input_thread.c
  # ${PROJECT_SOURCE_DIR}/src/core/game_logic_thread.c
  # ${PROJECT_SOURCE_DIR}/src/core/render_thread.c
)

set(APP_SOURCES
  ${PROJECT_SOURCE_DIR}/src/core/main.c
)

set(TEST_CAM_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_camera/test_camera.c
)

# Utils source
set(UTILS_SOURCES
  # ${PROJECT_SOURCE_DIR}/src/utils/utils.c
)

# Build executable from core and drivers
add_executable(camera_test
  ${TEST_CAM_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

# target_link_libraries(camera_test PRIVATE Threads::Threads)
# export OE_CMAKE_TOOLCHAIN_FILE=~/workspace/4.SDK/sysroots/x86_64-pokysdk-linux/usr/share/cmake/OEToolchainConfig.cmake
# cmake -DCMAKE_TOOLCHAIN_FILE=$OE_CMAKE_TOOLCHAIN_FILE ..