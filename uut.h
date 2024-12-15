#ifndef UUT_H
#define UUT_H

// This header is intended to be the interface between the C UUT software and 
// the python Test Code

extern "C" {
    int uart_repeater_initialize(const char *dev_name);
    void uart_repeater_shutdown();
}

#endif