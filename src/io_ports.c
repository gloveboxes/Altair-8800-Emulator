/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "io_ports.h"
#include <string.h>

typedef struct
{
    size_t len;
    size_t count;
    char buffer[128];
} REQUEST_UNIT_T;

static REQUEST_UNIT_T ru;

/// <summary>
/// Intel 8080 OUT Port handler
/// </summary>
/// <param name="port"></param>
/// <param name="data"></param>
void io_port_out(uint8_t port, uint8_t data)
{
    memset(&ru, 0x00, sizeof(REQUEST_UNIT_T));

    switch (port)
    {
        // Time IO Ports - Millisecond Timers (use permanent timer for reliability)
        case 24: // Timer 0 - Set milliseconds timer high byte (bits 15-8)
        case 25: // Timer 0 - Set milliseconds timer low byte (bits 7-0) and start timer
        case 26: // Timer 1 - Set milliseconds timer high byte (bits 15-8)
        case 27: // Timer 1 - Set milliseconds timer low byte (bits 7-0) and start timer
        case 28: // Timer 2 - Set milliseconds timer high byte (bits 15-8)
        case 29: // Timer 2 - Set milliseconds timer low byte (bits 7-0) and start timer
        // Time IO Ports - Seconds Timer (uses one-shot timer)
        case 30: // Set seconds timer
        // Time IO Ports - System Time
        case 41: // Load system tick count
        case 42: // Load UTC date and time
        case 43: // Load local date and time
            ru.len = time_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // Azure IoT IO Ports
        case 31: // Publish weather json
        case 32: // Publish weather
            ru.len = azure_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // Weather IO Ports
        case 34: // Load weather key
        case 35: // Load weather value
        case 36: // Load location key
        case 37: // Load location value
        case 38: // Load pollution key
        case 39: // Load pollution value
            ru.len = weather_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // Utility IO Ports
        case 44: // Load random number to seed Microsoft/Altair BASIC randomize command
        case 45: // generate a 16 bit random number and load low byte and high byte
        case 70: // Load Altair version number
        case 71: // Load OS Version
        case 72: // Load Azure Sphere Device ID - note for security reasons only getting the first 8 chars
            ru.len = utility_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // Onboard IO Ports
        case 60: // Red LEB
        case 61: // Green LEB
        case 62: // Blue LEB
        case 63: // Load Onboard sensors temperature, pressure, and light
        case 64: // Load accelerometer data and settings
            ru.len = onboard_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // Power IO Ports
        case 66: // Set Azure Sphere enable/disable/sleep power management
        case 67: // Set Azure Sphere wake from sleep in X seconds
            ru.len = power_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // LED Matrix IO Ports
        case 65:  // Set brightness of the 8x8 LED Panel
        case 80:  // Panel_mode 0 = bus data, 1 = font, 2 = bitmap
        case 81:  // Set font color
        case 82:  // Set pixel red color
        case 83:  // Set pixel green color
        case 84:  // Set pixel blue color
        case 85:  // Display character
        case 90:  // Bitmap row 0
        case 91:  // Bitmap row 1
        case 92:  // Bitmap row 2
        case 93:  // Bitmap row 3
        case 94:  // Bitmap row 4
        case 95:  // Bitmap row 5
        case 96:  // Bitmap row 6
        case 97:  // Bitmap row 7
        case 98:  // Pixel on
        case 99:  // Pixel off
        case 100: // Pixel flip
        case 101: // Clear all pixels
        case 102: // Bitmap draw
            ru.len = led_matrix_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // File transfer IO Ports
        case 109: // Set webget.index = 0
        case 110: // Set getfile custom endpoint url
        case 111: // Load getfile (gf) custom endpoint url
        case 114: // copy file from web server to mutable storage
            ru.len = file_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        // OpenAI IO Ports
        case 120: // Set system message
        case 121: // Set user Message
        case 122: // Set assistant message
        case 123: // Clear all messages
        case 124: // Load OpenAI stream
        case 125: // Cancel ChatGPT stream
            ru.len = openai_output(port, data, ru.buffer, sizeof(ru.buffer));
            break;

        default:
            break;
    }
}

/// <summary>
/// Intel 8080 IN Port handler
/// </summary>
/// <param name="port"></param>
/// <returns></returns>
uint8_t io_port_in(uint8_t port)
{
    uint8_t retVal = 0;

    switch (port)
    {
        // Time IO Ports - Timer Status (returns 1=running, 0=expired/not started)
        case 24: // Timer 0 - Has milliseconds timer expired (same as port 25)
        case 25: // Timer 0 - Has milliseconds timer expired  
        case 26: // Timer 1 - Has milliseconds timer expired (same as port 27)
        case 27: // Timer 1 - Has milliseconds timer expired
        case 28: // Timer 2 - Has milliseconds timer expired (same as port 29)
        case 29: // Timer 2 - Has milliseconds timer expired
        case 30: // Has seconds timer expired
            retVal = time_input(port);
            break;

        // Weather IO Ports
        case 31: // Is publish weather json pending
        case 32: // Is publish weather pending
            retVal = azure_input(port);
            break;

        // File transfer IO Ports
        case 33:  // Is copyx file need copied and loaded
        case 201: // Read file from http(s) web server
            retVal = file_input(port);
            break;

        // Utility IO Ports
        case 69: // Is network ready
            retVal = utility_input(port);
            break;

        // Request Unit IO Ports
        case 200: // Get next request unit byte
            retVal = ru.count < ru.len && ru.count < sizeof(ru.buffer) ? ru.buffer[ru.count++] : 0x00;
            break;

        // OpenAI IO Ports
        case 120: // Send API request / get status
        case 121: // get buffer length low byte
        case 122: // get buffer length high byte
        case 123: // check if response data available
        case 124: // read one byte from response
        case 125: // check if response complete
        case 126: // get finish reason length
        case 127: // get finish reason byte
            retVal = openai_input(port);
            break;

        default:
            retVal = 0x00;
    }

    return retVal;
}
