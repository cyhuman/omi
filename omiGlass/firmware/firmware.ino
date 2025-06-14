#define CAMERA_MODEL_XIAO_ESP32S3
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include "config.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"

// ---------------------------------------------------------------------------------
// Power Management & Battery
// ---------------------------------------------------------------------------------
power_state_t currentPowerState = POWER_STATE_ACTIVE;
device_state_t deviceState = DEVICE_BOOTING;
float batteryVoltage = 4.0f;
int batteryPercentage = 100;
unsigned long lastBatteryCheck = 0;
unsigned long lastActivityTime = 0;

// Power Button & LED Management
button_state_t buttonState = BUTTON_IDLE;
led_status_t ledStatus = LED_BOOT_SEQUENCE;
unsigned long buttonPressStart = 0;
unsigned long ledLastToggle = 0;
bool ledState = false;
bool devicePoweredOn = true;

// System state variables

// Power management functions
void updatePowerState();
void setBatteryAwareCPUFreq();
void readBatteryLevel();
float getBatteryPercentage(float voltage);
int getFixedPhotoInterval();

// Power Button & LED functions
void initPowerButton();
void initStatusLED();
void handlePowerButton();
void updateStatusLED();
void startPowerOffSequence();
void enterDeepSleep();
void handleWakeUp();

// ---------------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------------

// Device Information Service
#define DEVICE_INFORMATION_SERVICE_UUID (uint16_t)0x180A
#define MANUFACTURER_NAME_STRING_CHAR_UUID (uint16_t)0x2A29
#define MODEL_NUMBER_STRING_CHAR_UUID (uint16_t)0x2A24
#define FIRMWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A26
#define HARDWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A27

// Main Friend Service
static BLEUUID serviceUUID("19B10000-E8F2-537E-4F6C-D104768A1214");
static BLEUUID photoDataUUID("19B10005-E8F2-537E-4F6C-D104768A1214");
static BLEUUID photoControlUUID("19B10006-E8F2-537E-4F6C-D104768A1214");

// Characteristics
BLECharacteristic *photoDataCharacteristic;
BLECharacteristic *photoControlCharacteristic;
BLECharacteristic *batteryLevelCharacteristic;

// State
bool connected = false;
bool isCapturingPhotos = false;
int captureInterval = 0;
unsigned long lastCaptureTime = 0;
unsigned long lastAdvertiseTime = 0;
bool advertisingActive = false;

size_t sent_photo_bytes = 0;
size_t sent_photo_frames = 0;
bool photoDataUploading = false;

// -------------------------------------------------------------------------
// Camera Frame & Power Management
// -------------------------------------------------------------------------
camera_fb_t *fb = nullptr;
bool cameraActive = false;
unsigned long lastCameraUse = 0;

// Forward declarations
void handlePhotoControl(int8_t controlValue);
void powerDownCamera();
void powerUpCamera();

class ServerHandler : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    connected = true;
    lastActivityTime = millis();
    Serial.println(">>> BLE Client connected.");
    
    // Boost CPU frequency for stable connection
    setCpuFrequencyMhz(NORMAL_CPU_FREQ_MHZ);
    
    // Request optimized connection parameters for stability
    server->updateConnParams(server->getConnId(), BLE_CONN_MIN_INTERVAL, BLE_CONN_MAX_INTERVAL, BLE_CONN_LATENCY, BLE_CONN_TIMEOUT);
    Serial.printf("Requested stable connection parameters: %dms interval, %ds timeout\n", BLE_CONN_MIN_INTERVAL * 1.25, BLE_CONN_TIMEOUT / 100);
  }
  
  void onDisconnect(BLEServer *server) override {
    connected = false;
    Serial.println("<<< BLE Client disconnected - Immediately restarting advertising");
    
    // Immediately restart advertising for reconnection
    advertisingActive = true;
    lastAdvertiseTime = millis();
    BLEDevice::startAdvertising();
    
    // Keep normal CPU frequency for quick reconnection
    setCpuFrequencyMhz(NORMAL_CPU_FREQ_MHZ);
  }
};

class PhotoControlCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    lastActivityTime = millis(); // Reset activity timer
    if (characteristic->getLength() == 1) {
      int8_t received = characteristic->getData()[0];
      Serial.print("PhotoControl received: ");
      Serial.println(received);
      handlePhotoControl(received);
    }
  }
};

// -------------------------------------------------------------------------
// Power Management Functions
// -------------------------------------------------------------------------
void updatePowerState() {
  unsigned long now = millis();
  
  // Update device state based on battery and activity
  switch (deviceState) {
    case DEVICE_BOOTING:
      // Handled in LED update function
      break;
      
    case DEVICE_ACTIVE:
      if (batteryPercentage < 10) {
        deviceState = DEVICE_LOW_BATTERY;
        currentPowerState = POWER_STATE_SLEEP;
        ledStatus = LED_LOW_BATTERY;
      } else if (batteryPercentage < 20) {
        currentPowerState = POWER_STATE_LOW_BATTERY;
      } else if (batteryPercentage < 50 || (now - lastActivityTime > IDLE_THRESHOLD_MS)) {
        currentPowerState = POWER_STATE_POWER_SAVE;
      } else {
        currentPowerState = POWER_STATE_ACTIVE;
      }
      break;
      
    case DEVICE_LOW_BATTERY:
      currentPowerState = POWER_STATE_SLEEP;
      ledStatus = LED_LOW_BATTERY;
      // Only auto power off if battery voltage reading seems reliable
      if (batteryPercentage < 5 && batteryVoltage > 2.5 && batteryVoltage < 5.0) {
        Serial.println("Critical battery - auto power off");
        startPowerOffSequence();
      } else if (batteryVoltage < 2.5) {
        Serial.printf("Battery voltage too low (%.2fV) - possible ADC issue, not auto-powering off\n", batteryVoltage);
      }
      break;
      
    case DEVICE_POWERING_OFF:
    case DEVICE_SLEEP:
      // Don't change states during power off or sleep
      break;
      
    default:
      deviceState = DEVICE_ACTIVE;
      break;
  }
  
  setBatteryAwareCPUFreq();
}

void setBatteryAwareCPUFreq() {
  switch (currentPowerState) {
    case POWER_STATE_ACTIVE:
      setCpuFrequencyMhz(NORMAL_CPU_FREQ_MHZ);
      break;
    case POWER_STATE_POWER_SAVE:
      setCpuFrequencyMhz(MIN_CPU_FREQ_MHZ * 2);
      break;
    case POWER_STATE_LOW_BATTERY:
      setCpuFrequencyMhz(MIN_CPU_FREQ_MHZ);
      break;
    case POWER_STATE_SLEEP:
      setCpuFrequencyMhz(MIN_CPU_FREQ_MHZ);
      break;
  }
}

void readBatteryLevel() {
  // Set ADC resolution and attenuation for accurate readings
  analogReadResolution(12); // 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // Full scale voltage 3.9V
  
  // Take multiple readings for stability
  uint32_t adcSum = 0;
  for (int i = 0; i < 10; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delay(1);
  }
  uint32_t adcValue = adcSum / 10;
  
  // Convert ADC reading to voltage with proper scaling
  float adcVoltage = (adcValue / 4095.0) * 3.9; // 3.9V with 11dB attenuation
  batteryVoltage = adcVoltage * VOLTAGE_DIVIDER_RATIO;
  
  // Check if readings are realistic
  bool readingsRealistic = (adcValue > 100 && batteryVoltage > 3.0);
  
  // Clamp voltage to reasonable range only if readings seem unrealistic
  if (!readingsRealistic) {
    if (batteryVoltage < 2.0) batteryVoltage = 2.0; // Minimum reasonable voltage
    if (batteryVoltage > 5.0) batteryVoltage = 5.0; // Maximum reasonable voltage
  }
  
  batteryPercentage = (int)getBatteryPercentage(batteryVoltage);
  
  // Update BLE battery service if connected
  if (connected && batteryLevelCharacteristic) {
    uint8_t batteryLevel = (uint8_t)batteryPercentage;
    batteryLevelCharacteristic->setValue(&batteryLevel, 1);
    batteryLevelCharacteristic->notify();
  }
  
  Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage, batteryPercentage);
}

