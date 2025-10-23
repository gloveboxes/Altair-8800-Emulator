/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "time_io.h"

#include "dx_timer.h"    // For dx_getElapsedMilliseconds()
#include "dx_utilities.h"
#include "main.h" // For get_second_tick_count()
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Define timer indices for array access
#define TIMER_0       0 // Ports 24/25
#define TIMER_1       1 // Ports 26/27
#define TIMER_2       2 // Ports 28/29
#define NUM_MS_TIMERS 3

// Array-based millisecond timers for improved scalability
static uint64_t ms_timer_targets[NUM_MS_TIMERS]    = {0, 0, 0};
static unsigned int ms_timer_delays[NUM_MS_TIMERS] = {0, 0, 0};

// Thread-safe seconds timer using permanent timer approach
static uint64_t seconds_timer_target = 0;

/// <summary>
/// Get timer index based on port number
/// </summary>
/// <param name="port">Port number</param>
/// <returns>Timer index or -1 if invalid port</returns>
static int get_timer_index(int port)
{
    switch (port)
    {
        case 24:
        case 25:
            return TIMER_0;
        case 26:
        case 27:
            return TIMER_1;
        case 28:
        case 29:
            return TIMER_2;
        default:
            return -1;
    }
}

size_t time_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    size_t len    = 0;
    int timer_idx = get_timer_index(port);

    switch (port)
    {
        case 24: // Timer 0 - Set milliseconds timer high byte (bits 15-8)
        case 26: // Timer 1 - Set milliseconds timer high byte (bits 15-8)
        case 28: // Timer 2 - Set milliseconds timer high byte (bits 15-8)
            if (timer_idx >= 0 && timer_idx < NUM_MS_TIMERS)
            {
                ms_timer_delays[timer_idx] = (ms_timer_delays[timer_idx] & 0x00FF) | ((uint16_t)data << 8);
            }
            break;
        case 25: // Timer 0 - Set milliseconds timer low byte (bits 7-0) and start timer
        case 27: // Timer 1 - Set milliseconds timer low byte (bits 7-0) and start timer
        case 29: // Timer 2 - Set milliseconds timer low byte (bits 7-0) and start timer
            if (timer_idx >= 0 && timer_idx < NUM_MS_TIMERS)
            {
                ms_timer_delays[timer_idx] = (ms_timer_delays[timer_idx] & 0xFF00) | data;

                // Calculate target time using high-precision monotonic timer
                uint64_t current_time       = dx_getElapsedMilliseconds();
                ms_timer_targets[timer_idx] = current_time + ms_timer_delays[timer_idx];
            }
            break;
        case 30: // set seconds timer
            seconds_timer_target = dx_getElapsedMilliseconds() / 1000 + data;
            break;
        case 41: // System tick count
            len = (size_t)snprintf(buffer, buffer_length, "%u", (uint32_t)(dx_getElapsedMilliseconds() / 1000));
            break;
        case 42: // get utc date and time
            dx_getCurrentUtc(buffer, buffer_length);
            len = strnlen(buffer, buffer_length);
            break;
        case 43: // get local date and time
#ifdef AZURE_SPHERE
            dx_getCurrentUtc(buffer, buffer_length);
#else
            dx_getLocalTime(buffer, buffer_length);
#endif
            len = strnlen(buffer, buffer_length);
            break;
    }

    return len;
}

uint8_t time_input(uint8_t port)
{
    uint8_t retVal = 0;
    int timer_idx  = get_timer_index(port);

    switch (port)
    {
        case 24: // Timer 0 - Has milliseconds timer expired (same as case 25)
        case 25: // Timer 0 - Has milliseconds timer expired
        case 26: // Timer 1 - Has milliseconds timer expired (same as case 27)
        case 27: // Timer 1 - Has milliseconds timer expired
        case 28: // Timer 2 - Has milliseconds timer expired (same as case 29)
        case 29: // Timer 2 - Has milliseconds timer expired
        {
            if (timer_idx >= 0 && timer_idx < NUM_MS_TIMERS)
            {
                uint64_t target_time = ms_timer_targets[timer_idx];

                // Check if timer is active (target > 0) and has expired
                if (target_time > 0 && dx_getElapsedMilliseconds() >= target_time)
                {
                    // Timer has expired, clear it
                    ms_timer_targets[timer_idx] = 0;
                    ms_timer_delays[timer_idx]  = 0;
                    retVal                      = 0; // Timer expired (returns 0)
                }
                else if (target_time > 0)
                {
                    retVal = 1; // Timer still running (returns 1)
                }
                else
                {
                    retVal = 0; // Timer not active (returns 0)
                }
            }
        }
        break;
        case 30: // Has seconds timer expired
        {
            uint64_t target_time = seconds_timer_target;

            // Check if timer is active (target > 0) and has expired
            if (target_time > 0 && dx_getElapsedMilliseconds() / 1000 >= target_time)
            {
                // Timer has expired, clear it
                seconds_timer_target = 0;
                retVal               = 0; // Timer expired (returns 0)
            }
            else if (target_time > 0)
            {
                retVal = 1; // Timer still running (returns 1)
            }
            else
            {
                retVal = 0; // Timer not active (returns 0)
            }
        }
        break;
    }
    return retVal;
}