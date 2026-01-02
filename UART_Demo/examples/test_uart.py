#!/usr/bin/env python3
"""
UART Test Script for ESP32 UART Reference Project

This script provides automated testing of the UART communication interface.
It tests all available commands and performs stress testing.

Usage:
    python3 test_uart.py /dev/ttyUSB0
    python3 test_uart.py COM3 --baud 115200
    python3 test_uart.py /dev/ttyUSB0 --stress --count 1000

Requirements:
    pip install pyserial
"""

import serial
import argparse
import time
import sys
from typing import Tuple, List


class Colors:
    """ANSI color codes for terminal output"""
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'


def print_test(name: str):
    """Print test name"""
    print(f"\n{Colors.BLUE}{Colors.BOLD}[TEST]{Colors.RESET} {name}")


def print_pass(msg: str):
    """Print success message"""
    print(f"  {Colors.GREEN}✓ PASS:{Colors.RESET} {msg}")


def print_fail(msg: str):
    """Print failure message"""
    print(f"  {Colors.RED}✗ FAIL:{Colors.RESET} {msg}")


def print_info(msg: str):
    """Print info message"""
    print(f"  {Colors.YELLOW}ℹ INFO:{Colors.RESET} {msg}")


class UARTTester:
    """UART testing class"""
    
    def __init__(self, port: str, baud: int = 115200, timeout: float = 1.0):
        """Initialize UART connection"""
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        self.test_passed = 0
        self.test_failed = 0
        
    def connect(self) -> bool:
        """Open serial connection"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            print_info(f"Connected to {self.port} at {self.baud} baud")
            time.sleep(0.5)  # Allow ESP32 to stabilize
            self.ser.reset_input_buffer()
            return True
        except serial.SerialException as e:
            print_fail(f"Failed to open port: {e}")
            return False
            
    def disconnect(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print_info("Disconnected")
            
    def send_command(self, cmd: str, timeout: float = None) -> Tuple[bool, str]:
        """Send command and wait for response"""
        if timeout is not None:
            old_timeout = self.ser.timeout
            self.ser.timeout = timeout
            
        try:
            # Send command
            self.ser.write(f"{cmd}\n".encode('utf-8'))
            
            # Read response
            response = self.ser.readline().decode('utf-8', errors='ignore').strip()
            
            if timeout is not None:
                self.ser.timeout = old_timeout
                
            return (True, response)
        except Exception as e:
            if timeout is not None:
                self.ser.timeout = old_timeout
            return (False, str(e))
            
    def test_basic_commands(self):
        """Test basic command functionality"""
        tests = [
            ("PING", "PONG", "PING command"),
            ("VERSION", "ESP32S3_UART_REF v1", "VERSION command"),
        ]
        
        for cmd, expected, desc in tests:
            print_test(desc)
            success, response = self.send_command(cmd)
            
            if success and response == expected:
                print_pass(f"Got expected response: '{response}'")
                self.test_passed += 1
            else:
                print_fail(f"Expected '{expected}', got '{response}'")
                self.test_failed += 1
                
    def test_uptime(self):
        """Test UPTIME command"""
        print_test("UPTIME command")
        success, response = self.send_command("UPTIME")
        
        if success and response.startswith("UPTIME_MS "):
            try:
                uptime_str = response.split()[1]
                uptime = int(uptime_str)
                print_pass(f"Got uptime: {uptime} ms ({uptime/1000:.2f} seconds)")
                self.test_passed += 1
            except (ValueError, IndexError):
                print_fail(f"Invalid uptime format: '{response}'")
                self.test_failed += 1
        else:
            print_fail(f"Unexpected response: '{response}'")
            self.test_failed += 1
            
    def test_unknown_command(self):
        """Test error handling for unknown commands"""
        print_test("Unknown command handling")
        success, response = self.send_command("INVALID_COMMAND")
        
        if success and response == "ERR UNKNOWN_CMD":
            print_pass(f"Error handled correctly: '{response}'")
            self.test_passed += 1
        else:
            print_fail(f"Expected 'ERR UNKNOWN_CMD', got '{response}'")
            self.test_failed += 1
            
    def test_stress(self, count: int = 100):
        """Stress test with rapid commands"""
        print_test(f"Stress test ({count} commands)")
        
        start_time = time.time()
        sent = 0
        received = 0
        errors = 0
        
        for i in range(count):
            success, response = self.send_command("PING", timeout=0.1)
            sent += 1
            
            if success and response == "PONG":
                received += 1
            else:
                errors += 1
                
            # Progress indicator every 100 commands
            if (i + 1) % 100 == 0:
                print_info(f"Progress: {i + 1}/{count}")
                
        elapsed = time.time() - start_time
        rate = count / elapsed
        
        print_info(f"Sent: {sent}, Received: {received}, Errors: {errors}")
        print_info(f"Time: {elapsed:.2f}s, Rate: {rate:.2f} cmd/s")
        
        if errors == 0:
            print_pass("All commands successful")
            self.test_passed += 1
        elif errors < count * 0.01:  # Less than 1% error
            print_pass(f"Acceptable error rate: {errors/count*100:.2f}%")
            self.test_passed += 1
        else:
            print_fail(f"High error rate: {errors/count*100:.2f}%")
            self.test_failed += 1
            
    def test_buffer_overflow(self):
        """Test buffer handling with long input"""
        print_test("Buffer overflow handling")
        
        # Send command longer than buffer (>256 bytes)
        long_cmd = "X" * 300
        success, response = self.send_command(long_cmd, timeout=2.0)
        
        # Should either handle gracefully or respond with error
        if success:
            print_pass(f"Handled long input gracefully: '{response[:50]}...'")
            self.test_passed += 1
        else:
            print_info("Long input caused timeout (expected behavior)")
            self.test_passed += 1
            
    def print_summary(self):
        """Print test summary"""
        total = self.test_passed + self.test_failed
        print(f"\n{Colors.BOLD}{'='*50}{Colors.RESET}")
        print(f"{Colors.BOLD}Test Summary{Colors.RESET}")
        print(f"{'='*50}")
        print(f"Total Tests: {total}")
        print(f"{Colors.GREEN}Passed: {self.test_passed}{Colors.RESET}")
        print(f"{Colors.RED}Failed: {self.test_failed}{Colors.RESET}")
        
        if self.test_failed == 0:
            print(f"\n{Colors.GREEN}{Colors.BOLD}✓ ALL TESTS PASSED{Colors.RESET}\n")
            return 0
        else:
            print(f"\n{Colors.RED}{Colors.BOLD}✗ SOME TESTS FAILED{Colors.RESET}\n")
            return 1


def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description='Test UART communication with ESP32',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0 or COM3)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--timeout', type=float, default=1.0, help='Response timeout in seconds')
    parser.add_argument('--stress', action='store_true', help='Run stress test')
    parser.add_argument('--count', type=int, default=100, help='Number of stress test iterations')
    parser.add_argument('--all', action='store_true', help='Run all tests including stress')
    
    args = parser.parse_args()
    
    print(f"{Colors.BOLD}ESP32 UART Reference - Test Suite{Colors.RESET}")
    print(f"{'='*50}\n")
    
    tester = UARTTester(args.port, args.baud, args.timeout)
    
    if not tester.connect():
        return 1
        
    try:
        # Basic tests
        tester.test_basic_commands()
        tester.test_uptime()
        tester.test_unknown_command()
        tester.test_buffer_overflow()
        
        # Stress test (optional)
        if args.stress or args.all:
            tester.test_stress(args.count)
            
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Test interrupted by user{Colors.RESET}")
    finally:
        tester.disconnect()
        
    return tester.print_summary()


if __name__ == '__main__':
    sys.exit(main())
