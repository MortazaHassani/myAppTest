#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "libspi.h"
#include <generated/csr.h>

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

static inline char reverse_nibbles(char c) {
    return ((c<<4) & 0xF0) | ((c >> 4) & 0xFF);
}

static inline uint32_t fix_bit_format(uint32_t input) {
    char a1 = (input >> 24) & 0xFF;
    char a2 = (input >> 16) & 0xFF;
    char a3 = (input >> 8) & 0xFF;
    char a4 = input & 0xFF;
    return (a1 << 24) | (a2 << 16) | (a3 << 8) | a4;
}

static inline uint32_t fetch(void) {
    startSPI();  // SPI set up and started
    
    // Dummy write to initiate read
    spimaster_mosi_write(0);  // Write a dummy byte to trigger SPI read
    spimaster_control_start_write(1);
    WaitXfer();  // Wait for the transaction to complete

    uint32_t result_raw = spimaster_miso_read();  // Read data from SPI
    uint32_t result = fix_bit_format(result_raw);  // Adjust the bit format 

    stopSPI();  // stop the SPI transaction 

    return result;
}

static bool is_Wait = false;

// Read a byte from the SPI's MISO line
static inline uint8_t fetch_miso_byte(void) {
    uint8_t miso_byte = 0x00;

    // Read the incoming data byte from the SPI bus
    miso_byte = fetch();

    is_Wait = !(miso_byte & 0x01);  // Example: Check if LSB is not set

    // Stop SPI to clean up after transaction
    stopSPI();

    return miso_byte;
}

static uint32_t fetch_spi_data(void) {
    uint32_t data = 0;
    startSPI();  // Prepare and start SPI interface

    // Assuming a command or dummy byte might be needed
    spimaster_mosi_write(0x00);
    spimaster_control_start_write(1);
    WaitXfer();  // Wait for the transaction to complete

    data = spimaster_miso_read();  // Read the combined data from SPI
    stopSPI();  // Stop the SPI transaction

    return data;
}

static uint16_t fetch_message(void) {
    uint32_t mosi_msg = 0x0;
    bool init = true;

    while (true) {
        uint32_t combined_data = fetch_spi_data();
        uint16_t mosi_data = (combined_data >> 16) & 0xFFFF; // Extract MOSI data
        uint16_t miso_data = combined_data & 0xFFFF;         // Extract MISO data

        if (init) {
            mosi_msg = mosi_data; // First data word is direct
        } else {
            mosi_msg = (mosi_msg << 16) | mosi_data; // Shift and append new data
        }
        init = false;

        // Check for specific pattern
        if ((mosi_msg & 0xFFFFFFFF) == 0x80D40024) {
            uint16_t bytes_to_read = (mosi_msg >> 24) & 0x1F; // Extract length from MOSI
            is_Wait = !(miso_data & 0x01); // Check wait state from MISO
            return bytes_to_read + 1;
        }
    }
}

static const char vmk_header[] = {//0x2c, 0x00, 0x00, 0x0, 0x01, 0x00, 0x00, 0x00, 0x03, 0x20, 0x00, 0x00
    0x2c, 0x00, 0x05, 0x0, 0x01, 0x00, 0x00, 0x00, 0x03, 0x20, 0x00, 0x00
};

#define MAXCOUNT 512
uint32_t buf[MAXCOUNT];


char message_buffer[4096*2];
volatile size_t msg_buffer_ptr = 0;
bool byte_found = false;

void process_messages() {
    printf("Starting SPI Sniffer...\n");
    InitSPI();
    uint16_t bytes_to_read;

    while (1) {
        bytes_to_read = fetch_message();
        for (int i = 0; i < bytes_to_read; i++) {
            char message = fetch_spi_data();
            message_buffer[msg_buffer_ptr++] = message;
            if (message_buffer[msg_buffer_ptr - 1] == 0x2c) {  // Assuming 0x2c is significant
                printf("Message with significant byte detected at index %d\n", msg_buffer_ptr-1);
                // Additional pattern checking and detailed printing can be here
            }
            if (msg_buffer_ptr >= sizeof(message_buffer)) {
                printf("Buffer full. Resetting buffer.\n");
                msg_buffer_ptr = 0; // Reset buffer index
            }
        }
    }
}

int sniffer_main() {
    printf("begin sniffing...\n");

    process_messages();  // Start the main processing loop
    return 0;
}