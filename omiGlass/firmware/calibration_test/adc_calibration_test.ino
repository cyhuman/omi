/*
 * ADC Calibration Test for OpenGlass Battery Monitoring
 * 
 * This sketch helps you find the optimal ADC settings for your hardware.
 * Upload this, then compare readings with your multimeter.
 */

const int BATTERY_ADC_PIN = A0;

void setup() {
  Serial.begin(921600);
  Serial.println("=== ADC Calibration Test ===");
  
  // Test different ADC configurations
  testADCConfiguration();
}

void loop() {
  // Test current configuration every 5 seconds
  testCurrentReading();
  delay(5000);
}

void testADCConfiguration() {
  Serial.println("\nTesting different ADC configurations:");
  Serial.println("Compare these readings with your multimeter at the same time\n");
  
  // Test ADC_11db (0-3.6V range) - Current setting
  Serial.println("--- ADC_11db (0-3.6V range) ---");
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  // analogSetWidth(12);  // 12-bit is default on ESP32S3
  delay(100);
  testReading("ADC_11db", 3.6);
  
  // Test ADC_6db (0-2.2V range)
  Serial.println("\n--- ADC_6db (0-2.2V range) ---");
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_6db);
  delay(100);
  testReading("ADC_6db", 2.2);
  
  // Test ADC_2_5db (0-1.5V range)
  Serial.println("\n--- ADC_2_5db (0-1.5V range) ---");
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_2_5db);
  delay(100);
  testReading("ADC_2_5db", 1.5);
  
  // Set back to optimal setting
  Serial.println("\n--- Returning to ADC_11db ---");
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  delay(100);
}

void testReading(String config, float maxVoltage) {
  // Take multiple readings
  long sum = 0;
  const int samples = 20;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(100);
  }
  
  int avgRaw = sum / samples;
  float voltage = (avgRaw / 4095.0) * maxVoltage;
  
  Serial.printf("%s: Raw=%d, Voltage=%.3fV (max %.1fV)\n", 
                config.c_str(), avgRaw, voltage, maxVoltage);
}

void testCurrentReading() {
  Serial.println("\n=== CURRENT READING ===");
  
  // Multiple samples for accuracy
  long sum = 0;
  const int samples = 10;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(100);
  }
  
  int avgRaw = sum / samples;
  
  // Test different reference voltages
  Serial.printf("Raw ADC: %d (avg of %d samples)\n", avgRaw, samples);
  Serial.println("\nWith different reference voltages:");
  
  float refVoltages[] = {3.0, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6};
  int numRefs = sizeof(refVoltages) / sizeof(refVoltages[0]);
  
  for (int i = 0; i < numRefs; i++) {
    float adcVoltage = (avgRaw / 4095.0) * refVoltages[i];
    float batteryVoltage = adcVoltage * 3.865; // Current multiplier
    
    Serial.printf("  Ref %.1fV: ADC=%.3fV, Battery=%.3fV\n", 
                  refVoltages[i], adcVoltage, batteryVoltage);
  }
  
  Serial.println("\nCompare the Battery voltage with your multimeter!");
  Serial.println("Update 'adcRefVoltage' to the value that matches closest.");
  Serial.println("==========================================");
} 