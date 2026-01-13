#!/bin/bash
# quick_test.sh - Quick test script for sample app
# Tests all major features in 30 seconds

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# ============================================================================
# Test 1: Quick capture (5 seconds)
# ============================================================================
test_quick_capture() {
    print_header "Test 1: Quick Capture (5s)"
    
    print_info "Starting camera..."
    
    # Send commands to app via stdin
    (
        sleep 2
        echo "q"  # Quit after 2 seconds
    ) | timeout 5 ./sample_camera_app 640 480 yuv > /tmp/test1.log 2>&1 || true
    
    # Check if frames were captured
    if grep -q "Total Frames:" /tmp/test1.log; then
        frames=$(grep "Total Frames:" /tmp/test1.log | awk '{print $3}')
        print_success "Captured $frames frames"
        
        if [ "$frames" -gt 30 ]; then
            print_success "Frame rate OK (>30 frames in ~2s)"
        else
            print_error "Low frame rate ($frames frames)"
        fi
    else
        print_error "No frames captured"
        return 1
    fi
}

# ============================================================================
# Test 2: Brightness control
# ============================================================================
test_brightness_control() {
    print_header "Test 2: Brightness Control"
    
    print_info "Testing brightness adjustment..."
    
    (
        sleep 1
        echo "b"      # Brightness menu
        echo "0.5"    # Set to 0.5
        sleep 1
        echo "i"      # Show info
        sleep 1
        echo "q"      # Quit
    ) | timeout 5 ./sample_camera_app 640 480 yuv > /tmp/test2.log 2>&1 || true
    
    if grep -q "Brightness set to" /tmp/test2.log; then
        print_success "Brightness control works"
    else
        print_error "Brightness control failed"
        return 1
    fi
}

