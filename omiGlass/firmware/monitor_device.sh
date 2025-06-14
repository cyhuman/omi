#!/bin/bash

# ESP32-S3 Monitoring Helper Script
# Continuously tries to detect and monitor the ESP32-S3 device

echo "🔍 OMI Glass Monitoring Helper"
echo "==============================="

ATTEMPT=0
MAX_ATTEMPTS=30  # Try for 30 attempts (about 30 seconds)

monitor_device() {
    local device=$1
    echo "📺 Starting monitor on $device..."
    echo "Press Ctrl+C to exit monitoring"
    echo "================================="
    
    pio device monitor --port "$device" --baud 115200 --filter esp32_exception_decoder
}

detect_and_monitor() {
    while [ $ATTEMPT -lt $MAX_ATTEMPTS ]; do
        ATTEMPT=$((ATTEMPT + 1))
        echo "🔄 Attempt $ATTEMPT/$MAX_ATTEMPTS - Scanning for ESP32-S3..."
        
        # Try different device patterns
        for pattern in "/dev/cu.usbmodem*" "/dev/cu.usbserial*" "/dev/cu.wchusbserial*" "/dev/tty.usbmodem*" "/dev/tty.usbserial*"; do
            DEVICES=$(ls $pattern 2>/dev/null)
            if [ -n "$DEVICES" ]; then
                DEVICE=$(echo $DEVICES | head -n1 | cut -d' ' -f1)
                echo "✅ Found device: $DEVICE"
                monitor_device "$DEVICE"
                exit 0
            fi
        done
        
        echo "⏳ No device found, waiting 1 second..."
        sleep 1
    done
    
    echo "❌ No ESP32-S3 device found after $MAX_ATTEMPTS attempts"
    echo ""
    echo "📋 Try these steps:"
    echo "1. Press RESET button on ESP32-S3 (don't hold BOOT)"
    echo "2. Check USB cable connection"
    echo "3. Try a different USB port"
    echo "4. Verify the upload was successful"
    echo ""
    echo "🔧 Manual monitoring options:"
    echo "If you know the device name, try:"
    echo "  pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200"
    echo ""
    echo "💡 Current devices:"
    pio device list
}

# Show instructions
echo "This script will automatically detect and monitor your ESP32-S3"
echo "Make sure your device is connected and not in bootloader mode"
echo ""

# Start detection
detect_and_monitor 