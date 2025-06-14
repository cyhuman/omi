#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// BOARD CONFIGURATION - Must be defined before camera includes
// =============================================================================
#define CAMERA_MODEL_XIAO_ESP32S3  // Define camera model for Seeed Xiao ESP32S3
#define BOARD_HAS_PSRAM            // Enable PSRAM support
#define CONFIG_ARDUHAL_ESP_LOG     // Enable Arduino HAL logging

// =============================================================================
// DEVICE CONFIGURATION
// =============================================================================
#define BLE_DEVICE_NAME "OMI Glass"
#define FIRMWARE_VERSION_STRING "2.1.0"
#define HARDWARE_REVISION "ESP32-S3-v1.0"
#define MANUFACTURER_NAME "Based Hardware"

// =============================================================================
// POWER MANAGEMENT - Optimized for >14 hour battery life
// =============================================================================
// CPU Frequency Management
#define MAX_CPU_FREQ_MHZ 160          // Reduced from 240MHz for power savings
#define MIN_CPU_FREQ_MHZ 40           // Ultra-low power for idle states
#define NORMAL_CPU_FREQ_MHZ 80        // Normal operation frequency

// Sleep Management
#define LIGHT_SLEEP_DURATION_US 50000   // 50ms light sleep intervals
#define DEEP_SLEEP_THRESHOLD_MS 300000  // 5 minutes of inactivity triggers deep sleep
#define IDLE_THRESHOLD_MS 30000         // 30 seconds to enter power save mode

// Battery Configuration - Dual 450mAh @ 3.7V-4.3V
#define BATTERY_MAX_VOLTAGE 4.3f
#define BATTERY_MIN_VOLTAGE 3.7f      
#define BATTERY_CRITICAL_VOLTAGE 3.6f  // Emergency shutdown voltage
#define BATTERY_LOW_VOLTAGE 3.8f       // Low battery warning
#define VOLTAGE_DIVIDER_RATIO 1.862f   // Precisely calibrated to match multimeter readings

// Battery Monitoring - Power optimized intervals
#define BATTERY_REPORT_INTERVAL_MS 60000    // 1 minute reporting
#define BATTERY_TASK_INTERVAL_MS 10000      // 10 second internal checks
#define BATTERY_ADC_PIN 2                   // GPIO2 (A1) - voltage divider connection

// =============================================================================
// CAMERA CONFIGURATION - Power optimized for long battery life
// =============================================================================
#define CAMERA_FRAME_SIZE FRAMESIZE_VGA     // 640x480 - optimal balance
#define CAMERA_JPEG_QUALITY 20              // Reduced quality for power savings
#define CAMERA_XCLK_FREQ 6000000           // 6MHz - reduced from 8MHz for power
#define CAMERA_FB_IN_PSRAM CAMERA_FB_IN_PSRAM
#define CAMERA_GRAB_LATEST CAMERA_GRAB_LATEST

// Fixed Photo Capture Interval - No adaptive mode
#define PHOTO_CAPTURE_INTERVAL_MS 30000    // Fixed 30 second interval for all battery levels
#define CAMERA_TASK_INTERVAL_MS 2000              // 2 second task check
#define CAMERA_TASK_STACK_SIZE 3072               // Reduced stack size
#define CAMERA_TASK_PRIORITY 2

// Camera Power Management
#define CAMERA_POWER_DOWN_DELAY_MS 5000     // Power down camera after 5s idle

// =============================================================================
// BLE CONFIGURATION - Power optimized OMI Protocol
// =============================================================================
#define BLE_MTU_SIZE 517                    // Maximum MTU for efficiency
#define BLE_CHUNK_SIZE 500                  // Safe chunk size for photo transfer
#define BLE_PHOTO_TRANSFER_DELAY 10         // Increased delay for power savings
#define BLE_TX_POWER ESP_PWR_LVL_P3         // Reduced power (was P9)

// Power-optimized BLE Advertising
#define BLE_ADV_MIN_INTERVAL 0x0100         // 160ms minimum (was 20ms)
#define BLE_ADV_MAX_INTERVAL 0x0200         // 320ms maximum (was 40ms)
#define BLE_ADV_TIMEOUT_MS 300000           // Stop advertising after 5 minutes
#define BLE_SLEEP_ADV_INTERVAL 60000        // Re-advertise every 1 minute when not connected

// Connection Management - Power aware
#define BLE_CONNECTION_TIMEOUT_MS 180000    // 3 minutes before power save
#define BLE_TASK_INTERVAL_MS 15000          // 15 second connection check
#define BLE_TASK_STACK_SIZE 2048
#define BLE_TASK_PRIORITY 1

