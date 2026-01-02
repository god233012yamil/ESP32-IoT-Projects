#!/usr/bin/env python3
"""
Simple UART Communication Example

This script demonstrates basic communication with the ESP32 UART Reference project.
It sends commands and displays responses in a user-friendly way.

Requirements:
    pip install pyserial

Usage:
    python3 simple_example.py /dev/ttyUSB0
    python3 simple_example.py COM3
"""

import serial
import sys
import time


def send_command(ser, command):
    """Send a command and return the response"""
    # Send command with newline
    ser.write(f"{command}\n".encode('utf-8'))
    
    # Wait for response (with timeout)
    response = ser.readline().decode('utf-8', errors='ignore').strip()
    
    return response


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 simple_example.py <serial_port>")
        print("Example: python3 simple_example.py /dev/ttyUSB0")
        sys.exit(1)
    
    port = sys.argv[1]
    baud = 115200
    
    print(f"Connecting to {port} at {baud} baud...")
    
    try:
        # Open serial port
        ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=1.0
        )
        
        print("Connected!\n")
        time.sleep(0.5)  # Give ESP32 time to stabilize
        
        # Clear any pending data
        ser.reset_input_buffer()
        
        # Send PING command
        print("Sending: PING")
        response = send_command(ser, "PING")
        print(f"Received: {response}\n")
        
        # Send VERSION command
        print("Sending: VERSION")
        response = send_command(ser, "VERSION")
        print(f"Received: {response}\n")
        
        # Send UPTIME command
        print("Sending: UPTIME")
        response = send_command(ser, "UPTIME")
        print(f"Received: {response}\n")
        
        # Try invalid command
        print("Sending: INVALID")
        response = send_command(ser, "INVALID")
        print(f"Received: {response}\n")
        
        # Interactive mode
        print("-" * 50)
        print("Interactive mode (Ctrl+C to exit)")
        print("-" * 50)
        
        while True:
            try:
                # Get command from user
                command = input("\nEnter command: ").strip()
                
                if not command:
                    continue
                
                # Send and display response
                response = send_command(ser, command)
                print(f"Response: {response}")
                
            except KeyboardInterrupt:
                print("\n\nExiting...")
                break
        
        # Close serial port
        ser.close()
        print("Disconnected")
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
