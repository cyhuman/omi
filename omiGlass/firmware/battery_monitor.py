#!/usr/bin/env python3
"""
OpenGlass Battery Monitor
Real-time plotting of battery voltage, charging status, and ADC readings

Requirements:
pip install pyserial matplotlib

Usage:
python3 battery_monitor.py [COM_PORT]

Example:
python3 battery_monitor.py /dev/cu.usbserial-0001  # macOS
python3 battery_monitor.py COM3                    # Windows
"""

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import sys
import time
import re

class BatteryMonitor:
    def __init__(self, port, baud_rate=921600):
        self.port = port
        self.baud_rate = baud_rate
        self.ser = None
        
        # Data storage (keep last 500 points = ~40 minutes at 5s intervals)
        self.max_points = 500
        self.timestamps = deque(maxlen=self.max_points)
        self.adc_raw = deque(maxlen=self.max_points)
        self.adc_voltage = deque(maxlen=self.max_points)
        self.battery_voltage = deque(maxlen=self.max_points)
        self.percentage = deque(maxlen=self.max_points)
        
        # Plot setup
        self.fig, ((self.ax1, self.ax2), (self.ax3, self.ax4)) = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('OpenGlass Battery Monitor - Real-time', fontsize=16)
        
        # Initialize plots
        self.line1, = self.ax1.plot([], [], 'b-', linewidth=2)
        self.line2, = self.ax2.plot([], [], 'g-', linewidth=2)
        self.line3, = self.ax3.plot([], [], 'r-', linewidth=2)
        self.line4, = self.ax4.plot([], [], 'purple', linewidth=2)
        
        # Configure axes
        self.ax1.set_title('ADC Raw Value')
        self.ax1.set_ylabel('ADC (0-4095)')
        self.ax1.grid(True)
        
        self.ax2.set_title('ADC Voltage')
        self.ax2.set_ylabel('Voltage (V)')
        self.ax2.grid(True)
        
        self.ax3.set_title('Battery Voltage')
        self.ax3.set_ylabel('Voltage (V)')
        self.ax3.grid(True)
        self.ax3.axhline(y=3.7, color='orange', linestyle='--', alpha=0.7, label='Min (3.7V)')
        self.ax3.axhline(y=4.2, color='green', linestyle='--', alpha=0.7, label='Max (4.2V)')
        self.ax3.legend()
        
        self.ax4.set_title('Battery Percentage')
        self.ax4.set_ylabel('Percentage (%)')
        self.ax4.set_xlabel('Time (seconds)')
        self.ax4.grid(True)
        
        plt.tight_layout()
        
    def connect(self):
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baud_rate, timeout=1)
            print(f"Connected to {self.port} at {self.baud_rate} baud")
            return True
        except Exception as e:
            print(f"Error connecting to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def parse_data_line(self, line):
        """Parse a CSV data line from firmware"""
        try:
            # Expected format: DATA,timestamp,adc_raw,adc_voltage,battery_voltage,percentage
            if line.startswith('DATA,'):
                parts = line.strip().split(',')
                if len(parts) >= 6:
                    timestamp = int(parts[1]) / 1000.0  # Convert ms to seconds
                    adc_raw = int(parts[2])
                    adc_voltage = float(parts[3])
                    battery_voltage = float(parts[4])
                    percentage = int(parts[5])
                    
                    return timestamp, adc_raw, adc_voltage, battery_voltage, percentage
        except (ValueError, IndexError) as e:
            print(f"Error parsing line: {line.strip()} - {e}")
        return None
    
    def update_data(self, frame):
        """Update function for animation"""
        if not self.ser or not self.ser.is_open:
            return self.line1, self.line2, self.line3, self.line4
        
        # Read available lines
        while self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore')
                print(line.strip())  # Print all serial output
                
                # Parse data lines
                data = self.parse_data_line(line)
                if data:
                    timestamp, adc_raw, adc_voltage, battery_voltage, percentage = data
                    
                    # Add to collections
                    self.timestamps.append(timestamp)
                    self.adc_raw.append(adc_raw)
                    self.adc_voltage.append(adc_voltage)
                    self.battery_voltage.append(battery_voltage)
                    self.percentage.append(percentage)
                    
                    # Update plots
                    self.line1.set_data(list(self.timestamps), list(self.adc_raw))
                    self.line2.set_data(list(self.timestamps), list(self.adc_voltage))
                    self.line3.set_data(list(self.timestamps), list(self.battery_voltage))
                    self.line4.set_data(list(self.timestamps), list(self.percentage))
                    
                    # Auto-scale axes
                    if len(self.timestamps) > 1:
                        x_min, x_max = min(self.timestamps), max(self.timestamps)
                        x_range = x_max - x_min
                        
                        for ax in [self.ax1, self.ax2, self.ax3, self.ax4]:
                            ax.set_xlim(x_min - x_range*0.05, x_max + x_range*0.05)
                        
                        self.ax1.set_ylim(min(self.adc_raw)*0.95, max(self.adc_raw)*1.05)
                        self.ax2.set_ylim(min(self.adc_voltage)*0.95, max(self.adc_voltage)*1.05)
                        self.ax3.set_ylim(2.5, 4.5)  # Fixed range for battery voltage
                        self.ax4.set_ylim(-5, 105)    # Fixed range for percentage
            
            except Exception as e:
                print(f"Error reading serial: {e}")
        
        return self.line1, self.line2, self.line3, self.line4
    
    def start_monitoring(self):
        """Start real-time monitoring"""
        if not self.connect():
            return
        
        print("Starting battery monitoring...")
        print("Close the plot window to stop monitoring")
        
        # Start animation
        ani = animation.FuncAnimation(
            self.fig, self.update_data, interval=100, blit=False
        )
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\nMonitoring stopped by user")
        finally:
            self.disconnect()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 battery_monitor.py <serial_port>")
        print("\nExamples:")
        print("  macOS:   python3 battery_monitor.py /dev/cu.usbserial-0001")
        print("  Windows: python3 battery_monitor.py COM3")
        print("  Linux:   python3 battery_monitor.py /dev/ttyUSB0")
        sys.exit(1)
    
    port = sys.argv[1]
    monitor = BatteryMonitor(port)
    monitor.start_monitoring()

if __name__ == "__main__":
    main() 