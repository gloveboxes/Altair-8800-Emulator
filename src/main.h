// Declaration for nice() to avoid implicit declaration warning in main.c
#ifdef __cplusplus
extern "C"
{
#endif
    extern int nice(int);
#ifdef __cplusplus
}
#endif
#pragma once

// DevX Libraries
#include "dx_async.h"
#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_version.h"

// System Libraries
#include "applibs_versions.h"
#include <applibs/log.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

// Altair app
#include "altair_config.h"
#include "altair_panel.h"
#include "cpu_monitor.h"
#include "mqtt_manager.h"
#include "utils.h"
#include "web_console.h"
#include <curl/curl.h>
#include <curl/easy.h>

// Global MQTT configuration
extern DX_MQTT_CONFIG mqtt_config;

// Intel 8080 emulator
#include "88dcdd.h"
#include "intel8080.h"
#include "io_ports.h"
#include "memory.h"

extern const char ALTAIR_EMULATOR_VERSION[];
#define Log_Debug(f_, ...) dx_Log_Debug((f_), ##__VA_ARGS__)
#define DX_LOGGING_ENABLED FALSE

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:climatemonitor;1"

#define APP_SAMPLES_DIRECTORY "AppSamples"

extern enum PANEL_MODE_T panel_mode;
extern char msgBuffer[MSG_BUFFER_BYTES];
extern const char *network_interface;

extern ALTAIR_CONFIG_T altair_config;
extern ENVIRONMENT_TELEMETRY environment;

extern intel8080_t cpu;
extern uint8_t memory[64 * 1024]; // Altair system memory.

extern ALTAIR_COMMAND cmd_switches;
extern uint16_t bus_switches;

static DX_DECLARE_TIMER_HANDLER(heart_beat_handler);

static DX_DECLARE_TIMER_HANDLER(report_memory_usage);
static DX_DECLARE_TIMER_HANDLER(update_environment_handler);
static void *altair_thread(void *arg);

extern const uint8_t reverse_lut[16];

// clang-format off
// Common Timers

extern DX_TIMER_BINDING tmr_timer_millisecond_expired;
extern DX_TIMER_BINDING tmr_ws_ping_pong;

extern DX_TIMER_BINDING tmr_heart_beat;
extern DX_TIMER_BINDING tmr_millisecond_tick;
extern DX_TIMER_BINDING tmr_report_memory_usage;
extern DX_TIMER_BINDING tmr_update_environment;

extern DX_ASYNC_BINDING async_expire_session;
extern DX_ASYNC_BINDING async_publish_json;
extern DX_ASYNC_BINDING async_publish_weather;

// clang-format on

extern DX_ASYNC_BINDING *async_bindings[];

// initialize bindings
extern DX_TIMER_BINDING *timer_bindings[];

// High-performance millisecond tick count getter (inline for maximum performance)
// Uses direct uv_hrtime() calculation - no atomics needed
static inline uint64_t get_millisecond_tick_count(void)
{
    return dx_getElapsedMilliseconds();
}

// High-performance seconds tick count getter (inline for maximum performance)  
// Derives seconds from milliseconds for efficiency (avoids second timer)
static inline uint64_t get_second_tick_count(void)
{
    return dx_getElapsedMilliseconds() / 1000;
}
