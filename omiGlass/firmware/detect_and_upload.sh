#!/bin/bash

# ESP32-S3 Detection and Upload Helper Script
# This script helps detect the ESP32-S3 device and upload firmware

echo "🔍 ESP32-S3 Device Detection and Upload Helper"
echo "=============================================="

# Function to detect ESP32-S3 device
detect_device() {
    echo "📡 Scanning for ESP32-S3 devices..."
    
    # Look for common ESP32-S3 device patterns
    DEVICE=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
    if [ -z "$DEVICE" ]; then
        DEVICE=$(ls /dev/cu.usbserial* 2>/dev/null | head -1)
    fi
    if [ -z "$DEVICE" ]; then
        DEVICE=$(ls /dev/cu.wchusbserial* 2>/dev/null | head -1)
    fi
    
    if [ -n "$DEVICE" ]; then
        echo "✅ Found device: $DEVICE"
        return 0
    else
        echo "❌ No ESP32-S3 device found"
        return 1
    fi
}

# Function to show device instructions
show_instructions() {
    echo ""
    echo "📋 Manual Bootloader Mode Instructions:"
    echo "1. Hold down the BOOT button on your ESP32-S3"
    echo "2. Press and release the RESET button (while holding BOOT)"
    echo "3. Release the BOOT button"
    echo "4. Run this script again"
    echo ""
    echo "💡 Current devices detected:"
    pio device list
}

# Function to upload firmware
upload_firmware() {
    local device=$1
    echo ""
    echo "🚀 Uploading firmware to $device..."
    
    # Build first if needed
    if [ ! -f ".pio/build/xiao_esp32s3/firmware.bin" ]; then
        echo "🔨 Building firmware first..."
        pio run -e xiao_esp32s3
    fi
    
    # Upload with the detected device
    echo "📤 Starting upload..."
    pio run -e xiao_esp32s3 --target upload --upload-port "$device"
    
    if [ $? -eq 0 ]; then
        echo "✅ Upload successful!"
        echo "🔍 You can monitor the device with:"
        echo "    pio device monitor --port $device --baud 115200"
    else
        echo "❌ Upload failed. Try putting the device in bootloader mode again."
    fi
}

# Main logic
echo ""
if detect_device; then
    read -p "🤔 Upload firmware to $DEVICE? (y/n): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        upload_firmware "$DEVICE"
    else
        echo "⏹️ Upload cancelled."
    fi
else
    show_instructions
    echo ""
    echo "🔄 After putting the device in bootloader mode, run:"
    echo "    ./detect_and_upload.sh"
fi 