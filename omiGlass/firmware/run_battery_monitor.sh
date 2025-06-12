#!/bin/bash

# OpenGlass Battery Monitor Launcher
# This script activates the virtual environment and runs the battery monitor

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}🔋 OpenGlass Battery Monitor${NC}"
echo "=================================="

# Check if virtual environment exists
if [ ! -d "battery_monitor_env" ]; then
    echo -e "${RED}❌ Virtual environment not found!${NC}"
    echo "Please run: python3 -m venv battery_monitor_env"
    echo "Then: source battery_monitor_env/bin/activate && pip install pyserial matplotlib"
    exit 1
fi

# Check if serial port is provided
if [ $# -eq 0 ]; then
    echo -e "${YELLOW}📡 Available serial ports:${NC}"
    ls /dev/cu.* 2>/dev/null || echo "No USB serial devices found"
    echo ""
    echo -e "${YELLOW}Usage:${NC}"
    echo "  $0 <serial_port>"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0 /dev/cu.usbserial-0001"
    echo "  $0 /dev/cu.wchusbserial1410"
    exit 1
fi

SERIAL_PORT=$1

# Check if the serial port exists
if [ ! -e "$SERIAL_PORT" ]; then
    echo -e "${RED}❌ Serial port '$SERIAL_PORT' not found!${NC}"
    echo ""
    echo -e "${YELLOW}Available ports:${NC}"
    ls /dev/cu.* 2>/dev/null || echo "No USB serial devices found"
    exit 1
fi

echo -e "${GREEN}📡 Connecting to: $SERIAL_PORT${NC}"
echo -e "${YELLOW}📊 Starting real-time battery monitoring...${NC}"
echo -e "${YELLOW}   Close the plot window to stop monitoring${NC}"
echo ""

# Activate virtual environment and run the monitor
source battery_monitor_env/bin/activate
python3 battery_monitor.py "$SERIAL_PORT" 