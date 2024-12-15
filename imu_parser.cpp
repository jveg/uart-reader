#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "imu_parser.h"

float bytes_to_float(u_char raw_bytes[4]) {
    float f;
    // Determine Endian-ness to ensure floats are output correctly
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        u_char proper_byte_order[] = {raw_bytes[3], raw_bytes[2], raw_bytes[1], raw_bytes[0]};
    #else
        u_char * proper_byte_order = raw_bytes;
    #endif

    memcpy(&f, &proper_byte_order, sizeof(f));
    return f;
}


int open_serial_port(const char *dev_name) {
    int status = IMU_OK;
    /* 
     * FLAGS:
     *  O_RDWR     = read and write
     *  O_NOCTTY   = prevent from controlling serial port
     *  O_NONBLOCK = set to non-blocking mode
     */
    int port_flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    struct termios settings;

    // Attempt to open serial port file descripter
    int fd = open(dev_name, port_flags);

    if (fd == -1) {
        debug_print("[ERROR] Failed to open serial port fd: %s\n", strerror(errno));
        // Return early as there is no point in doing any more processing if the
        // fd couldn't be opened
        return IMU_ERROR;
    }

    // Configure serial port settings as per requirements
    // Get current settings of serial port
    if (tcgetattr(fd, &settings) != 0) {
        debug_print("[ERROR] Failed to get port attributes: %s\n", strerror(errno));
        status = IMU_ERROR;
    }
    else {
        // Set 8N1
        settings.c_cflag &= ~( PARENB | CSTOPB | CSIZE );
        settings.c_cflag |= CS8;

        // Set Baud Rate to 921600
        if (cfsetspeed(&settings, B921600) != 0) {
            debug_print("[ERROR] Failed to set baudrate: %s\n", strerror(errno));
            status = IMU_ERROR;
        }
        else {
            // Apply settings
            if (tcsetattr(fd, TCSANOW, &settings) != 0) {
                debug_print("[ERROR] Failed to set new attributes: %s\n", strerror(errno));
                status = IMU_ERROR;
            }
        }
    }

    if (status == IMU_ERROR) {
        debug_print("[INFO] An error occurred. Closing serial port.\n");
        close(fd);
    }
    else {
        // Return the now configured port file descriptor for later use
        status = fd;
    }

    return status;
}

int read_from_serial(int port, imu_message_t *messages) {
    int status = IMU_ERROR;
    int loop_control = 1;
    int bytes_read = 0;
    u_char buffer[MAX_BUFFER_SIZE];
    imu_message_t output_msg_buffer[MAX_NUM_MSGS];
    int count = 0;

    int offset = 4;
    // vars to hold values for processing before copying to output
    u_int32_t packet_count;
    u_char tmp_X[4];
    u_char tmp_Y[4];
    u_char tmp_Z[4];

    imu_message_t new_message;
    
    // Clear the output buffer before doing any reading
    memset(output_msg_buffer, 0, sizeof(output_msg_buffer));

    while (loop_control) {
        // Clear buffer in between reads
        memset(buffer, 0, sizeof(buffer));

        // Read data from the line
        bytes_read = read(port, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            int byte = 0;
            int shift = 1;
            int bytes_remaining = 0;
            for (byte = 0; byte < bytes_read; byte+=shift) {
                // Assume to only shift the buffer by 1 byte. This only changes
                // if the correct data is found.
                shift = 1;

                // Ensure there are enough bytes in the buffer to at least make
                // up one whole IMU message, as well as the first start of frame
                // byte is found.
                bytes_remaining = bytes_read - byte;

                if (bytes_remaining >= IMU_MAX_MESSAGE_SIZE && buffer[byte] == 0x7f) {
                    // Verify the rest of the start pattern is there
                    if (buffer[byte+1] == 0xf0 && buffer[byte+2] == 0x1c && buffer[byte+3] == 0xaf) {
                        // Expected fields into temp buffers for formatting
                        memcpy(&packet_count, &buffer[byte+offset],sizeof(packet_count));
                        memcpy(&tmp_X, &buffer[byte+2*offset], sizeof(tmp_X));
                        memcpy(&tmp_Y, &buffer[byte+3*offset], sizeof(tmp_Y));
                        memcpy(&tmp_Z, &buffer[byte+4*offset], sizeof(tmp_Z));

                        // Ensure proper Endian-ness before copying over to 
                        // message buffer
                        new_message.packet_count = ntohl(packet_count);
                        new_message.X_Rate_rdps = bytes_to_float(tmp_X);
                        new_message.Y_Rate_rdps = bytes_to_float(tmp_Y);
                        new_message.Z_Rate_rdps = bytes_to_float(tmp_Z);

                        // Copy finished message into output buffer
                        memcpy(&output_msg_buffer[count],&new_message,sizeof(new_message));
                        count++;

                        // Clear temp buffers
                        packet_count = 0;
                        memset(&tmp_X, 0, sizeof(tmp_X));
                        memset(&tmp_Y, 0, sizeof(tmp_Y));
                        memset(&tmp_Z, 0, sizeof(tmp_Z));

                        // Shift the current read byte by the number of bytes
                        // copied out
                        shift=IMU_MAX_MESSAGE_SIZE;
                    }
                }
            }
        }
        else if (bytes_read == -1) {
            switch (errno) {
                case EAGAIN:
                    status = IMU_OK;
                    break;
                default:
                    debug_print("[ERROR] An error occurred reading bytes: %s\n", strerror(errno));
                    close(port);
                    break;
            }
            // break out of the loop if an error occurred.
            loop_control = 0;
        }
    }

    // copy buffer out
    memcpy(messages, output_msg_buffer, sizeof(output_msg_buffer));

    if (status != IMU_ERROR) status = count;
    return status;
}

void close_serial_port(int fd) {
    close(fd);
}