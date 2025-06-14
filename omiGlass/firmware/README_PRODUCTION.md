# OpenGlass Pro Production Firmware v2.0.0

A production-quality firmware implementation for ESP32-S3 based smart glasses with advanced power management, audio streaming, and battery protection.

## 🚀 Key Features

### Core Functionality
- **📸 Automated Photo Capture**: 30-second interval photos with optimized JPEG compression
- **🎵 Real-time Audio Streaming**: 16kHz μ-law compressed audio over BLE
- **🔋 Advanced Battery Management**: Dual 500mAh battery monitoring with protection
- **📱 BLE Communication**: Reliable wireless communication with mobile devices
- **⚡ Power Optimization**: 8+ hour battery life with intelligent sleep modes

### Production Features
- **🛡️ Battery Protection**: Automatic shutdown at critical voltage levels
- **🔧 Task-based Architecture**: FreeRTOS multi-tasking for reliable operation
- **📊 System Monitoring**: Real-time status reporting and health checks
- **⚙️ Configurable Parameters**: Easy customization via config.h
- **🔒 Thread Safety**: Mutex-protected resource access
- **💾 Memory Management**: Optimized PSRAM usage and leak prevention

## 📋 Requirements

### Hardware
- **MCU**: Seeed XIAO ESP32S3 Sense
- **Camera**: Built-in OV2640 camera module
- **Microphone**: I2S PDM microphone (pins 41/42)
- **Battery**: 2× 500mAh Li-Po batteries (3.7V)
- **Voltage Divider**: R1=169kΩ, R2=110kΩ (for battery monitoring)

### Software
- **Platform**: PlatformIO with ESP32 Arduino framework
- **IDE**: VS Code with PlatformIO extension (recommended)
- **Libraries**: All dependencies managed via platformio.ini

## 🛠️ Installation & Setup

### 1. Hardware Setup

#### Battery Connection
```
Battery+ ----[R1: 169kΩ]----+----[R2: 110kΩ]---- Battery-
                             |
                          ADC Pin A0
```

#### Audio Connection
- **PDM Clock**: GPIO 42
- **PDM Data**: GPIO 41

### 2. Software Installation

#### Using PlatformIO CLI
```bash
# Clone the repository
git clone <repository-url>
cd omiGlass/firmware

# Install dependencies
pio lib install

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor
```

#### Using PlatformIO IDE
1. Open the `omiGlass/firmware` folder in VS Code
2. Install PlatformIO extension if not already installed
3. Click "Build" then "Upload" in the PlatformIO toolbar
4. Open Serial Monitor to view output

### 3. Configuration

Edit `config.h` to customize firmware behavior:

```cpp
// Example: Change photo interval to 60 seconds
#define PHOTO_CAPTURE_INTERVAL_MS 60000

// Example: Enable debug output
#define SERIAL_DEBUG 1

// Example: Adjust battery thresholds
#define BATTERY_CRITICAL_VOLTAGE 3.5f
```

## 🏗️ Architecture

### Task Structure
```
┌─────────────────┐    ┌─────────────────┐
│   Camera Task   │    │   Audio Task    │
│   Priority: 2   │    │   Priority: 3   │
│   Core: 0       │    │   Core: 1       │
└─────────────────┘    └─────────────────┘
┌─────────────────┐    ┌─────────────────┐
│  Battery Task   │    │    BLE Task     │
│   Priority: 1   │    │   Priority: 2   │
│   Core: 0       │    │   Core: 1       │
└─────────────────┘    └─────────────────┘
```

### Power Management Flow
```
Power On → Initialize → BLE Advertising
    ↓
Connected? → No → Deep Sleep (5min timeout)
    ↓ Yes
Photo/Audio Tasks → Light Sleep (between operations)
    ↓
Battery Critical? → Yes → Emergency Shutdown
    ↓ No
Continue Operation
```

## 📡 BLE Protocol

### Services

#### Main Service (`19B10000-E8F2-537E-4F6C-D104768A1214`)
- **Photo Data** (`19B10005`): Chunked photo transmission
- **Photo Control** (`19B10006`): Photo capture commands

#### Audio Service (`19B10010-E8F2-537E-4F6C-D104768A1214`)
- **Audio Data** (`19B10011`): Real-time audio stream
- **Audio Control** (`19B10012`): Audio streaming commands

#### Standard Services
- **Battery Service** (`0x180F`): Battery level reporting
- **Device Info** (`0x180A`): Device identification

### Commands

#### Photo Control
- `0`: Stop photo capture
- `-1`: Single photo
- `5-300`: Interval capture (uses 30s regardless)

#### Audio Control
- `0`: Stop audio streaming
- `1`: Start audio streaming

## 🔋 Power Consumption

### Estimated Current Draw
| Component | Active | Sleep | Notes |
|-----------|--------|-------|-------|
| **CPU** | 20mA | 2mA | Dynamic frequency scaling |
| **Camera** | 8mA | 0mA | Power-down between captures |
| **BLE** | 10mA | 0.5mA | Low power advertising |
| **Audio** | 25mA | 0mA | When streaming active |
| **Total** | ~63mA | ~2.5mA | 8+ hour battery life |

