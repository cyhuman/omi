#!/usr/bin/env python3
"""
UF2 Generator Script for ESP32-S3 OMI Glass Firmware
Converts ESP32-S3 binary files to UF2 format for easy flashing

This script is automatically called by PlatformIO after building.
"""

Import("env")
import os
import sys
import struct
import binascii

def create_uf2_file(binary_path, uf2_path, family_id=0x1c5f21b0):
    """
    Convert binary file to UF2 format for ESP32-S3
    
    Args:
        binary_path: Path to input binary file
        uf2_path: Path to output UF2 file
        family_id: ESP32-S3 family ID (0x1c5f21b0)
    """
    
    # UF2 constants
    UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
    UF2_MAGIC_START1 = 0x9E5D5157  # Random magic
    UF2_MAGIC_END = 0x0AB16F30     # Final magic
    UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
    
    # ESP32-S3 specific settings
    ESP32S3_FLASH_BASE = 0x10000  # App partition offset
    UF2_BLOCK_SIZE = 256
    UF2_DATA_SIZE = 256
    
    if not os.path.exists(binary_path):
        print(f"Error: Binary file not found: {binary_path}")
        return False
    
    # Read binary file
    with open(binary_path, 'rb') as f:
        binary_data = f.read()
    
    binary_size = len(binary_data)
    print(f"Converting {binary_size} bytes from {binary_path}")
    
    # Calculate number of blocks needed
    num_blocks = (binary_size + UF2_DATA_SIZE - 1) // UF2_DATA_SIZE
    
    # Create UF2 file
    with open(uf2_path, 'wb') as uf2_file:
        for block_no in range(num_blocks):
            # Calculate data for this block
            data_start = block_no * UF2_DATA_SIZE
            data_end = min(data_start + UF2_DATA_SIZE, binary_size)
            block_data = binary_data[data_start:data_end]
            
            # Pad block to 256 bytes
            if len(block_data) < UF2_DATA_SIZE:
                block_data += b'\x00' * (UF2_DATA_SIZE - len(block_data))
            
            # Calculate target address
            target_addr = ESP32S3_FLASH_BASE + data_start
            
            # Create UF2 block (512 bytes total)
            uf2_block = struct.pack('<I', UF2_MAGIC_START0)      # Magic start 0
            uf2_block += struct.pack('<I', UF2_MAGIC_START1)     # Magic start 1
            uf2_block += struct.pack('<I', UF2_FLAG_FAMILY_ID_PRESENT)  # Flags
            uf2_block += struct.pack('<I', target_addr)          # Target address
            uf2_block += struct.pack('<I', UF2_DATA_SIZE)        # Payload size
            uf2_block += struct.pack('<I', block_no)             # Block number
            uf2_block += struct.pack('<I', num_blocks)           # Total blocks
            uf2_block += struct.pack('<I', family_id)            # Family ID
            uf2_block += block_data                              # Data (256 bytes)
            uf2_block += struct.pack('<I', UF2_MAGIC_END)        # Magic end
            
            # Pad to 512 bytes
            padding_size = 512 - len(uf2_block)
            uf2_block += b'\x00' * padding_size
            
            uf2_file.write(uf2_block)
    
    print(f"✅ UF2 file created: {uf2_path}")
    print(f"   Blocks: {num_blocks}")
    print(f"   Size: {os.path.getsize(uf2_path)} bytes")
    return True

def generate_uf2_post_build(source, target, env):
    """
    PlatformIO post-build action to generate UF2 file
    """
    print("\n🔄 Generating UF2 file...")
    
    # Get build environment info
    build_dir = env.subst("$BUILD_DIR")
    project_name = env.subst("$PROJECT_NAME")
    firmware_name = env.subst("$PROGNAME")
    
    # Input binary file
    binary_path = os.path.join(build_dir, f"{firmware_name}.bin")
    
    # Output UF2 file
    uf2_path = os.path.join(build_dir, f"{firmware_name}.uf2")
    
    # Also create a copy in the project root for easy access
    project_uf2_path = os.path.join(env.subst("$PROJECT_DIR"), f"omi_glass_firmware.uf2")
    
    if create_uf2_file(binary_path, uf2_path):
        # Copy to project root
        try:
            import shutil
            shutil.copy2(uf2_path, project_uf2_path)
            print(f"✅ UF2 file copied to: {project_uf2_path}")
        except Exception as e:
            print(f"⚠️  Could not copy UF2 to project root: {e}")
        
        # Create flash instructions
        instructions_path = os.path.join(env.subst("$PROJECT_DIR"), "FLASHING_INSTRUCTIONS.md")
        create_flash_instructions(instructions_path, project_uf2_path)
        
        print("\n🎉 UF2 generation complete!")
        print("📋 Flashing options:")
        print(f"   1. Copy {os.path.basename(project_uf2_path)} to ESP32S3 drive when in bootloader mode")
        print(f"   2. Use esptool: esptool.py write_flash 0x10000 {binary_path}")
        print(f"   3. Use PlatformIO: pio run -t upload")
        
    else:
        print("❌ UF2 generation failed!")

def create_flash_instructions(instructions_path, uf2_path):
    """Create detailed flashing instructions"""
    instructions = f"""# OMI Glass Firmware Flashing Instructions

## Option 1: UF2 File (Easiest Method)

1. **Enter Bootloader Mode:**
   - Press and hold the BOOT button on your ESP32-S3
   - While holding BOOT, press and release the RESET button
   - Release the BOOT button
   - The device should appear as a USB drive named "ESP32S3"

2. **Flash the Firmware:**
   - Copy the file `{os.path.basename(uf2_path)}` to the ESP32S3 drive
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
"""
    
    try:
        with open(instructions_path, 'w') as f:
            f.write(instructions)
        print(f"📝 Instructions created: {instructions_path}")
    except Exception as e:
        print(f"⚠️  Could not create instructions: {e}")

# Register the post-build action
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_uf2_post_build)

print("🔧 UF2 generator script loaded") 