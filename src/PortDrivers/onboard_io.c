/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "onboard_io.h"
#include "dx_utilities.h"
#include "front_panel_manager.h"

// Include sense_hat header only on Linux where the hardware is available
#if defined(__linux__)
#include "sense_hat.h"
#endif

#ifdef AZURE_SPHERE
#include "device_id.h"
#include "onboard_sensors.h"
#include <applibs/applications.h>
#include <applibs/storage.h>
#endif

#ifdef OEM_AVNET
#include "light_sensor.h"

static float x, y, z;
static bool accelerometer_running = false;
static char PREDICTION[20]        = {'n', 'o', 'r', 'm', 'a', 'l'};

#endif // OEM_AVNET

/// <summary>
/// Callback handler for Asynchronous Inter-Core Messaging Pattern
/// </summary>
void intercore_classify_response_handler(void *data_block, ssize_t message_length)
{
#ifdef OEM_AVNET

    INTERCORE_PREDICTION_BLOCK_T *ic_message_block = (INTERCORE_PREDICTION_BLOCK_T *)data_block;

    switch (ic_message_block->cmd)
    {
        case IC_PREDICTION:
            // Log_Debug("Prediction %s\n", ic_message_block->PREDICTION);
            memcpy(PREDICTION, ic_message_block->PREDICTION, sizeof(PREDICTION));
            break;
        default:
            break;
    }

#endif // OEM_AVNET
}

DX_TIMER_HANDLER(read_accelerometer_handler)
{
#ifdef OEM_AVNET
    float xx, yy, zz;

    avnet_get_acceleration(&xx, &yy, &zz);

    // Log_Debug("now x %f, y %f, z %f\n", xx, yy, zz);

    x += xx;
    y += yy;
    z += zz;
    x /= 2;
    y /= 2;
    z /= 2;

    intercore_ml_classify_block.x = x;
    intercore_ml_classify_block.y = y;
    intercore_ml_classify_block.z = z;

    dx_intercorePublish(&intercore_ml_classify_ctx, &intercore_ml_classify_block, sizeof(intercore_ml_classify_block));

    // dx_Log_Debug("avg x %f, y %f, z %f\n", x, y, z);

    dx_timerOneShotSet(&tmr_read_accelerometer, &(struct timespec){0, 10 * ONE_MS});
#endif // OEM_AVNET
}
DX_TIMER_HANDLER_END

DX_ASYNC_HANDLER(async_accelerometer_start_handler, handle)
{
#ifdef OEM_AVNET
    avnet_get_temperature_lps22h(); // This is a hack to initialize the accelerometer :)
    dx_timerStart(&tmr_read_accelerometer);
    dx_timerOneShotSet(&tmr_read_accelerometer, &(struct timespec){0, 10 * ONE_MS});
#endif // OEM_AVNET
}
DX_ASYNC_HANDLER_END

DX_ASYNC_HANDLER(async_accelerometer_stop_handler, handle)
{
#ifdef OEM_AVNET
    dx_timerStop(&tmr_read_accelerometer);
#endif // OEM_AVNET
}
DX_ASYNC_HANDLER_END

size_t onboard_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    size_t len = 0;

    switch (port)
    {

#ifdef AZURE_SPHERE
        case 62: // Onboard RGB LED
            switch (data)
            {
                case 0: // Red LED
                    dx_gpioStateSet(&gpioRed, (bool)data);
                    break;
                case 1: // Green LED
                    dx_gpioStateSet(&gpioGreen, (bool)data);
                    break;
                case 2: // Blue LED
                    dx_gpioStateSet(&gpioBlue, (bool)data);
                    break;
                default:
                    break;
            }
            break;
        case 63: // Onboard sensors temperature, pressure, and light
            switch (data)
            {
                case 0:
                    // Temperature minus 9 is super rough calibration
                    len = (size_t)snprintf(buffer, buffer_length, "%d", (int)onboard_get_temperature() - 9);
                    break;
                case 1:
                    len = (size_t)snprintf(buffer, buffer_length, "%d", (int)onboard_get_pressure());
                    break;
                case 2:
#if defined(OEM_AVNET) && !defined(ALTAIR_FRONT_PANEL_KIT)
                    len = (size_t)snprintf(buffer, buffer_length, "%d", avnet_get_light_level() * 2);
#else
                    len = (size_t)snprintf(buffer, buffer_length, "%d", 0);
#endif // OEM_AVNET
                    break;
            }

            break;
#endif // AZURE_SPHERE

        case 63: // Onboard sensors temperature, pressure, and light - runtime check for Sense HAT
#if defined(__linux__)
            if (front_panel_manager_is_active(FRONT_PANEL_TYPE_SENSE_HAT))
            {
                switch (data)
                {
                    case 0: // Temperature minus 1 for very rough calibration
                        len = (size_t)snprintf(buffer, buffer_length, "%d", (int)get_temperature_from_lps25h() - 1);
                        break;
                    case 1: // pressure
                        len = (size_t)snprintf(buffer, buffer_length, "%d", get_pressure());
                        break;
                    case 2: // light
                        len = (size_t)snprintf(buffer, buffer_length, "%d", 0);
                        break;
                    case 3: // humidity
                        len = (size_t)snprintf(buffer, buffer_length, "%d", (int)get_humidity());
                        break;
                }
            }
#endif
            break;

#ifdef OEM_AVNET
        case 64: // Accelerometer data and settings
            switch (data)
            {
                case 0: // accelerometer X
                    len = (size_t)snprintf(buffer, buffer_length, "%f", x);
                    break;
                case 1: // accelerometer Y
                    len = (size_t)snprintf(buffer, buffer_length, "%f", y);
                    break;
                case 2: // accelerometer Z
                    len = (size_t)snprintf(buffer, buffer_length, "%f", z);
                    break;
                case 3: // accelerometer start feeding TinyML model running on real-time core
                    if (!accelerometer_running)
                    {
                        dx_asyncSend(&async_accelerometer_start, NULL);
                        accelerometer_running = true;
                        nanosleep(&(struct timespec){1, 0}, NULL); // sleep for 1 second to allow accelerometer to start
                    }
                    break;
                case 4: // accelerometer stop feeding TinyML model running on real-time core
                    if (accelerometer_running)
                    {
                        dx_asyncSend(&async_accelerometer_stop, NULL);
                        accelerometer_running = false;
                        nanosleep(&(struct timespec){1, 0}, NULL); // sleep for 1 second to allow accelerometer to stop
                    }
                    break;
                case 5: // Load accelerometer x,y,z
                    if (!accelerometer_running)
                    {
                        avnet_get_acceleration(&x, &y, &z);
                    }
                    break;
                case 6: // Calibrate accelerometer for angular rate
                    if (!accelerometer_running)
                    {
                        avnet_calibrate_angular_rate();
                    }
                    break;
                case 7: // Load accelerometer for angular rate
                    if (!accelerometer_running)
                    {
                        avnet_get_angular_rate(&x, &y, &z);
                    }
                    break;
                case 8: // Get Tiny ML latest movement inference result
                    len = (size_t)snprintf(buffer, buffer_length, "%s", PREDICTION);
                    break;
            }
            break;
#endif // OEM_AVNET
    }

    return len;
}

uint8_t onboard_input(uint8_t port)
{
    uint8_t retVal = 0;

    return retVal;
}