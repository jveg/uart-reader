import ctypes
import numpy as np
import os
import pathlib
import random
import serial
import select
import socket
import struct
import subprocess
import time

################################################################################
# Function Definitions
################################################################################

# Generate a valid set of inputs to be formatted for a UART message
def generate_inputs():
    pc = random.randint(0,100)
    x = np.float32(random.uniform(0,100))
    y = np.float32(random.uniform(0,100))
    z = np.float32(random.uniform(0,100))
    return pc, x, y, z

# Send a valid formatted UART message to serial connection ser
def send_valid_message(ser, pkt, x, y, z):
    # Set message to be of the correct format
    # ! : network (big endian)
    # c : char (1 byte)
    # I : unsigned integer (4 bytes)
    # f : float (4 byte)
    fmt = '!ccccIfff'
    ser.write(struct.pack(fmt,sb1,sb2,sb3,sb4,pkt,x,y,z))

# Send a series of num_bytes random 1b values to the serial connection ser
def send_random_bytes(ser, num_bytes):
    for i in range(num_bytes):
        c = random.randbytes(1)
        ser.write(struct.pack('!c', c))

def send_tc_input_messages(num):
    inputs = []
    for i in range(num):
        input_pc, input_x, input_y, input_z = generate_inputs()
        send_valid_message(s, input_pc, input_x, input_y, input_z)
        inputs.append([input_pc, input_x, input_y, input_z])
    return inputs

def recv_tc_messages():
    outputs = []
    loop_control = 1
    while loop_control == 1:
        # wait for data for 1s before timing out
        ready = select.select([sock], [], [], 1)
        # if data is available on the socket this test is considered a failure 
        if ready[0]:
            data, addr = sock.recvfrom(1024)
            pc_out, x_out, y_out, z_out = struct.unpack('@Ifff', data)
            outputs.append([pc_out, x_out, y_out, z_out])
        else:
            loop_control = 0
    return outputs

# Validate received data against sent data and print results to console
def validate_data(inputs, outputs):
    matched = 0
    if len(inputs) != len(outputs):
        print(f"Number of Messages Sent ({len(inputs)}) do not match Number of Messages Received ({len(outputs)})")
        return False
    msgs = len(inputs)
    for i in range(msgs):
        if inputs[i] == outputs[i]:
            matched += 1
        else:
            print(f"Mismatched data -> Message {i}/{msgs}")
            print(f"Sent: {inputs[i]}")
            print(f"Revd: {outputs[i]}")
    print(f"{matched}/{msgs} messages are correct!")
    return matched == msgs

################################################################################
# Test Setup
################################################################################

# Load the UUT from the c shared object library
libname=pathlib.Path().absolute()/"libuut.so"
uut = ctypes.CDLL(libname)

# Set up receiver socket
UDP_IP = "127.0.0.1"
UDP_PORT = 4321
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Set up serial connection
socat_process = subprocess.Popen(["socat", "pty,rawer,echo=0,link=/dev/tty1", "pty,rawer,echo=0,link=./writer"], start_new_session=True)

test_dev_name = "./writer"
while not os.path.exists(test_dev_name):
    time.sleep(1)

#print(s_name)
s = serial.Serial(test_dev_name, 921600, timeout=1, bytesize=8, parity='N', stopbits=1)

# Start the unit under test thread
device_name = "/dev/tty1"
c_dev_name = ctypes.c_char_p(device_name.encode('utf-8'))
uut.uart_repeater_initialize(c_dev_name)

# Start byte pattern
sb1 = bytes.fromhex("7f")
sb2 = bytes.fromhex("f0")
sb3 = bytes.fromhex("1c")
sb4 = bytes.fromhex("af")

################################################################################
# Test Cases
################################################################################
test_cases = 5
tests_passed = 0
################################################################################
msgs = 100
print(f"[TEST 1] Sending {msgs} subseqent valid messages")

# Generate X messages to send
inputs = []
inputs = send_tc_input_messages(msgs)

# Receive X messages
outputs = []
outputs = recv_tc_messages()

# Validate data and print results
if validate_data(inputs, outputs):
    tests_passed+=1
    print("TEST PASSED!\n")
else:
    print("TEST FAILED.\n")

################################################################################
print("[TEST 2] Extract message from random byte stream")
inputs = []
send_random_bytes(s,100)
inputs = send_tc_input_messages(1)
send_random_bytes(s,100)

outputs = []
outputs = recv_tc_messages()

if validate_data(inputs, outputs):
    tests_passed+=1
    print("TEST PASSED!\n")
else:
    print("TEST FAILED.\n")

################################################################################
print("[TEST 3] Ignore invalid Start-of-Frame, and extract correct message")
# Send a nearly correct, mismatched byte stream follwed by random data
s.write(struct.pack('!cccc',sb1,sb3,sb4,sb2))
send_random_bytes(s,20)
inputs = []
inputs = send_tc_input_messages(1)
send_random_bytes(s,100)

outputs = []
outputs = recv_tc_messages()

if validate_data(inputs, outputs):
    tests_passed+=1
    print("TEST PASSED!\n")
else:
    print("TEST FAILED.\n")

################################################################################
print("[TEST 4] Receive multiple messages separated by random bytes")
inputs = []
inputs += send_tc_input_messages(1)
send_random_bytes(s,1000)
inputs += send_tc_input_messages(3)
send_random_bytes(s,50)
inputs += send_tc_input_messages(10)
send_random_bytes(s,1)
inputs += send_tc_input_messages(1)
send_random_bytes(s,50)

outputs = []
outputs = recv_tc_messages()

if validate_data(inputs, outputs):
    tests_passed+=1
    print("TEST PASSED!\n")
else:
    print("TEST FAILED.\n")

################################################################################
print("[TEST 5] No return when no valid message is sent (timeout of 1s)")
send_random_bytes(s,4096)

# wait for data for 1s before timing out
ready = select.select([sock], [], [], 1)
# if data is available on the socket this test is considered a failure 
if ready[0]:
    print("Message received when it should not have.")
    print("TEST FAILED.\n")
else:
    tests_passed+=1
    print("TEST PASSED!\n")

print(f"#########################################")
print(f"{tests_passed}/{test_cases} TESTS PASSED!")

# Shutdown test by closing all sockets and terminals
s.close()
uut.uart_repeater_shutdown()
socat_process.terminate()
