cmake_minimum_required(VERSION 3.10)
project(ssd1306_project C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE is not specified!")
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

if(CMAKE_CROSSCOMPILING)
  set(ENV{PKG_CONFIG_PATH} "")
  set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
endif()

# ============================================================================
# Find libcamera (required for wrapper)
# ============================================================================
# pkg_check_modules(LIBCAMERA REQUIRED libcamera)
# pkg_check_modules(LIBCAMERA_BASE REQUIRED libcamera-base)
set(LIBCAMERA_INCLUDE_DIRS "${CMAKE_SYSROOT}/usr/include/libcamera")
set(LIBCAMERA_LIBRARIES "camera;camera-base")
set(LIBCAMERA_LIBRARY_DIRS "${CMAKE_SYSROOT}/usr/lib")

# Also set for libcamera-base
set(LIBCAMERA_BASE_INCLUDE_DIRS "${CMAKE_SYSROOT}/usr/include/libcamera")
set(LIBCAMERA_BASE_LIBRARIES "camera-base")

message(STATUS "libcamera found (manual): ${LIBCAMERA_LIBRARIES}")
message(STATUS "libcamera include (manual): ${LIBCAMERA_INCLUDE_DIRS}")

# Add library search path
link_directories(${LIBCAMERA_LIBRARY_DIRS})
include_directories(${LIBCAMERA_INCLUDE_DIRS})

message(STATUS "libcamera found: ${LIBCAMERA_LIBRARIES}")
message(STATUS "libcamera include: ${LIBCAMERA_INCLUDE_DIRS}")

# Include directories for headers
include_directories(
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/drivers
  ${PROJECT_SOURCE_DIR}/include/core
  ${PROJECT_SOURCE_DIR}/include/utils
  ${PROJECT_SOURCE_DIR}/config
)

# ============================================================================
# RPI Camera Wrapper Library
# ============================================================================
set(RPI_CAMERA_WRAPPER_SOURCES
  ${PROJECT_SOURCE_DIR}/src/drivers/rpi_camera.cpp
)

# Build wrapper as shared library
add_library(rpi_camera_wrapper SHARED
  ${RPI_CAMERA_WRAPPER_SOURCES}
)

target_link_libraries(rpi_camera_wrapper PUBLIC
  ${LIBCAMERA_LIBRARIES}
  ${LIBCAMERA_BASE_LIBRARIES}
  Threads::Threads
)

target_compile_options(rpi_camera_wrapper PRIVATE
  ${LIBCAMERA_CFLAGS_OTHER}
  -Wall -Wextra
)

# ============================================================================
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

# ============================================================================
# Test sources
# ============================================================================
set(TEST_CAM_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_camera/test_camera_with_libcamera.c
)

set(TEST_GSTREAMER_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_camera/test_gstreamer.c
)

# Wrapper tests
set(TEST_WRAPPER_BASIC_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_wrapper/test_basic.c
)

set(TEST_WRAPPER_FORMATS_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_wrapper/test_formats.c
)

set(TEST_WRAPPER_CONTROLS_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_wrapper/test_controls.c
)

set(TEST_WRAPPER_STRESS_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_wrapper/test_stress.c
)
# ============================================================================

# Utils source
set(UTILS_SOURCES
  ${PROJECT_SOURCE_DIR}/src/utils/utils.c
)

# Build executable from core and drivers
add_executable(camera_test
  ${TEST_CAM_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

add_executable(gstreamer_test
  ${TEST_GSTREAMER_SOURCES}
  ${CORE_SOURCES}
  ${DRIVER_SOURCES}
  ${UTILS_SOURCES}
)

target_link_libraries(camera_test PRIVATE Threads::Threads)

target_link_libraries(gstreamer_test PRIVATE 
  Threads::Threads
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_APP_LIBRARIES}
  ${GSTREAMER_VIDEO_LIBRARIES}
)

# ============================================================================
# Build wrapper test executables
# ============================================================================

# Test 1: Basic functionality
add_executable(test_wrapper_basic
  ${TEST_WRAPPER_BASIC_SOURCES}
)

target_link_libraries(test_wrapper_basic PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)
target_sources(test_wrapper_basic PRIVATE
  ${UTILS_SOURCES}
)

# Test 2: Format tests
add_executable(test_wrapper_formats
  ${TEST_WRAPPER_FORMATS_SOURCES}
)

target_link_libraries(test_wrapper_formats PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)

# Test 3: Control tests
add_executable(test_wrapper_controls
  ${TEST_WRAPPER_CONTROLS_SOURCES}
)

target_link_libraries(test_wrapper_controls PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)

# Test 4: Stress tests
add_executable(test_wrapper_stress
  ${TEST_WRAPPER_STRESS_SOURCES}
)

target_link_libraries(test_wrapper_stress PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)

# ============================================================================
# Build Sample app
# ============================================================================
set(SAMPLE_APP_SOURCES
  ${PROJECT_SOURCE_DIR}/test/test_wrapper/sample_app.c
)

add_executable(sample_camera_app
  ${SAMPLE_APP_SOURCES}
)

target_link_libraries(sample_camera_app PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)

# ============================================================================
# Installation rules
# ============================================================================
install(TARGETS rpi_camera_wrapper
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(FILES 
  ${PROJECT_SOURCE_DIR}/include/drivers/rpi_camera.h
  DESTINATION include
)

# Install test executables (optional)
install(TARGETS 
  test_wrapper_basic
  test_wrapper_formats
  test_wrapper_controls
  test_wrapper_stress
  sample_camera_app
  RUNTIME DESTINATION bin/tests
)

target_include_directories(rpi_camera_wrapper PUBLIC
  ${LIBCAMERA_INCLUDE_DIRS}
  ${LIBCAMERA_BASE_INCLUDE_DIRS}
)

# ============================================================================
# Custom target to run all wrapper tests
# ============================================================================
add_custom_target(run_wrapper_tests
  COMMAND echo "Running wrapper tests..."
  COMMAND echo "================================"
  COMMAND echo "Test 1: Basic Tests"
  COMMAND echo "================================"
  COMMAND $<TARGET_FILE:test_wrapper_basic> || true
  COMMAND echo ""
  COMMAND echo "================================"
  COMMAND echo "Test 2: Format Tests"
  COMMAND echo "================================"
  COMMAND $<TARGET_FILE:test_wrapper_formats> || true
  COMMAND echo ""
  COMMAND echo "================================"
  COMMAND echo "Test 3: Control Tests"
  COMMAND echo "================================"
  COMMAND $<TARGET_FILE:test_wrapper_controls> || true
  COMMAND echo ""
  COMMAND echo "================================"
  COMMAND echo "Test 4: Stress Tests"
  COMMAND echo "================================"
  COMMAND $<TARGET_FILE:test_wrapper_stress> || true
  DEPENDS 
    test_wrapper_basic
    test_wrapper_formats
    test_wrapper_controls
    test_wrapper_stress
)

# ============================================================================
# Print build information
# ============================================================================
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "  libcamera: ${LIBCAMERA_VERSION}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  rpi_camera_wrapper - Camera wrapper library")
message(STATUS "  test_wrapper_basic - Basic functionality tests")
message(STATUS "  test_wrapper_formats - Format tests")
message(STATUS "  test_wrapper_controls - Control tests")
message(STATUS "  test_wrapper_stress - Stress tests")
message(STATUS "  run_wrapper_tests - Run all wrapper tests")
message(STATUS "")

# ============================================================================
# Usage instructions
# ============================================================================
# Build:
#   export OE_CMAKE_TOOLCHAIN_FILE=~/workspace/4.SDK/sysroots/x86_64-pokysdk-linux/usr/share/cmake/OEToolchainConfig.cmake
#   mkdir build && cd build
#   cmake -DCMAKE_TOOLCHAIN_FILE=$OE_CMAKE_TOOLCHAIN_FILE \
#   -DCMAKE_CXX_FLAGS="--sysroot=$SDKTARGETSYSROOT -I$SDKTARGETSYSROOT/usr/include/c++/13 -I$SDKTARGETSYSROOT/usr/include/c++/13/aarch64-poky-linux" \
#   -DCMAKE_C_FLAGS="--sysroot=$SDKTARGETSYSROOT" \
#   ..
#   make -j$(nproc)
#
# Run tests on target (Pi4):
#   scp build/test_wrapper_* pi@raspberrypi:~/
#   scp build/librpi_camera_wrapper.so pi@raspberrypi:~/
#   ssh pi@raspberrypi
#   export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
#   ./test_wrapper_basic
#   ./test_wrapper_formats
#   ./test_wrapper_controls
#   ./test_wrapper_stress
#   ./sample_camera_app