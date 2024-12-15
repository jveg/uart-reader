#ifndef IMU_PARSER_H
#define IMU_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef DEBUG
#define debug_print(args ...) fprintf(stderr, args);
#else
#define debug_print(args ...)
#endif

#define IMU_OK     0
#define IMU_ERROR -1
#define IMU_MAX_MESSAGE_SIZE 20
#define BAUD_RATE 921600

// (baudrate / 8 )  = bytes per second
const int MAX_BUFFER_SIZE = (BAUD_RATE / 8);

// (bytes per second / IMU_MAX_MESSAGE_SIZE) = max IMU_Messages per second
const int MAX_NUM_MSGS    = ceil(MAX_BUFFER_SIZE/IMU_MAX_MESSAGE_SIZE);

// message type containing pertinent imu information
typedef struct message {
    u_int32_t packet_count;
    float X_Rate_rdps;
    float Y_Rate_rdps;
    float Z_Rate_rdps;
} imu_message_t;

/*
********************************************************************************
* open_serial_port
********************************************************************************
* Description: Opens and configures serial port
* Inputs:   
*    dev_name = name of desired serial device to configure
* Returns:
*    IMU_OK or IMU_ERROR
*/
int open_serial_port(const char *dev_name);

/*
********************************************************************************
* read_from_serial
********************************************************************************
* Description: Read data from serial port connection
* Inputs:   
*    port = file descriptor for desired read port
* Outputs:
*    messages = contains all successfully received messages from serial port
* Returns:
*    Either the number of successful messages received or IMU_ERROR
*/
int read_from_serial(int port, imu_message_t *messages);

/*
********************************************************************************
* close_serial_port
********************************************************************************
* Description: Closes serial port
* Inputs:   
*    fd = file descriptor for desired port to close
*/
void close_serial_port(int fd);

#endif
