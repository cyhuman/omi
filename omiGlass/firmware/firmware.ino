#define CAMERA_MODEL_XIAO_ESP32S3
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_camera.h"
#include "camera_pins.h"

// ---------------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------------

// Device Information Service
#define DEVICE_INFORMATION_SERVICE_UUID (uint16_t)0x180A
#define MANUFACTURER_NAME_STRING_CHAR_UUID (uint16_t)0x2A29
#define MODEL_NUMBER_STRING_CHAR_UUID (uint16_t)0x2A24
#define FIRMWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A26
#define HARDWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A27

// Battery Service
#define BATTERY_SERVICE_UUID (uint16_t)0x180F
#define BATTERY_LEVEL_CHAR_UUID (uint16_t)0x2A19

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
int captureInterval = 0;         // Interval in ms
unsigned long lastCaptureTime = 0;
unsigned long lastBatteryUpdate = 0;
const unsigned long BATTERY_UPDATE_INTERVAL = 60000; // Update battery every 60 seconds

size_t sent_photo_bytes = 0;
size_t sent_photo_frames = 0;
bool photoDataUploading = false;

// Battery reading constants for ESP32S3
const int BATTERY_ADC_PIN = A0; // Adjust this pin according to your hardware
const float BATTERY_MAX_VOLTAGE = 4.2; // Maximum voltage for LiPo battery
const float BATTERY_MIN_VOLTAGE = 3.7; // Minimum safe voltage for LiPo battery
// Voltage divider: R1=169kΩ, R2=110kΩ, Ratio=2.536

// Camera error tracking (global scope for access from multiple functions)
int camera_failure_count = 0;
unsigned long last_camera_error = 0;
const int MAX_CAMERA_FAILURES = 5;
const unsigned long CAMERA_ERROR_COOLDOWN = 300000; // 5 minutes

// ADC Calibration Constants - ADJUST THESE FOR YOUR HARDWARE
const float ADC_REFERENCE_VOLTAGE = 3.3;   // Measure your 3.3V rail with multimeter
const float VOLTAGE_MULTIPLIER = 3.865;    // Calculated from multimeter: 3.923V / 1.015V = 3.865

// -------------------------------------------------------------------------
// Camera Frame
// -------------------------------------------------------------------------
camera_fb_t *fb = nullptr;

// Forward declaration
void handlePhotoControl(int8_t controlValue);
uint8_t readBatteryLevel();
void updateBatteryLevel();

class ServerHandler : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    connected = true;
    Serial.println(">>> BLE Client connected.");
    // Send initial battery level when client connects
    updateBatteryLevel();
  }
  void onDisconnect(BLEServer *server) override {
    connected = false;
    Serial.println("<<< BLE Client disconnected. Restarting advertising.");
    BLEDevice::startAdvertising();
  }
};

class PhotoControlCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    if (characteristic->getLength() == 1) {
      int8_t received = characteristic->getData()[0];
      Serial.print("PhotoControl received: ");
      Serial.println(received);
      handlePhotoControl(received);
    }
  }
};