// Connection Parameters for Power Optimization
#define BLE_CONN_MIN_INTERVAL 24            // 30ms minimum connection interval
#define BLE_CONN_MAX_INTERVAL 48            // 60ms maximum connection interval
#define BLE_CONN_LATENCY 4                  // Allow slave to skip 4 connection events
#define BLE_CONN_TIMEOUT 400                // 4 second supervision timeout

// =============================================================================
// POWER STATES
// =============================================================================
typedef enum {
    POWER_STATE_ACTIVE,      // Normal operation - camera + BLE active
    POWER_STATE_POWER_SAVE,  // Reduced frequency, longer intervals
    POWER_STATE_LOW_BATTERY, // Minimal operation
    POWER_STATE_SLEEP        // Deep sleep mode
} power_state_t;

// =============================================================================
// TASK CONFIGURATION - Optimized stack sizes
// =============================================================================
#define BATTERY_TASK_STACK_SIZE 2048
#define BATTERY_TASK_PRIORITY 1
#define POWER_MANAGEMENT_TASK_STACK_SIZE 2048
#define POWER_MANAGEMENT_TASK_PRIORITY 0

// Status Reporting - Power optimized
#define STATUS_REPORT_INTERVAL_MS 120000    // 2 minutes (was 30 seconds)

// =============================================================================
// BLE UUID DEFINITIONS - OMI Protocol
// =============================================================================
#define OMI_SERVICE_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define AUDIO_DATA_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"
#define AUDIO_CONTROL_UUID "19B10002-E8F2-537E-4F6C-D104768A1214"
#define PHOTO_DATA_UUID "19B10005-E8F2-537E-4F6C-D104768A1214"
#define PHOTO_CONTROL_UUID "19B10006-E8F2-537E-4F6C-D104768A1214"

// Battery Service UUID - Cast to uint16_t for BLE compatibility
#define BATTERY_SERVICE_UUID (uint16_t)0x180F
#define BATTERY_LEVEL_UUID (uint16_t)0x2A19

// =============================================================================
// PIN DEFINITIONS (from camera_pins.h integration)
// =============================================================================
#ifdef CAMERA_MODEL_XIAO_ESP32S3
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     10
  #define SIOD_GPIO_NUM     40
  #define SIOC_GPIO_NUM     39
  #define Y9_GPIO_NUM       48
  #define Y8_GPIO_NUM       11
  #define Y7_GPIO_NUM       12
  #define Y6_GPIO_NUM       14
  #define Y5_GPIO_NUM       16
  #define Y4_GPIO_NUM       18
  #define Y3_GPIO_NUM       17
  #define Y2_GPIO_NUM       15
  #define VSYNC_GPIO_NUM    38
  #define HREF_GPIO_NUM     47
  #define PCLK_GPIO_NUM     13
  
  // Power Button and LED Control
  #define POWER_BUTTON_PIN  1           // Custom button (GPIO1/A0) - power on/off
  #define STATUS_LED_PIN    21          // User LED (GPIO21) - status indicator
#endif

// =============================================================================
// POWER BUTTON & LED CONFIGURATION
// =============================================================================
// Button Configuration
#define BUTTON_DEBOUNCE_MS 50             // Button debounce time
#define POWER_OFF_PRESS_MS 2000           // Long press duration for power off (2 seconds)
#define BOOT_COMPLETE_DELAY_MS 3000       // LED indication during boot

// LED Status Patterns (in milliseconds)
#define LED_BOOT_BLINK_FAST 200           // Fast blink during boot
#define LED_BATTERY_LOW_BLINK 1000        // Slow blink for low battery
#define LED_SLEEP_BLINK 5000              // Very slow blink in deep sleep mode
#define LED_PHOTO_CAPTURE_FLASH 100       // Quick flash during photo capture

// Deep Sleep Configuration  
#define DEEP_SLEEP_BUTTON_WAKEUP 1        // Enable button wake-up from deep sleep
#define POWER_OFF_SLEEP_DELAY_MS 1000     // Delay before entering deep sleep after power off

// Power Button States
typedef enum {
    BUTTON_IDLE,
    BUTTON_PRESSED,
    BUTTON_LONG_PRESS,
    BUTTON_RELEASED
} button_state_t;

// LED Status Modes
typedef enum {
    LED_OFF,
    LED_ON,
    LED_BOOT_SEQUENCE,
    LED_NORMAL_OPERATION, 
    LED_LOW_BATTERY,
    LED_PHOTO_CAPTURE,
    LED_POWER_OFF_SEQUENCE,
    LED_SLEEP_MODE
} led_status_t;

// Device Power States
typedef enum {
    DEVICE_BOOTING,
    DEVICE_ACTIVE,
    DEVICE_POWER_SAVE,
    DEVICE_LOW_BATTERY,
    DEVICE_POWERING_OFF,
    DEVICE_SLEEP
} device_state_t;

#endif // CONFIG_H 