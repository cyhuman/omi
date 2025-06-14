#!/bin/bash

# OMI Glass Firmware Build and Deployment Script
# Version: 2.0.0
# 
# This script provides comprehensive build, test, and deployment
# capabilities for the ESP32-S3 smart glasses firmware

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
ENVIRONMENTS=("xiao_esp32s3" "debug" "release")
DEFAULT_ENV="xiao_esp32s3"

# Function to print colored output
print_header() {
    echo -e "${PURPLE}═══════════════════════════════════════${NC}"
    echo -e "${PURPLE}    OMI Glass Firmware Builder v2.0.0${NC}"
    echo -e "${PURPLE}═══════════════════════════════════════${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Function to check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO CLI not found. Please install PlatformIO."
        echo "Install with: pip install platformio"
        exit 1
    fi
    
    if ! command -v esptool.py &> /dev/null; then
        print_warning "esptool.py not found. Installing..."
        pip install esptool
    fi
    
    print_success "All dependencies are available"
}

# Function to clean build files
clean_build() {
    print_info "Cleaning build files..."
    cd "$PROJECT_DIR"
    
    if [ -d ".pio" ]; then
        rm -rf .pio
        print_success "Removed .pio directory"
    fi
    
    if [ -d "build" ]; then
        rm -rf build
        print_success "Removed build directory"
    fi
    
    print_success "Build files cleaned"
}

# Function to validate configuration
validate_config() {
    print_info "Validating configuration..."
    
    if [ ! -f "config.h" ]; then
        print_error "config.h not found. Please ensure it exists."
        exit 1
    fi
    
    if [ ! -f "src/main.cpp" ]; then
        print_error "src/main.cpp not found. Please ensure it exists."
        exit 1
    fi
    
    if [ ! -f "platformio.ini" ]; then
        print_error "platformio.ini not found. Please ensure it exists."
        exit 1
    fi
    
    print_success "Configuration validation passed"
}

# Function to build firmware
build_firmware() {
    local env=${1:-$DEFAULT_ENV}
    print_info "Building firmware for environment: $env"
    
    cd "$PROJECT_DIR"
    
    # Build the firmware
    if pio run -e "$env"; then
        print_success "Firmware built successfully for $env"
        
        # Show build statistics
        if [ -f ".pio/build/$env/firmware.bin" ]; then
            local size=$(stat -f%z ".pio/build/$env/firmware.bin" 2>/dev/null || stat -c%s ".pio/build/$env/firmware.bin" 2>/dev/null)
            print_info "Firmware size: $(($size / 1024)) KB"
        fi
    else
        print_error "Build failed for environment: $env"
        exit 1
    fi
}

# Function to upload firmware
upload_firmware() {
    local env=${1:-$DEFAULT_ENV}
    local port=${2:-"auto"}
    
    print_info "Uploading firmware for environment: $env"
    
    cd "$PROJECT_DIR"
    
    if [ "$port" = "auto" ]; then
        if pio run -e "$env" --target upload; then
            print_success "Firmware uploaded successfully"
        else
            print_error "Upload failed"
            exit 1
        fi
    else
        if pio run -e "$env" --target upload --upload-port "$port"; then
            print_success "Firmware uploaded successfully to $port"
        else
            print_error "Upload failed to $port"
            exit 1
        fi
    fi
}

# Function to monitor serial output
monitor_serial() {
    local port=${1:-"auto"}
    
    print_info "Starting serial monitor..."
    
    cd "$PROJECT_DIR"
    
    if [ "$port" = "auto" ]; then
        pio device monitor --baud 115200 --filter esp32_exception_decoder
    else
        pio device monitor --port "$port" --baud 115200 --filter esp32_exception_decoder
    fi
}

# Function to run tests
run_tests() {
    print_info "Running firmware tests..."
    
    cd "$PROJECT_DIR"
    
    # Build test environment
    if pio run -e debug; then
        print_success "Test build completed"
    else
        print_error "Test build failed"
        exit 1
    fi
    
    # Static analysis
    print_info "Running static analysis..."
    if pio check; then
        print_success "Static analysis passed"
    else
        print_warning "Static analysis found issues"
    fi
}

# Function to show device information
show_device_info() {
    print_info "Scanning for ESP32 devices..."
    
    # List connected devices
    echo ""
    print_info "Connected devices:"
    pio device list
    
    echo ""
    print_info "Available environments:"
    for env in "${ENVIRONMENTS[@]}"; do
        echo "  - $env"
    done
}