// -------------------------------------------------------------------------
// Battery Functions
// -------------------------------------------------------------------------
uint8_t readBatteryLevel() {
  // Take multiple ADC readings and average them for stability
  const int NUM_SAMPLES = 10;
  long adcSum = 0;
  
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(100); // Small delay between readings
  }
  
  int adcValue = adcSum / NUM_SAMPLES;
  
  // Convert ADC value to voltage using calibrated reference
  float voltage = (adcValue / 4095.0) * ADC_REFERENCE_VOLTAGE;
  
  // Debug: Print raw ADC values to serial monitor
  Serial.print("DEBUG - ADC Raw: ");
  Serial.print(adcValue);
  Serial.print(" (avg of ");
  Serial.print(NUM_SAMPLES);
  Serial.print(" samples), ADC Voltage: ");
  Serial.print(voltage, 3);
  Serial.print("V");
  
  // Apply calibrated voltage divider correction
  voltage *= VOLTAGE_MULTIPLIER;
  
  Serial.print(", Battery Voltage: ");
  Serial.print(voltage, 3);
  Serial.print("V (multimeter reference)");
  
  // Convert voltage to percentage
  uint8_t percentage;
  if (voltage >= BATTERY_MAX_VOLTAGE) {
    percentage = 100;
  } else if (voltage <= BATTERY_MIN_VOLTAGE) {
    percentage = 0;
  } else {
    percentage = (uint8_t)(((voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100);
  }
  
  Serial.print(", Percentage: ");
  Serial.print(percentage);
  Serial.println("%");
  
  return percentage;
}

void updateBatteryLevel() {
  uint8_t batteryLevel = readBatteryLevel();
  
  Serial.print("Battery level: ");
  Serial.print(batteryLevel);
  Serial.println("%");
  
  // Quick health check
  if (batteryLevel == 0) {
    Serial.println("⚠️  BATTERY VERY LOW or measurement error!");
    Serial.println("   Check: 1) Battery charge, 2) Voltage divider connections");
  }
  
  // Update the characteristic value
  batteryLevelCharacteristic->setValue(&batteryLevel, 1);
  
  // Notify connected clients if any
  if (connected) {
    batteryLevelCharacteristic->notify();
  }
}

// Enhanced logging function for battery monitoring
void logBatteryData() {
  // Use same averaging method as readBatteryLevel for consistency
  const int NUM_SAMPLES = 10;
  long adcSum = 0;
  
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(100);
  }
  
  int adcValue = adcSum / NUM_SAMPLES;
  float adcVoltage = (adcValue / 4095.0) * ADC_REFERENCE_VOLTAGE;
  float batteryVoltage = adcVoltage * VOLTAGE_MULTIPLIER;
  
  // Calculate percentage using the SAME reading (don't call readBatteryLevel again)
  uint8_t percentage;
  if (batteryVoltage >= BATTERY_MAX_VOLTAGE) {
    percentage = 100;
  } else if (batteryVoltage <= BATTERY_MIN_VOLTAGE) {
    percentage = 0;
  } else {
    percentage = (uint8_t)(((batteryVoltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100);
  }
  
  // CSV format for easy plotting: timestamp,adc_raw,adc_voltage,battery_voltage,percentage
  Serial.print("DATA,");
  Serial.print(millis());
  Serial.print(",");
  Serial.print(adcValue);
  Serial.print(",");
  Serial.print(adcVoltage, 3);
  Serial.print(",");
  Serial.print(batteryVoltage, 3);
  Serial.print(",");
  Serial.println(percentage);
}

// System diagnostics for debugging camera issues
void logSystemDiagnostics() {
  static unsigned long lastDiagnostic = 0;
  static int diagnosticCount = 0;
  
  // Log diagnostics every 2 minutes
  if (millis() - lastDiagnostic >= 120000) {
    diagnosticCount++;
    lastDiagnostic = millis();
    
    Serial.println("=== SYSTEM DIAGNOSTICS ===");
    Serial.printf("📊 Uptime: %lu seconds\n", millis() / 1000);
    Serial.printf("📷 Camera failures: %d\n", camera_failure_count);
    
    // Get fresh battery reading for diagnostics
    uint8_t batteryLevel = readBatteryLevel();
    Serial.printf("🔋 Battery: %d%%\n", batteryLevel);
    
    // ADC diagnostics
    int rawADC = analogRead(BATTERY_ADC_PIN);
    Serial.printf("🔬 ADC Raw (single reading): %d\n", rawADC);
    Serial.printf("🔬 ADC Pin: A%d\n", BATTERY_ADC_PIN);
    
    // Memory information
    Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("💾 Min free heap: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("💾 Heap size: %d bytes\n", ESP.getHeapSize());
    
    // PSRAM information
    if (psramFound()) {
      Serial.printf("💾 Free PSRAM: %d bytes\n", ESP.getFreePsram());
      Serial.printf("💾 PSRAM size: %d bytes\n", ESP.getPsramSize());
    } else {
      Serial.println("⚠️  PSRAM not available");
    }
    
    // Connection status
    Serial.printf("📡 BLE connected: %s\n", connected ? "Yes" : "No");
    Serial.printf("📸 Photo capturing: %s\n", isCapturingPhotos ? "Yes" : "No");
    Serial.printf("📸 Capture interval: %d seconds\n", captureInterval / 1000);
    
    Serial.println("========================\n");
  }
}

// -------------------------------------------------------------------------
// configure_ble()
// -------------------------------------------------------------------------
void configure_ble() {
  Serial.println("Initializing BLE...");
  BLEDevice::init("OpenGlass");
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
      BATTERY_LEVEL_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  
  // Add descriptor for battery level notifications
  BLE2902 *batteryDescriptor = new BLE2902();
  batteryDescriptor->setNotifications(true);
  batteryLevelCharacteristic->addDescriptor(batteryDescriptor);
  
  // Set initial battery level
  uint8_t initialBatteryLevel = readBatteryLevel();
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

  manufacturerNameCharacteristic->setValue("Based Hardware");
  modelNumberCharacteristic->setValue("OpenGlass");
  firmwareRevisionCharacteristic->setValue("1.0.2");
  hardwareRevisionCharacteristic->setValue("Seeed Xiao ESP32S3 Sense");

  // Start services
  service->start();
  batteryService->start();
  deviceInfoService->start();

  // Start advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(deviceInfoService->getUUID());
  advertising->addServiceUUID(service->getUUID());
  advertising->addServiceUUID(batteryService->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE initialized and advertising started.");
}

// -------------------------------------------------------------------------
// Camera
// -------------------------------------------------------------------------

bool take_photo() {
  // Check if camera is in error state (too many recent failures)
  if (camera_failure_count >= MAX_CAMERA_FAILURES) {
    if (millis() - last_camera_error < CAMERA_ERROR_COOLDOWN) {
      Serial.println("Camera in error state - skipping capture to save battery");
      return false;
    } else {
      // Reset failure count after cooldown
      camera_failure_count = 0;
      Serial.println("Camera error cooldown expired - retrying");
    }
  }

  // Release previous buffer
  if (fb) {
    Serial.println("Releasing previous camera buffer...");
    esp_camera_fb_return(fb);
    fb = nullptr;
  }

  Serial.println("Capturing photo...");
  
  // Try to get frame buffer
  fb = esp_camera_fb_get();
  if (!fb) {
    camera_failure_count++;
    last_camera_error = millis();
    
    Serial.print("Failed to get camera frame buffer! (Failure #");
    Serial.print(camera_failure_count);
    Serial.println(")");
    
    if (camera_failure_count >= MAX_CAMERA_FAILURES) {
      Serial.println("⚠️  Too many camera failures - entering power save mode");
      Serial.println("   Camera will be disabled for 5 minutes to save battery");
      
      // Try to reinitialize camera
      configure_camera();
    }
    
    return false;
  }
  
  // Success - reset failure count
  camera_failure_count = 0;
  
  Serial.print("Photo captured: ");
  Serial.print(fb->len);
  Serial.println(" bytes.");
  return true;
}

void handlePhotoControl(int8_t controlValue) {
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
    Serial.print("Received command: Start interval capture with parameter ");
    Serial.println(controlValue);

    // Adaptive interval based on camera health and battery level
    int baseInterval = 30000; // 30 seconds base
    
    // Increase interval if camera is having issues (save battery)
    if (camera_failure_count > 0) {
      baseInterval = 60000; // 1 minute if camera issues
      Serial.println("📷 Camera issues detected - using longer interval to save battery");
    }
    
    // Increase interval if battery is low
    uint8_t batteryLevel = readBatteryLevel();
    if (batteryLevel < 20) {
      baseInterval = 120000; // 2 minutes if battery low
      Serial.println("🔋 Low battery - using longer interval to conserve power");
    }
    
    captureInterval = baseInterval;
    Serial.printf("📸 Capture interval set to %d seconds\n", captureInterval / 1000);

    isCapturingPhotos = true;
    lastCaptureTime = millis() - captureInterval;
  }
}

// -------------------------------------------------------------------------
// configure_camera()
// -------------------------------------------------------------------------
void configure_camera() {
  Serial.println("Initializing camera...");
  
  // Deinitialize camera first if it was previously initialized
  esp_camera_deinit();
  delay(100); // Small delay to ensure clean state
  
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
  config.xclk_freq_hz = 20000000;

  // Start with smaller frame size and lower quality to reduce memory pressure
  config.frame_size   = FRAMESIZE_VGA; // Reduced from SVGA to save memory
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_count     = 1;
  config.jpeg_quality = 15; // Increased quality number = lower quality, less memory
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  
  // Check PSRAM availability
  bool psramFound = psramInit();
  if (!psramFound) {
    Serial.println("⚠️  PSRAM not found - using DRAM (limited memory)");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_QVGA; // Even smaller for DRAM
    config.jpeg_quality = 20;
  } else {
    Serial.println("✅ PSRAM found - using PSRAM for camera buffer");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x ", err);
    
    // Print more detailed error information
    switch(err) {
      case ESP_ERR_INVALID_ARG:
        Serial.println("(Invalid argument)");
        break;
      case ESP_ERR_NO_MEM:
        Serial.println("(Out of memory)");
        break;
      case ESP_ERR_INVALID_STATE:
        Serial.println("(Invalid state)");
        break;
      default:
        Serial.printf("(Unknown error: %s)\n", esp_err_to_name(err));
    }
  }
  else {
    Serial.println("✅ Camera initialized successfully.");
    
    // Print camera info for debugging
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      Serial.printf("📷 Camera sensor: 0x%02X\n", s->id.PID);
    }
  }
}

// -------------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------------

// A small buffer for sending photo chunks over BLE
static uint8_t *s_compressed_frame_2 = nullptr;

void setup() {
  Serial.begin(921600);
  Serial.println("Setup started...");

  // Initialize ADC for battery monitoring
  analogSetAttenuation(ADC_11db);  // Set ADC input range to 0-3.6V
  analogSetWidth(12);              // Set ADC resolution to 12 bits
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  
  // Take a few initial ADC readings to "warm up" the ADC
  for (int i = 0; i < 5; i++) {
    analogRead(BATTERY_ADC_PIN);
    delay(10);
  }
  
  Serial.println("ADC initialized for battery monitoring");

  configure_ble();
  configure_camera();

  // Allocate buffer for photo chunks (200 bytes + 2 for frame index)
  s_compressed_frame_2 = (uint8_t *)ps_calloc(202, sizeof(uint8_t));
  if (!s_compressed_frame_2) {
    Serial.println("Failed to allocate chunk buffer!");
  } else {
    Serial.println("Chunk buffer allocated successfully.");
  }

  // Force a 30s default capture interval
  isCapturingPhotos = true;
  captureInterval = 30000; // 30 seconds
  lastCaptureTime = millis() - captureInterval;
  Serial.println("Default capture interval set to 30 seconds.");
}

void loop() {
  unsigned long now = millis();

  // Update battery level periodically
  if (now - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
    updateBatteryLevel();
    lastBatteryUpdate = now;
  }
  
  // Log battery data every 5 seconds for real-time monitoring
  static unsigned long lastDataLog = 0;
  if (now - lastDataLog >= 5000) { // 5 second intervals
    logBatteryData();
    lastDataLog = now;
  }
  
  // Log system diagnostics periodically
  logSystemDiagnostics();

  // Check if it's time to capture a photo
  if (isCapturingPhotos && !photoDataUploading && connected) {
    if ((captureInterval == 0) || (now - lastCaptureTime >= (unsigned long)captureInterval)) {
      if (captureInterval == 0) {
        // Single shot if interval=0
        isCapturingPhotos = false;
      }
      Serial.println("Interval reached. Capturing photo...");
      if (take_photo()) {
        Serial.println("Photo capture successful. Starting upload...");
        photoDataUploading = true;
        sent_photo_bytes = 0;
        sent_photo_frames = 0;
        lastCaptureTime = now;
      }
    }
  }

  // If uploading, send chunks over BLE
  if (photoDataUploading && fb) {
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

      Serial.print("Uploading chunk ");
      Serial.print(sent_photo_frames);
      Serial.print(" (");
      Serial.print(bytes_to_copy);
      Serial.print(" bytes), ");
      Serial.print(remaining - bytes_to_copy);
      Serial.println(" bytes remaining.");
    }
    else {
      // End of photo marker
      s_compressed_frame_2[0] = 0xFF;
      s_compressed_frame_2[1] = 0xFF;
      photoDataCharacteristic->setValue(s_compressed_frame_2, 2);
      photoDataCharacteristic->notify();
      Serial.println("Photo upload complete.");

      photoDataUploading = false;
      // Free camera buffer
      esp_camera_fb_return(fb);
      fb = nullptr;
      Serial.println("Camera frame buffer freed.");
    }
  }

  delay(20);
}