float getBatteryPercentage(float voltage) {
  if (voltage >= BATTERY_MAX_VOLTAGE) return 100.0;
  if (voltage <= BATTERY_MIN_VOLTAGE) return 0.0;
  return ((voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0;
}

int getFixedPhotoInterval() {
  return PHOTO_CAPTURE_INTERVAL_MS; // Always return fixed 30-second interval
}

void powerDownCamera() {
  if (cameraActive) {
    Serial.println("Powering down camera for battery savings");
    // Note: esp_camera doesn't have a direct power down, but we can deinit
    cameraActive = false;
  }
}

void powerUpCamera() {
  if (!cameraActive) {
    Serial.println("Powering up camera");
    cameraActive = true;
  }
}

// -------------------------------------------------------------------------
// configure_ble()
// -------------------------------------------------------------------------
void configure_ble() {
  Serial.println("Initializing BLE...");
  BLEDevice::init("OMI Glass");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerHandler());

  // Main service
  BLEService *service = server->createService(serviceUUID);

  // Photo Data characteristic
  photoDataCharacteristic = service->createCharacteristic(
      photoDataUUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLE2902 *ccc = new BLE2902();
  ccc->setNotifications(true);
  photoDataCharacteristic->addDescriptor(ccc);

  // Photo Control characteristic
  photoControlCharacteristic = service->createCharacteristic(
      photoControlUUID,
      BLECharacteristic::PROPERTY_WRITE);
  photoControlCharacteristic->setCallbacks(new PhotoControlCallback());
  uint8_t controlValue = 0;
  photoControlCharacteristic->setValue(&controlValue, 1);

  // Battery Service
  BLEService *batteryService = server->createService(BATTERY_SERVICE_UUID);
  batteryLevelCharacteristic = batteryService->createCharacteristic(
      BATTERY_LEVEL_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  uint8_t initialBatteryLevel = (uint8_t)batteryPercentage;
  batteryLevelCharacteristic->setValue(&initialBatteryLevel, 1);

  // Device Information Service
  BLEService *deviceInfoService = server->createService(DEVICE_INFORMATION_SERVICE_UUID);
  BLECharacteristic *manufacturerNameCharacteristic =
      deviceInfoService->createCharacteristic(MANUFACTURER_NAME_STRING_CHAR_UUID,
                                              BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *modelNumberCharacteristic =
      deviceInfoService->createCharacteristic(MODEL_NUMBER_STRING_CHAR_UUID,
                                              BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *firmwareRevisionCharacteristic =
      deviceInfoService->createCharacteristic(FIRMWARE_REVISION_STRING_CHAR_UUID,
                                              BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *hardwareRevisionCharacteristic =
      deviceInfoService->createCharacteristic(HARDWARE_REVISION_STRING_CHAR_UUID,
                                              BLECharacteristic::PROPERTY_READ);

  manufacturerNameCharacteristic->setValue(MANUFACTURER_NAME);
  modelNumberCharacteristic->setValue("OMI Glass");
  firmwareRevisionCharacteristic->setValue(FIRMWARE_VERSION_STRING);
  hardwareRevisionCharacteristic->setValue(HARDWARE_REVISION);

  // Start services
  service->start();
  batteryService->start();
  deviceInfoService->start();

  // Configure power-optimized advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  advertising->addServiceUUID(batteryService->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(BLE_ADV_MIN_INTERVAL);
  advertising->setMaxPreferred(BLE_ADV_MAX_INTERVAL);
  
  // Add device name to advertising data for better discoverability
  BLEAdvertisementData advertisementData;
  advertisementData.setName("Omi Glass");
  advertisementData.setCompleteServices(service->getUUID());
  advertisementData.setCompleteServices(batteryService->getUUID());
  advertising->setAdvertisementData(advertisementData);
  
  advertisingActive = true;
  lastAdvertiseTime = millis();
  BLEDevice::startAdvertising();

  Serial.println("BLE initialized with power optimization.");
}

// -------------------------------------------------------------------------
// Camera Functions
// -------------------------------------------------------------------------
bool take_photo() {
  // Ensure camera is powered up
  powerUpCamera();
  lastCameraUse = millis();
  
  // Release previous buffer
  if (fb) {
    Serial.println("Releasing previous camera buffer...");
    esp_camera_fb_return(fb);
    fb = nullptr;
  }

  Serial.println("Capturing photo...");
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame buffer!");
    return false;
  }
  Serial.printf("Photo captured: %d bytes.\n", fb->len);
  return true;
}

void handlePhotoControl(int8_t controlValue) {
  lastActivityTime = millis(); // Reset activity timer
  
  if (controlValue == -1) {
    Serial.println("Received command: Single photo.");
    isCapturingPhotos = true;
    captureInterval = 0;
  }
  else if (controlValue == 0) {
    Serial.println("Received command: Stop photo capture.");
    isCapturingPhotos = false;
    captureInterval = 0;
  }
  else if (controlValue >= 5 && controlValue <= 300) {
    Serial.printf("Received command: Start interval capture (fixed)\n");
    captureInterval = getFixedPhotoInterval();
    isCapturingPhotos = true;
    lastCaptureTime = millis() - captureInterval;
    Serial.printf("Using fixed interval: %d seconds\n", captureInterval / 1000);
  }
}

// -------------------------------------------------------------------------
// configure_camera()
// -------------------------------------------------------------------------
void configure_camera() {
  Serial.println("Initializing camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = CAMERA_XCLK_FREQ;

  // Power-optimized camera settings
  config.frame_size   = CAMERA_FRAME_SIZE;
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_count     = 1;
  config.jpeg_quality = CAMERA_JPEG_QUALITY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  }
  else {
    Serial.println("Camera initialized with power optimization.");
    cameraActive = true;
    lastCameraUse = millis();
  }
}

// -------------------------------------------------------------------------
// Power Button & LED Management Functions
// -------------------------------------------------------------------------
void initPowerButton() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  
  // Check if device woke up from deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.printf("Woke up: Custom button (GPIO%d)\n", POWER_BUTTON_PIN);
      handleWakeUp();
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Woke up: Boot button (GPIO0)");
      handleWakeUp();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Woke up: Timer (1 hour)");
      handleWakeUp();
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Serial.println("Normal boot");
      break;
    default:
      Serial.printf("Woke up: Unknown reason (%d)\n", wakeup_reason);
      handleWakeUp();
      break;
  }
  
  // Configure buttons as wake-up sources for deep sleep
  esp_err_t ext0_result = esp_sleep_enable_ext0_wakeup((gpio_num_t)POWER_BUTTON_PIN, 0);
  uint64_t ext1_mask = (1ULL << 0); // GPIO0 boot button
  esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  
  if (ext0_result == ESP_OK) {
    Serial.printf("Deep sleep wake-up configured: GPIO%d (custom) + GPIO0 (boot)\n", POWER_BUTTON_PIN);
  } else {
    Serial.printf("Deep sleep wake-up configured: GPIO0 (boot) only - GPIO%d failed\n", POWER_BUTTON_PIN);
  }
}

void initStatusLED() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW); // LED on (inverted logic)
  ledState = true;
  Serial.println("Status LED initialized");
}

void handlePowerButton() {
  bool currentButtonRead = !digitalRead(POWER_BUTTON_PIN); // Inverted because of pull-up
  unsigned long now = millis();
  
  switch (buttonState) {
    case BUTTON_IDLE:
      if (currentButtonRead) {
        buttonState = BUTTON_PRESSED;
        buttonPressStart = now;
        lastActivityTime = now;
        Serial.println("Power button pressed");
      }
      break;
      
    case BUTTON_PRESSED:
      if (!currentButtonRead) {
        // Short press - toggle LED brightness or take photo
        if (now - buttonPressStart < POWER_OFF_PRESS_MS) {
          if (isCapturingPhotos) {
            // Quick photo capture
            Serial.println("Button: Quick photo capture");
            if (take_photo()) {
              ledStatus = LED_PHOTO_CAPTURE;
              photoDataUploading = true;
              sent_photo_bytes = 0;
              sent_photo_frames = 0;
            }
          }
        }
        buttonState = BUTTON_IDLE;
      } else if (now - buttonPressStart > POWER_OFF_PRESS_MS) {
        // Long press detected - initiate power off
        buttonState = BUTTON_LONG_PRESS;
        Serial.println("Long press detected - powering off");
        startPowerOffSequence();
      }
      break;
      
    case BUTTON_LONG_PRESS:
      if (!currentButtonRead) {
        buttonState = BUTTON_IDLE;
      }
      break;
      
    default:
      buttonState = BUTTON_IDLE;
      break;
  }
}

void updateStatusLED() {
  unsigned long now = millis();
  bool shouldToggle = false;
  unsigned long blinkInterval = 1000;
  
  switch (ledStatus) {
    case LED_BOOT_SEQUENCE:
      blinkInterval = LED_BOOT_BLINK_FAST;
      shouldToggle = true;
      // Switch to normal operation after boot delay
      if (now > BOOT_COMPLETE_DELAY_MS) {
        ledStatus = LED_NORMAL_OPERATION;
        deviceState = DEVICE_ACTIVE;
        Serial.println("Boot sequence complete");
      }
      break;
      
    case LED_NORMAL_OPERATION:
      // Solid on during normal operation
      if (!ledState) {
        digitalWrite(STATUS_LED_PIN, LOW); // LED on
        ledState = true;
      }
      break;
      
    case LED_LOW_BATTERY:
      blinkInterval = LED_BATTERY_LOW_BLINK;
      shouldToggle = true;
      break;
      
    case LED_PHOTO_CAPTURE:
      // Quick flash during photo capture
      if (now - ledLastToggle > LED_PHOTO_CAPTURE_FLASH) {
        ledStatus = LED_NORMAL_OPERATION;
      } else {
        digitalWrite(STATUS_LED_PIN, LOW); // LED on during capture
        ledState = true;
      }
      break;
      
    case LED_POWER_OFF_SEQUENCE:
      // Fast blink during power off
      blinkInterval = LED_BOOT_BLINK_FAST;
      shouldToggle = true;
      break;
      
    case LED_SLEEP_MODE:
      // Very slow blink in sleep mode
      blinkInterval = LED_SLEEP_BLINK;
      shouldToggle = true;
      break;
      
    case LED_OFF:
      if (ledState) {
        digitalWrite(STATUS_LED_PIN, HIGH); // LED off
        ledState = false;
      }
      break;
      
    default:
      ledStatus = LED_NORMAL_OPERATION;
      break;
  }
  
  // Handle LED blinking
  if (shouldToggle && (now - ledLastToggle >= blinkInterval)) {
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState ? LOW : HIGH); // Inverted logic
    ledLastToggle = now;
  }
}

