# Real-time Battery Monitoring Setup

Monitor your OpenGlass battery voltage, charging status, and performance in real-time with graphs and data logging.

## 🛠 Setup Requirements

### Option 1: Python Real-time Plotting (Recommended)

**Install Python dependencies:**
```bash
pip install pyserial matplotlib
```

**Quick Start (Recommended):**
```bash
# Navigate to firmware directory
cd omiGlass/firmware

# Run the easy launcher script (will show available ports)
./run_battery_monitor.sh

# Run with your specific port
./run_battery_monitor.sh /dev/cu.usbmodem1101
```

### Option 3: ADC Calibration Test (For fine-tuning)

If you need to calibrate your specific hardware:

1. **Navigate to calibration test**:
   ```bash
   cd omiGlass/firmware/calibration_test/
   ```

2. **Open `adc_calibration_test.ino` in Arduino IDE**

3. **Upload to your device** and compare readings with multimeter

4. **Update calibration constants** in main firmware based on results

**Manual Usage:**
```bash
# Find your serial port first
ls /dev/cu.*              # macOS
ls /dev/ttyUSB* /dev/ttyACM*  # Linux

# Activate virtual environment and run
source battery_monitor_env/bin/activate
python3 battery_monitor.py /dev/cu.usbmodem1101  # macOS
python3 battery_monitor.py COM3                  # Windows
```

### Option 2: Arduino IDE Serial Plotter (Simple)

1. **Flash the enhanced firmware** to your OpenGlass device
2. **Open Arduino IDE** → Tools → Serial Plotter
3. **Set baud rate** to `921600`
4. **View real-time plots** of battery data

## 📊 What You'll See

### Python Plotter Features:
- **4 synchronized graphs**:
  - ADC Raw Value (0-4095)
  - ADC Voltage (0-3.3V)
  - Battery Voltage (3.7-4.2V range with reference lines)
  - Battery Percentage (0-100%)
- **Real-time updates** every 5 seconds
- **40+ minutes of history** (500 data points)
- **Auto-scaling axes**
- **CSV data logging** in terminal

### Arduino Serial Plotter:
- **Simple line graphs** of all values
- **Real-time updates**
- **Built into Arduino IDE**

## 🔋 Monitoring Scenarios

### Charging Behavior Analysis:
```bash
# Start monitoring before plugging in charger
python3 battery_monitor.py /dev/cu.usbserial-0001

# Then plug in USB charger and watch:
# - Voltage should rise gradually
# - Percentage should increase
# - Look for charging anomalies
```

### Discharge Curve Analysis:
```bash
# Monitor during normal operation
# Watch for:
# - Smooth voltage decline
# - Consistent ADC readings
# - Proper percentage calculation
```

## 📈 Interpreting the Data

### Healthy Battery Behavior:
- **Voltage range**: 3.7V - 4.2V
- **Smooth curves**: No sudden jumps
- **Consistent ADC**: Stable readings
- **Linear percentage**: Matches voltage curve

### Problematic Patterns:
- **Voltage spikes/drops**: Hardware issues
- **Inconsistent ADC**: Connection problems
- **Weird charging curve**: Battery/charger issues
- **Percentage not matching voltage**: Calibration needed

## 🔧 Troubleshooting

### "ModuleNotFoundError: No module named 'serial'":
- Install dependencies in virtual environment:
  ```bash
  cd omiGlass/firmware
  python3 -m venv battery_monitor_env
  source battery_monitor_env/bin/activate
  pip install pyserial matplotlib
  ```
- Or use the launcher script: `./run_battery_monitor.sh`

### "Could not connect to serial port":
- Check port name: `ls /dev/cu.*` (macOS) or Device Manager (Windows)
- Close Arduino IDE Serial Monitor first
- Verify device is connected and recognized
- Use the correct port from the list shown by the launcher script

### "No data appearing":
- Ensure firmware is flashed with monitoring code
- Check baud rate is 921600
- Look for "DATA," lines in terminal output

### Graphs not updating:
- Close and restart the Python script
- Check for Python errors in terminal
- Verify matplotlib is installed correctly in the virtual environment

## 📝 Data Format

The firmware outputs CSV data every 5 seconds:
```
DATA,timestamp_ms,adc_raw,adc_voltage,battery_voltage,percentage
DATA,12500,1264,1.019,3.800,45
```

You can also capture this data to a file:
```bash
python3 battery_monitor.py /dev/cu.usbserial-0001 | tee battery_log.txt
```

## 🎯 Use Cases

- **Debug charging issues**
- **Calibrate voltage divider**
- **Monitor battery health over time**
- **Validate percentage calculations**
- **Test different charging scenarios**
- **Detect hardware problems**

Happy monitoring! 🔋📊 