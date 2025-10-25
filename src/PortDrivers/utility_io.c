/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "utility_io.h"
#include "dx_utilities.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef AZURE_SPHERE
#include "device_id.h"
#include <applibs/applications.h>
#endif

// Device ID is 128 bytes. But we are only taking the first 8 bytes and allowing a terminating NULL;
#define DEVICE_ID_BUFFER_SIZE 9
static char azsphere_device_id[DEVICE_ID_BUFFER_SIZE];

size_t utility_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    size_t len = 0;
#ifdef AZURE_SPHERE
    Applications_OsVersion os_version;
#endif

    switch (port)
    {
        case 44: // Generate random number to seed mbasic randomize command
            len = (size_t)snprintf(buffer, buffer_length, "%d", ((rand() % 64000) - 32000));
            break;
        case 45: // generate a 16 bit random number and load low byte and high byte
        {
            u_int16_t num = (uint16_t)(rand() % 65536); // 0 to 65535 unsigned 16 bit number
            buffer[0]   = (char)(num & 0x00FF);        // low byte
            buffer[1]   = (char)((num & 0xFF00) >> 8); // high byte
            len         = 2;
            break;
        }
        case 70: // Load Altair version number
            len = (size_t)snprintf(buffer, buffer_length, "%s", ALTAIR_EMULATOR_VERSION);
            break;
#ifdef AZURE_SPHERE

        case 71: // OS Version
            Applications_GetOsVersion(&os_version);
            len = (size_t)snprintf(buffer, buffer_length, "%s", os_version.version);
            break;

        case 72: // Get Azure Sphere Device ID - note for security reasons only getting the first 8 chars
            memset(azsphere_device_id, 0x00, sizeof(azsphere_device_id));
            if (GetDeviceID((char *)&azsphere_device_id, sizeof(azsphere_device_id)) == 0)
            {
                len = (size_t)snprintf(buffer, buffer_length, "%s", azsphere_device_id);
            }
            else
            {
                // Failed to get device ID so return random ID
                len = (size_t)snprintf(buffer, buffer_length, "%s", "A7B43940");
            }
            break;

#endif // AZURE_SPHERE
    }

    return len;
}

uint8_t utility_input(uint8_t port)
{
    uint8_t retVal = 0;

    switch (port)
    {
        case 69: // get network ready state
            retVal = dx_isNetworkReady();
            break;
    }

    return retVal;
}