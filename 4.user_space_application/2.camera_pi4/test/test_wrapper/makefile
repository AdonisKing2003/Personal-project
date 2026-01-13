# Makefile for RPI Camera Tests

CXX = g++
CC = gcc
CFLAGS = -Wall -Wextra -I..
CXXFLAGS = -Wall -Wextra -std=c++17 -I..
LDFLAGS = -lcamera -lcamera-base -lpthread

# Source files
CAMERA_SRC = ../rpi_camera.cpp
CAMERA_OBJ = rpi_camera.o

# Test executables
TESTS = test_basic test_formats test_controls test_stress

.PHONY: all clean run run_all

all: $(TESTS)

# Compile camera wrapper
$(CAMERA_OBJ): $(CAMERA_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build tests
test_basic: test_basic.c $(CAMERA_OBJ)
	$(CC) $(CFLAGS) $< $(CAMERA_OBJ) $(LDFLAGS) -o $@

test_formats: test_formats.c $(CAMERA_OBJ)
	$(CC) $(CFLAGS) $< $(CAMERA_OBJ) $(LDFLAGS) -o $@

test_controls: test_controls.c $(CAMERA_OBJ)
	$(CC) $(CFLAGS) $< $(CAMERA_OBJ) $(LDFLAGS) -o $@

test_stress: test_stress.c $(CAMERA_OBJ)
	$(CC) $(CFLAGS) $< $(CAMERA_OBJ) $(LDFLAGS) -o $@

# Run individual tests
run_basic: test_basic
	./test_basic

run_formats: test_formats
	./test_formats

run_controls: test_controls
	./test_controls

run_stress: test_stress
	./test_stress

# Run all tests
run_all: $(TESTS)
	@echo "================================================"
	@echo "Running Basic Tests..."
	@echo "================================================"
	./test_basic
	@echo ""
	@echo "================================================"
	@echo "Running Format Tests..."
	@echo "================================================"
	./test_formats
	@echo ""
	@echo "================================================"
	@echo "Running Control Tests..."
	@echo "================================================"
	./test_controls
	@echo ""
	@echo "================================================"
	@echo "Running Stress Tests..."
	@echo "================================================"
	./test_stress
	@echo ""
	@echo "================================================"
	@echo "ALL TESTS COMPLETED!"
	@echo "================================================"

# Clean
clean:
	rm -f $(TESTS) $(CAMERA_OBJ)
	rm -f *.o core

# Help
help:
	@echo "Available targets:"
	@echo "  make all          - Build all tests"
	@echo "  make run_all      - Run all tests"
	@echo "  make run_basic    - Run basic tests only"
	@echo "  make run_formats  - Run format tests only"
	@echo "  make run_controls - Run control tests only"
	@echo "  make run_stress   - Run stress tests only"
	@echo "  make clean        - Clean build files"