void startPowerOffSequence() {
  Serial.println("Starting power off sequence...");
  deviceState = DEVICE_POWERING_OFF;
  ledStatus = LED_POWER_OFF_SEQUENCE;
  
  // Stop photo capture
  isCapturingPhotos = false;
  
  // Release camera buffer if needed
  if (fb) {
    esp_camera_fb_return(fb);
    fb = nullptr;
  }
  
  // Note: Keep BLE advertising active even during power off for discoverability
  
  // Power off camera
  powerDownCamera();
  
  // Wait for LED sequence to complete
  unsigned long powerOffStart = millis();
  while (millis() - powerOffStart < POWER_OFF_SLEEP_DELAY_MS) {
    updateStatusLED();
    delay(50);
  }
  
  enterDeepSleep();
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep mode...");
  
  // Set LED to sleep mode briefly
  ledStatus = LED_SLEEP_MODE;
  updateStatusLED();
  delay(200);
  
  // Turn off LED completely
  ledStatus = LED_OFF;
  updateStatusLED();
  delay(100);
  
  // Disable watchdog timers to prevent reset loop (if available)
  #ifdef CONFIG_ESP_TASK_WDT_EN
    esp_task_wdt_deinit();
  #endif
  
  // Set CPU to minimum frequency
  setCpuFrequencyMhz(MIN_CPU_FREQ_MHZ);
  
  // Ensure button pin is configured for wake-up
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  
  // FAILSAFE: Add timer wake-up as backup (wake up after 1 hour)
  esp_sleep_enable_timer_wakeup(60 * 60 * 1000000ULL); // 1 hour in microseconds
  
  // Configure multiple wake-up sources for reliability
  
  // Primary wake-up: Custom button (ext0)
  esp_err_t ext0_result = esp_sleep_enable_ext0_wakeup((gpio_num_t)POWER_BUTTON_PIN, 0);
  if (ext0_result == ESP_OK) {
    Serial.printf("✓ Primary wake-up: GPIO%d (ext0)\n", POWER_BUTTON_PIN);
  } else {
    Serial.printf("✗ GPIO%d ext0 wake-up failed (error: %d)\n", POWER_BUTTON_PIN, ext0_result);
  }
  
  // Backup wake-up: Boot button (ext1) - always works
  uint64_t ext1_mask = (1ULL << 0); // GPIO0 (boot button)
  esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.println("✓ Backup wake-up: GPIO0 (boot button, ext1)");
  
  // Disable other wake-up sources
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
  
  Serial.printf("Deep sleep configured:\n");
  Serial.printf("- Primary wake-up: GPIO%d (your custom button)\n", POWER_BUTTON_PIN);
  Serial.printf("- Backup wake-up: GPIO0 (boot button on board)\n");
  Serial.printf("- Timer failsafe: 1 hour automatic wake-up\n");
  Serial.println("Press and hold EITHER button to wake up the device");
  Serial.println("Going to sleep now...");
  Serial.flush();
  
  // Longer delay to ensure serial output completes
  delay(500);
  
  // Enter deep sleep
  esp_deep_sleep_start();
}



