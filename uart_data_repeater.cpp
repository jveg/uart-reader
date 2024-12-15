#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "imu_parser.h"
#include "uut.h"

pthread_t uart_thread;
int global_shutdown_flag = 0;
int uart_port;
int udp_sock;


// Thread operation.
void *thread_func(void *arg) {
    imu_message_t new_message_p[MAX_NUM_MSGS];
    int messages_found;
    int bytes_sent;

    while (global_shutdown_flag == 0) {
        messages_found = read_from_serial(uart_port, new_message_p);
        if (messages_found > 0) {
            // iterate over all messages found and send to localhost
            for (int i = 0; i < messages_found; i++) {
                bytes_sent = send(udp_sock, &new_message_p[i], sizeof(new_message_p[i]), 0);
                if (bytes_sent <= 0) {
                    debug_print("[ERROR] Message send failed with: %s\n", strerror(errno));
                }
            }
        }
 
        //Sleep for 80ms
        usleep(80000);
    }
    return NULL;
}

// Initialize and kickoff threads
int uart_repeater_initialize(const char *dev_name) {
    struct sockaddr_in server_addr;
    // Setup and open UART serial port for data input
    uart_port = open_serial_port(dev_name);

    if (uart_port != IMU_ERROR) {
        // Setup UDP socket to send to localhost
        // create socket
        udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_sock < 0) {
            debug_print("[ERROR] Failed to create socket: %s\n", strerror(errno));
            return IMU_ERROR;
        }
        // setup address
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(4321);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // connect address to socket
        if (connect(udp_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            debug_print("[ERROR] Failed to connect to addres: %s\n", strerror(errno));
            close(udp_sock);
            return IMU_ERROR;
        }

        // Create and kickoff thread
        if (pthread_create(&uart_thread, NULL, *thread_func, NULL) != 0) {
            debug_print("[ERROR] Failed to create pthread.\n");
            close(udp_sock);
            return IMU_ERROR;
        }
        
        return IMU_OK;
    }
    return IMU_ERROR;
}

// Close all resources, join all threads 
void uart_repeater_shutdown() {
    // set global shutdown flag so that the process thread can exit during its
    // next iteration
    global_shutdown_flag = 1;
    // wait for the thread to join
    pthread_join(uart_thread, NULL);
    // close necessary ports and sockets
    close_serial_port(uart_port);
    close(udp_sock);
}