# ============================================================================
# Test 3: Frame saving
# ============================================================================
test_frame_saving() {
    print_header "Test 3: Frame Saving"
    
    print_info "Testing frame saving..."
    
    # Clean old frames
    rm -rf ./captured_frames
    mkdir -p ./captured_frames
    
    (
        sleep 1
        echo "s"      # Enable saving
        sleep 3       # Capture for 3 seconds
        echo "q"      # Quit
    ) | timeout 6 ./sample_camera_app 640 480 yuv > /tmp/test3.log 2>&1 || true
    
    # Check saved frames
    saved_count=$(ls -1 ./captured_frames/*.yuv 2>/dev/null | wc -l)
    
    if [ "$saved_count" -gt 0 ]; then
        print_success "Saved $saved_count frames"
        
        # Check frame size
        frame_size=$(stat -f%z ./captured_frames/*.yuv 2>/dev/null | head -1 || stat -c%s ./captured_frames/*.yuv 2>/dev/null | head -1)
        expected_size=$((640 * 480 * 3 / 2))  # YUV420
        
        if [ "$frame_size" -eq "$expected_size" ]; then
            print_success "Frame size correct ($frame_size bytes)"
        else
            print_error "Frame size incorrect (got $frame_size, expected $expected_size)"
        fi
    else
        print_error "No frames saved"
        return 1
    fi
}

# ============================================================================
# Test 4: Multiple formats
# ============================================================================
test_multiple_formats() {
    print_header "Test 4: Multiple Formats"
    
    for format in yuv rgb mjpeg; do
        print_info "Testing format: $format"
        
        (
            sleep 1
            echo "q"
        ) | timeout 3 ./sample_camera_app 320 240 $format > /tmp/test4_$format.log 2>&1 || true
        
        if grep -q "Camera created successfully" /tmp/test4_$format.log; then
            print_success "Format $format works"
        else
            print_error "Format $format failed"
        fi
    done
}

# ============================================================================
# Test 5: Signal handling
# ============================================================================
test_signal_handling() {
    print_header "Test 5: Signal Handling (Ctrl+C)"
    
    print_info "Testing graceful shutdown..."
    
    # Start app in background
    ./sample_camera_app 640 480 yuv > /tmp/test5.log 2>&1 &
    app_pid=$!
    
    sleep 2
    
    # Send SIGINT (Ctrl+C)
    kill -INT $app_pid
    
    # Wait for cleanup
    wait $app_pid 2>/dev/null || true
    
    if grep -q "Signal received" /tmp/test5.log && grep -q "Cleanup complete" /tmp/test5.log; then
        print_success "Graceful shutdown works"
    else
        print_error "Signal handling failed"
        return 1
    fi
}

# ============================================================================
# Test 6: Control changes during capture
# ============================================================================
test_dynamic_controls() {
    print_header "Test 6: Dynamic Control Changes"
    
    print_info "Testing real-time control changes..."
    
    (
        sleep 1
        echo "b"      # Brightness
        echo "0.3"
        sleep 0.5
        echo "c"      # Contrast
        echo "1.5"
        sleep 0.5
        echo "e"      # Exposure
        echo "15000"
        sleep 0.5
        echo "g"      # Gain
        echo "2.0"
        sleep 0.5
        echo "i"      # Show info
        sleep 0.5
        echo "q"
    ) | timeout 6 ./sample_camera_app 640 480 yuv > /tmp/test6.log 2>&1 || true
    
    local success=0
    
    if grep -q "Brightness set to" /tmp/test6.log; then
        print_success "Brightness change OK"
        ((success++))
    fi
    
    if grep -q "Contrast set to" /tmp/test6.log; then
        print_success "Contrast change OK"
        ((success++))
    fi
    
    if grep -q "Exposure set to" /tmp/test6.log; then
        print_success "Exposure change OK"
        ((success++))
    fi
    
    if grep -q "Gain set to" /tmp/test6.log; then
        print_success "Gain change OK"
        ((success++))
    fi
    
    if [ $success -eq 4 ]; then
        print_success "All dynamic controls work"
    else
        print_error "Some controls failed ($success/4)"
        return 1
    fi
}

# ============================================================================
# Main
# ============================================================================
main() {
    echo ""
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║         RPI Camera Sample App - Quick Test Suite         ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo ""
    
    # Check if binary exists
    if [ ! -f "./sample_camera_app" ]; then
        print_error "sample_camera_app not found"
        print_info "Build it first with: make"
        exit 1
    fi
    
    # Check if library exists
    if [ ! -f "./librpi_camera_wrapper.so" ]; then
        print_error "librpi_camera_wrapper.so not found"
        print_info "Build it first with: make"
        exit 1
    fi
    
    # Set library path
    export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
    
    # Run tests
    local total_tests=6
    local passed_tests=0
    
    if test_quick_capture; then
        ((passed_tests++))
    fi
    echo ""
    
    if test_brightness_control; then
        ((passed_tests++))
    fi
    echo ""
    
    if test_frame_saving; then
        ((passed_tests++))
    fi
    echo ""
    
    if test_multiple_formats; then
        ((passed_tests++))
    fi
    echo ""
    
    if test_signal_handling; then
        ((passed_tests++))
    fi
    echo ""
    
    if test_dynamic_controls; then
        ((passed_tests++))
    fi
    echo ""
    
    # Summary
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║                        TEST SUMMARY                       ║"
    echo "╠═══════════════════════════════════════════════════════════╣"
    
    if [ $passed_tests -eq $total_tests ]; then
        echo -e "║ Result: ${GREEN}ALL TESTS PASSED${NC} ($passed_tests/$total_tests)                   ║"
        echo "╚═══════════════════════════════════════════════════════════╝"
        echo ""
        print_success "Sample app is working correctly!"
        echo ""
        print_info "Next steps:"
        echo "  1. Deploy to Pi: make deploy"
        echo "  2. Run on Pi: ssh pi@raspberrypi 'cd ~/camera_app && LD_LIBRARY_PATH=. ./sample_camera_app'"
        echo ""
        return 0
    else
        local failed=$((total_tests - passed_tests))
        echo -e "║ Result: ${RED}SOME TESTS FAILED${NC} ($passed_tests/$total_tests passed, $failed failed) ║"
        echo "╚═══════════════════════════════════════════════════════════╝"
        echo ""
        print_error "Some tests failed. Check logs in /tmp/test*.log"
        echo ""
        return 1
    fi
}

# Cleanup on exit
cleanup() {
    # Kill any remaining processes
    pkill -f sample_camera_app 2>/dev/null || true
}

trap cleanup EXIT

# Run main
main "$@"