void handleWakeUp() {
  Serial.println("Device waking up from deep sleep");
  deviceState = DEVICE_BOOTING;
  ledStatus = LED_BOOT_SEQUENCE;
  devicePoweredOn = true;
  lastActivityTime = millis();
  
  // Show wake-up LED pattern
  for (int i = 0; i < 5; i++) {
    digitalWrite(STATUS_LED_PIN, LOW);  // LED on
    delay(100);
    digitalWrite(STATUS_LED_PIN, HIGH); // LED off
    delay(100);
  }
  digitalWrite(STATUS_LED_PIN, LOW); // LED on for normal operation
  
  Serial.println("Wake-up sequence complete - device ready");
  
  // Reinitialize systems
  readBatteryLevel();
  updatePowerState();
}

// -------------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------------

// A small buffer for sending photo chunks over BLE
static uint8_t *s_compressed_frame_2 = nullptr;

void setup() {
  Serial.begin(921600);
  Serial.println("OMI Glass - Power Optimized Firmware v2.1.0 with Power Button Control");

  // Initialize power button and status LED first
  initPowerButton();
  initStatusLED();
  
  Serial.println("Power button and LED initialized");

  // Initialize power management
  esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = MAX_CPU_FREQ_MHZ,
    .min_freq_mhz = MIN_CPU_FREQ_MHZ,
    .light_sleep_enable = true
  };
  esp_pm_configure(&pm_config);
  Serial.println("Power management configured");

  // Read initial battery level
  readBatteryLevel();
  updatePowerState();

  configure_ble();
  configure_camera();

  // Allocate buffer for photo chunks
  s_compressed_frame_2 = (uint8_t *)ps_calloc(202, sizeof(uint8_t));
  if (!s_compressed_frame_2) {
    Serial.println("Failed to allocate chunk buffer!");
  } else {
    Serial.println("Chunk buffer allocated successfully.");
  }

  // Start with fixed capture interval
  isCapturingPhotos = true;
  captureInterval = getFixedPhotoInterval();
  lastCaptureTime = millis() - captureInterval;
  lastActivityTime = millis();
  
  Serial.printf("Power-optimized firmware ready. Target: >14 hours battery life\n");
  Serial.printf("Fixed capture interval: %d seconds\n", captureInterval / 1000);
  Serial.println("Power Controls (Custom Button A0):");
  Serial.println("- Short press: Quick photo capture");
  Serial.println("- Long press (2s): Power off");
  Serial.println("- Press button during sleep: Wake up");
  Serial.println("");
  Serial.println("Serial Commands:");
  Serial.println("- 'status' - Show device status");
  Serial.println("- 'charging' - Check charging status (10 readings)");
  Serial.println("- 'monitor' - Continuous battery monitor (5s intervals)");
}

