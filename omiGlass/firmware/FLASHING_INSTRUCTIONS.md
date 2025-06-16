# OMI Glass Firmware Flashing Instructions

## Option 1: UF2 File (Easiest Method)

1. **Enter Bootloader Mode:**
   - Press and hold the BOOT button on your ESP32-S3
   - While holding BOOT, press and release the RESET button
   - Release the BOOT button
   - The device should appear as a USB drive named "ESP32S3"

2. **Flash the Firmware:**
   - Copy the file `omi_glass_firmware.uf2` to the ESP32S3 drive
   - The device will automatically flash and reboot

3. **Verify:**
   - The device should start running the new firmware
   - Check serial monitor at 115200 baud for status messages

## Option 2: esptool.py (Advanced)

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x10000 firmware.bin
```

## Option 3: PlatformIO

```bash
pio run -t upload
```

## Troubleshooting

- **Device not appearing as USB drive:** Make sure you're in bootloader mode
- **Flash fails:** Try a different USB cable or port
- **Device not recognized:** Install ESP32-S3 USB drivers
- **Still having issues:** Use the slower PlatformIO environment: `pio run -e seeed_xiao_esp32s3_slow -t upload`

## Monitoring

After flashing, you can monitor the device:
```bash
pio device monitor --baud 115200
```

The firmware will show:
- Boot sequence with LED blinks
- BLE initialization
- Camera initialization
- Battery status
- Photo capture every 30 seconds