### Battery Life Calculation
- **Capacity**: 2× 500mAh = 1000mAh total
- **Usable**: 800mAh (80% for protection)
- **Average Draw**: ~63mA
- **Expected Life**: 12+ hours continuous use

## 🛡️ Safety Features

### Battery Protection
- **Critical Voltage**: 3.6V (automatic shutdown)
- **Low Warning**: 20% charge level
- **Hysteresis**: 50mV to prevent oscillation
- **Emergency Signal**: BLE notification before shutdown

### Thermal Protection
- **Automatic Frequency Scaling**: Reduces heat generation
- **Task Prioritization**: Critical tasks have priority
- **Watchdog Timer**: System reset on hangs (configurable)

### Error Recovery
- **Automatic Retry**: Up to 3 attempts for failed operations
- **Resource Cleanup**: Proper cleanup on errors
- **Graceful Degradation**: Continue operation with reduced features

## 📊 Monitoring & Debugging

### Serial Output
```
=== OpenGlass Pro Firmware v2.0.0 ===
Configuring power management...
Power management configured successfully
Battery: 4.15V (95%)
Camera initialized successfully
BLE initialized and advertising started
Status: Connected=Yes, Photos=Yes, Audio=No, Battery=4.15V (95%)
```

### Performance Metrics
- **Heap Usage**: Monitored every 60 seconds
- **Task Stack**: High water mark tracking
- **Battery Trending**: Voltage history tracking
- **Error Counts**: Accumulated error statistics

## 🔧 Troubleshooting

### Common Issues

#### Camera Not Working
- Check camera connector
- Verify PSRAM availability
- Review camera pin configuration

#### Audio Issues
- Verify I2S pin connections (GPIO 41/42)
- Check microphone power supply
- Confirm audio task priority

#### Battery Monitoring
- Verify voltage divider (R1=169kΩ, R2=110kΩ)
- Check ADC pin A0 connection
- Calibrate voltage readings if needed

#### BLE Connection Problems
- Check advertising intervals
- Verify service UUIDs
- Monitor connection timeout

### Debug Build
Use the debug environment for detailed logging:
```bash
pio run -e seeed_xiao_esp32s3_debug -t upload
```

### Memory Analysis
Enable heap monitoring in config.h:
```cpp
#define ENABLE_PERFORMANCE_MONITOR true
#define HEAP_MONITOR_INTERVAL_MS 10000
```

## 🎛️ Configuration Options

### Key Parameters (config.h)

```cpp
// Timing
#define PHOTO_CAPTURE_INTERVAL_MS  30000    // Photo interval
#define AUDIO_PACKET_SIZE_MS       10       // Audio packet size
#define STATUS_REPORT_INTERVAL_MS  30000    // Status reporting

// Power Management
#define MAX_CPU_FREQ_MHZ          240       // Maximum CPU speed
#define MIN_CPU_FREQ_MHZ          10        // Minimum CPU speed
#define DEEP_SLEEP_TIMEOUT_MS     300000    // Deep sleep timeout

// Camera
#define CAMERA_FRAME_SIZE         FRAMESIZE_VGA  // 640x480
#define CAMERA_JPEG_QUALITY       12        // Quality level
#define CAMERA_XCLK_FREQ          10000000  // Camera clock

// Audio
#define AUDIO_SAMPLE_RATE         16000     // Sample rate
#define SAMPLES_PER_PACKET        160       // Packet size

// Battery
#define BATTERY_CRITICAL_VOLTAGE  3.6f      // Shutdown voltage
#define VOLTAGE_DIVIDER_RATIO     2.536f    // Hardware specific

// BLE
#define BLE_CHUNK_SIZE           244        // Maximum chunk
#define BLE_TX_POWER             ESP_PWR_LVL_N12  // Low power
```

## 📈 Performance Optimization

### Recommendations
1. **Adjust photo quality** based on image requirements
2. **Tune audio sample rate** for bandwidth/quality balance
3. **Optimize BLE intervals** for connection stability
4. **Configure sleep modes** for maximum battery life
5. **Monitor task stack usage** to prevent overflows

### Advanced Tuning
- Use `seeed_xiao_esp32s3_release` environment for production
- Enable compiler optimizations (`-Os` flag)
- Profile memory usage with heap monitoring
- Adjust task priorities based on requirements

## 🤝 Contributing

1. Follow the existing code style and architecture
2. Test thoroughly with different battery levels
3. Verify power consumption meets requirements
4. Update documentation for configuration changes
5. Use the debug build for development

## 📄 License

This firmware is part of the OpenGlass project. Please refer to the main project license.

## 🔗 Related Documentation

- [Hardware Assembly Guide](../hardware/)
- [Mobile App Integration](../../app/)
- [BLE Protocol Specification](./docs/ble-protocol.md)
- [Power Optimization Guide](./docs/power-optimization.md)

---

**Version**: 2.0.0  
**Last Updated**: 2024  
**Compatibility**: ESP32-S3, Arduino Framework  
**Minimum Battery Life**: 8+ hours  
**Status**: Production Ready ✅ 