# Function to create release package
create_release() {
    local version=${1:-"v2.0.0"}
    print_info "Creating release package: $version"
    
    cd "$PROJECT_DIR"
    
    # Build release version
    build_firmware "release"
    
    # Create release directory
    local release_dir="release_$version"
    mkdir -p "$release_dir"
    
    # Copy firmware files
    cp ".pio/build/release/firmware.bin" "$release_dir/omi_glass_firmware_$version.bin"
    cp ".pio/build/release/bootloader.bin" "$release_dir/bootloader.bin" 2>/dev/null || true
    cp ".pio/build/release/partitions.bin" "$release_dir/partitions.bin" 2>/dev/null || true
    
    # Create flash script
    cat > "$release_dir/flash.sh" << EOF
#!/bin/bash
# OMI Glass Firmware Flash Script
# Version: $version

echo "Flashing OMI Glass Firmware $version..."

# Check if esptool is available
if ! command -v esptool.py &> /dev/null; then
    echo "Error: esptool.py not found. Please install esptool."
    echo "Install with: pip install esptool"
    exit 1
fi

# Auto-detect port or use first argument
PORT=\${1:-"auto"}

if [ "\$PORT" = "auto" ]; then
    PORT=\$(pio device list | grep -o '/dev/tty[^ ]*' | head -1)
    if [ -z "\$PORT" ]; then
        PORT=\$(pio device list | grep -o 'COM[0-9]*' | head -1)
    fi
fi

if [ -z "\$PORT" ]; then
    echo "Error: No ESP32 device found. Please specify port manually:"
    echo "  ./flash.sh /dev/ttyUSB0"
    echo "  ./flash.sh COM3"
    exit 1
fi

echo "Using port: \$PORT"

# Flash the firmware
esptool.py --chip esp32s3 --port "\$PORT" --baud 921600 write_flash 0x10000 omi_glass_firmware_$version.bin

echo "Firmware flashed successfully!"
echo "You can now monitor the device with:"
echo "  pio device monitor --port \$PORT --baud 115200"
EOF
    
    chmod +x "$release_dir/flash.sh"
    
    # Create README
    cat > "$release_dir/README.md" << EOF
# OMI Glass Firmware $version

## Features
- Continuous audio streaming with μ-law compression
- Automatic photo capture every 30 seconds
- Advanced power management for 8+ hour battery life
- OMI protocol compliant BLE communication
- Real-time battery monitoring and protection
- Multi-core task architecture for optimal performance

## Flashing Instructions

### Option 1: Using the included script (Recommended)
\`\`\`bash
./flash.sh
\`\`\`

### Option 2: Manual flashing
\`\`\`bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x10000 omi_glass_firmware_$version.bin
\`\`\`

### Option 3: Using PlatformIO
\`\`\`bash
pio run --target upload
\`\`\`

## Monitoring
\`\`\`bash
pio device monitor --port /dev/ttyUSB0 --baud 115200
\`\`\`

## Configuration
The firmware is pre-configured for optimal battery life and OMI compatibility.
Default settings:
- Photo capture: Every 30 seconds
- Audio streaming: 16kHz, μ-law compressed
- Battery monitoring: Every 30 seconds
- BLE connection timeout: 5 minutes

## Support
For support and updates, visit: https://github.com/BasedHardware/omi
EOF
    
    print_success "Release package created: $release_dir"
    print_info "Contents:"
    ls -la "$release_dir"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  build [env]           Build firmware (default: $DEFAULT_ENV)"
    echo "  upload [env] [port]   Upload firmware to device"
    echo "  monitor [port]        Monitor serial output"
    echo "  test                  Run tests and static analysis"
    echo "  clean                 Clean build files"
    echo "  devices               Show connected devices"
    echo "  release [version]     Create release package"
    echo "  all [env]             Clean, build, and upload"
    echo "  help                  Show this help message"
    echo ""
    echo "Environments: ${ENVIRONMENTS[*]}"
    echo ""
    echo "Examples:"
    echo "  $0 build                    # Build with default environment"
    echo "  $0 build release            # Build release version"
    echo "  $0 upload debug /dev/ttyUSB0  # Upload debug version to specific port"
    echo "  $0 all release              # Clean, build, and upload release"
    echo "  $0 release v2.0.1           # Create release package"
}

# Main script logic
main() {
    print_header
    
    local command=${1:-"help"}
    
    case $command in
        "build")
            check_dependencies
            validate_config
            build_firmware "$2"
            ;;
        "upload")
            check_dependencies
            validate_config
            upload_firmware "$2" "$3"
            ;;
        "monitor")
            monitor_serial "$2"
            ;;
        "test")
            check_dependencies
            validate_config
            run_tests
            ;;
        "clean")
            clean_build
            ;;
        "devices")
            show_device_info
            ;;
        "release")
            check_dependencies
            validate_config
            create_release "$2"
            ;;
        "all")
            check_dependencies
            validate_config
            clean_build
            build_firmware "$2"
            upload_firmware "$2"
            print_success "Complete build and upload finished!"
            print_info "You can now monitor with: $0 monitor"
            ;;
        "help"|*)
            show_usage
            ;;
    esac
}

# Run main function with all arguments
main "$@" 