void loop() {
  unsigned long now = millis();

  // Handle basic serial commands
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    command.toLowerCase();
    
    if (command == "status") {
      Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage, batteryPercentage);
      Serial.printf("Connected: %s\n", connected ? "YES" : "NO");
      Serial.printf("Advertising: %s\n", advertisingActive ? "YES" : "NO");
      Serial.printf("Photos captured: %s\n", isCapturingPhotos ? "YES" : "NO");
      Serial.printf("Device state: %d\n", deviceState);
      Serial.printf("BLE Connection: Always discoverable, stable parameters\n");
    } else if (command == "charging") {
      Serial.println("*** CHARGING STATUS MONITOR ***");
      for (int i = 0; i < 10; i++) {
        readBatteryLevel();
        Serial.printf("Reading %d: %.2fV (%d%%) ", i+1, batteryVoltage, batteryPercentage);
        
        // Charging detection logic
        if (batteryVoltage > 4.1) {
          Serial.println("🔋 CHARGING (High voltage detected)");
        } else if (batteryVoltage > 3.9) {
          Serial.println("⚡ CHARGED (Good level)");
        } else if (batteryVoltage > 3.7) {
          Serial.println("🔴 LOW (Needs charging)");
        } else {
          Serial.println("❌ CRITICAL (Check connections)");
        }
        delay(2000); // 2 second intervals
      }
      Serial.println("Charging monitor complete");
    } else if (command == "monitor") {
      Serial.println("*** CONTINUOUS BATTERY MONITOR ***");
      Serial.println("Monitoring battery every 5 seconds. Send any command to stop.");
      
      while (!Serial.available()) {
        readBatteryLevel();
        Serial.printf("%.2fV (%d%%) - ", batteryVoltage, batteryPercentage);
        
        if (batteryVoltage > 4.1) {
          Serial.println("🔋 CHARGING");
        } else if (batteryVoltage > 3.9) {
          Serial.println("⚡ CHARGED"); 
        } else if (batteryVoltage > 3.7) {
          Serial.println("🔴 LOW");
        } else {
          Serial.println("❌ CRITICAL");
        }
        delay(5000);
      }
      Serial.readString(); // Clear the input
      Serial.println("Monitor stopped");
    }
  }

  // Handle power button and LED status (highest priority)
  handlePowerButton();
  updateStatusLED();
  
  // Skip other operations if powering off or in sleep mode
  if (deviceState == DEVICE_POWERING_OFF || deviceState == DEVICE_SLEEP) {
    delay(10);
    return;
  }

  // Update battery level periodically
  if (now - lastBatteryCheck >= BATTERY_TASK_INTERVAL_MS) {
    readBatteryLevel();
    updatePowerState();
    lastBatteryCheck = now;
    
    // Using fixed capture interval - no updates needed
  }

  // Handle camera power management
  if (cameraActive && (now - lastCameraUse > CAMERA_POWER_DOWN_DELAY_MS) && !photoDataUploading) {
    powerDownCamera();
  }

  // Keep advertising active for continuous discoverability
  if (!advertisingActive && (now - lastAdvertiseTime > BLE_SLEEP_ADV_INTERVAL)) {
    Serial.println("Restarting BLE advertising for discoverability");
    BLEDevice::startAdvertising();
    advertisingActive = true;
    lastAdvertiseTime = now;
  }
  
  // Keep device always discoverable - never stop advertising
  if (!advertisingActive) {
    BLEDevice::startAdvertising();
    advertisingActive = true;
    lastAdvertiseTime = now;
  }

  // Photo capture logic - only when not in sleep state
  if (currentPowerState != POWER_STATE_SLEEP && isCapturingPhotos && !photoDataUploading && connected) {
    if ((captureInterval == 0) || (now - lastCaptureTime >= (unsigned long)captureInterval)) {
      if (captureInterval == 0) {
        isCapturingPhotos = false; // Single shot
      }
      Serial.println("Capturing photo...");
      if (take_photo()) {
        Serial.println("Photo capture successful. Starting upload...");
        ledStatus = LED_PHOTO_CAPTURE; // Flash LED during capture
        photoDataUploading = true;
        sent_photo_bytes = 0;
        sent_photo_frames = 0;
        lastCaptureTime = now;
        lastActivityTime = now; // Reset activity timer
      }
    }
  }

  // BLE photo upload with power-optimized delays
  if (photoDataUploading && fb && connected) {
    size_t remaining = fb->len - sent_photo_bytes;
    if (remaining > 0) {
      // Prepare chunk
      s_compressed_frame_2[0] = (uint8_t)(sent_photo_frames & 0xFF);
      s_compressed_frame_2[1] = (uint8_t)((sent_photo_frames >> 8) & 0xFF);
      size_t bytes_to_copy = (remaining > 200) ? 200 : remaining;
      memcpy(&s_compressed_frame_2[2], &fb->buf[sent_photo_bytes], bytes_to_copy);

      photoDataCharacteristic->setValue(s_compressed_frame_2, bytes_to_copy + 2);
      photoDataCharacteristic->notify();

      sent_photo_bytes += bytes_to_copy;
      sent_photo_frames++;
    }
    else {
      // End of photo marker
      s_compressed_frame_2[0] = 0xFF;
      s_compressed_frame_2[1] = 0xFF;
      photoDataCharacteristic->setValue(s_compressed_frame_2, 2);
      photoDataCharacteristic->notify();
      Serial.println("Photo upload complete.");

      photoDataUploading = false;
      esp_camera_fb_return(fb);
      fb = nullptr;
      
      // Return LED to normal operation after photo upload
      if (ledStatus == LED_PHOTO_CAPTURE) {
        ledStatus = LED_NORMAL_OPERATION;
      }
    }
  }

  // Power-optimized delay with light sleep capability
  if (currentPowerState == POWER_STATE_SLEEP) {
    delay(100); // Longer delay in sleep state
  } else if (photoDataUploading) {
    delay(BLE_PHOTO_TRANSFER_DELAY); // Controlled delay during upload
  } else {
    delay(50); // Standard delay with light sleep
  }
}
