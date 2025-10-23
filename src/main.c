/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include "main.h"
#include "app_exit_codes.h"
#include "dx_mqtt.h"
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

static pthread_mutex_t altair_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t altair_start_cond   = PTHREAD_COND_INITIALIZER;

// Atomic CPU state for high-performance read access
static _Atomic(CPU_OPERATING_MODE) atomic_cpu_operating_mode = CPU_STOPPED;

ALTAIR_CONFIG_T altair_config;
ENVIRONMENT_TELEMETRY environment;

intel8080_t cpu;
uint8_t memory[64 * 1024];

ALTAIR_COMMAND cmd_switches;
uint16_t bus_switches = 0x00;

const uint8_t reverse_lut[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

const char ALTAIR_EMULATOR_VERSION[] = "5.2.5";
enum PANEL_MODE_T panel_mode         = PANEL_BUS_MODE;
char msgBuffer[MSG_BUFFER_BYTES]     = {0};
const char *network_interface        = NULL;

static bool stop_cpu = false;
static char Log_Debug_Time_buffer[128];

DX_TIMER_BINDING tmr_ws_ping_pong = {.repeat = &(struct timespec){10, 0}, .name = "tmr_partial_message", .handler = ws_ping_pong_handler};

DX_TIMER_BINDING tmr_heart_beat          = {.repeat = &(struct timespec){30, 0}, .name = "tmr_heart_beat", .handler = heart_beat_handler};
DX_TIMER_BINDING tmr_report_memory_usage = {.repeat = &(struct timespec){20, 0}, .name = "tmr_report_memory_usage", .handler = report_memory_usage};
DX_TIMER_BINDING tmr_update_environment  = {.repeat = &(struct timespec){20, 0}, .name = "tmr_update_environment", .handler = update_environment_handler};

DX_ASYNC_BINDING async_expire_session  = {.name = "async_expire_session", .handler = async_expire_session_handler};
DX_ASYNC_BINDING async_publish_json    = {.name = "async_publish_json", .handler = async_publish_json_handler};
DX_ASYNC_BINDING async_publish_weather = {.name = "async_publish_weather", .handler = async_publish_weather_handler};

DX_ASYNC_BINDING *async_bindings[] = {
    &async_expire_session,
    &async_publish_json,
    &async_publish_weather,
};

DX_TIMER_BINDING *timer_bindings[] = {
    &tmr_update_environment,
    &tmr_ws_ping_pong,
};

// High-performance CPU operating mode getter for emulation loop
// Uses atomic load with relaxed ordering for maximum performance
inline CPU_OPERATING_MODE get_cpu_operating_mode_fast(void)
{
    return atomic_load_explicit(&atomic_cpu_operating_mode, memory_order_relaxed);
}

// Constants to replace magic numbers
#define ASCII_MASK_7BIT    0x7F
#define MEMORY_SIZE_64K    (64 * 1024)
#define ROM_LOADER_ADDRESS 0xFF00

// MQTT Configuration - will be initialized after command line parsing
DX_MQTT_CONFIG mqtt_config;

// Forward declarations

/// <summary>
/// Safely publish an MQTT message with automatic reconnection
/// </summary>
/// <param name="mqtt_msg">MQTT message to publish</param>
/// <param name="message_type">Description of the message type for logging (e.g., "heartbeat", "memory usage")</param>
/// <returns>true if published successfully, false otherwise</returns>
static bool safe_mqtt_publish(const DX_MQTT_MESSAGE *mqtt_msg, const char *message_type)
{
    if (!dx_isMqttConnected())
    {
        if (!dx_mqttConnect(&mqtt_config, NULL, NULL))
        {
            dx_Log_Debug("Failed to reconnect to MQTT broker: %s\n", dx_mqttGetLastError());
            return false;
        }
    }

    if (dx_isMqttConnected())
    {
        if (!dx_mqttPublish(mqtt_msg))
        {
            dx_Log_Debug("Failed to publish %s: %s\n", message_type, dx_mqttGetLastError());
            return false;
        }
        return true;
    }

    return false;
}

/// <summary>
/// Detect if running on Apple Silicon at runtime
/// </summary>
static bool is_apple_silicon(void)
{
#ifdef __APPLE__
    struct utsname systemInfo;
    if (uname(&systemInfo) == 0)
    {
        // Check for arm64 AND verify we're on macOS (not other ARM platforms)
        // On macOS, sysname will be "Darwin"
        return (strcmp(systemInfo.machine, "arm64") == 0) && (strcmp(systemInfo.sysname, "Darwin") == 0);
    }
#endif
    return false;
}

void set_cpu_operating_mode(CPU_OPERATING_MODE new_mode)
{
    atomic_store_explicit(&atomic_cpu_operating_mode, new_mode, memory_order_release);
}

CPU_OPERATING_MODE toggle_cpu_operating_mode(void)
{
    CPU_OPERATING_MODE current_mode = atomic_load_explicit(&atomic_cpu_operating_mode, memory_order_acquire);
    CPU_OPERATING_MODE new_mode     = (current_mode == CPU_RUNNING) ? CPU_STOPPED : CPU_RUNNING;
    atomic_store_explicit(&atomic_cpu_operating_mode, new_mode, memory_order_release);
    return new_mode;
}

/// <summary>
/// Get geo location and environment for geo location from Open Weather Map
/// Requires Open Weather Map free api key
/// </summary>
static DX_TIMER_HANDLER(update_environment_handler)
{
    // Hitch a ride on the report_memory_usage event. Only publishes once.
    if (dx_isNetworkConnected(network_interface))
    {
        update_geo_location(&environment);
        update_weather();
        publish_telemetry(&environment);
    }
}
DX_TIMER_HANDLER_END

/// <summary>
/// Reports memory usage to IoT Central and MQTT
/// </summary>
static DX_TIMER_HANDLER(report_memory_usage)
{
    char current_utc[64];
    struct rusage r_usage;

    dx_getCurrentUtc(current_utc, sizeof(current_utc));
    getrusage(RUSAGE_SELF, &r_usage);

    // macOS has ru_maxrss in bytes, Linux has it in kilobytes
#ifdef __APPLE__
    long memory_usage_kb = r_usage.ru_maxrss / 1024; // Convert bytes to KB
#else
    long memory_usage_kb = r_usage.ru_maxrss; // Already in KB on Linux
#endif

    if (dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 3, DX_JSON_STRING, "device", mqtt_config.client_id, DX_JSON_STRING, "timestamp", current_utc,
            DX_JSON_LONG, "memory_usage_kb", memory_usage_kb))
    {
        DX_MQTT_MESSAGE mqtt_msg = {.topic = "v1/devices/me/telemetry", .payload = msgBuffer, .payload_length = strlen(msgBuffer), .qos = 0, .retain = false};
        safe_mqtt_publish(&mqtt_msg, "memory usage");
    }
}
DX_TIMER_HANDLER_END

/// <summary>
/// Reports IoT Central Heartbeat UTC Date and Time to MQTT
/// </summary>
static DX_TIMER_HANDLER(heart_beat_handler)
{
    char current_utc[64];
    dx_getCurrentUtc(current_utc, sizeof(current_utc));

    if (dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 2, DX_JSON_STRING, "device", mqtt_config.client_id, DX_JSON_STRING, "heartbeat_utc", current_utc))
    {
        DX_MQTT_MESSAGE mqtt_msg = {.topic = "v1/devices/me/telemetry", .payload = msgBuffer, .payload_length = strlen(msgBuffer), .qos = 0, .retain = false};
        safe_mqtt_publish(&mqtt_msg, "heartbeat");
    }
}

DX_TIMER_HANDLER_END



/// <summary>
/// Client connected successfully
/// </summary>
static void client_connected_cb(void)
{
    print_console_banner();
    set_cpu_operating_mode(CPU_RUNNING);
}

/// <summary>
/// Validate filename for security and length constraints
/// </summary>
/// <param name="filename">Filename to validate</param>
/// <returns>true if valid, false otherwise</returns>
static char terminal_read(void)
{
    char retVal = dequeue_terminal_input_character();
    retVal &= ASCII_MASK_7BIT;
    return retVal;
}

static void terminal_write(uint8_t c)
{
    c &= ASCII_MASK_7BIT; // take first 7 bits (127 ascii chars) only and discard 8th bit.

    publish_message(&c, 1);
}

static inline uint8_t sense(void)
{
    return (uint8_t)(bus_switches >> 8);
}

/// <summary>
/// Print welcome banner
/// </summary>
void print_console_banner(void)
{
    static bool first           = true;
    const char altair_version[] = "\r\nAltair version: ";

    if (first)
    {
        first = false;

        for (int x = 0; x < strlen(altair_version); x++)
        {
            terminal_write(altair_version[x]);
        }

        for (int x = 0; x < strlen(ALTAIR_EMULATOR_VERSION); x++)
        {
            terminal_write(ALTAIR_EMULATOR_VERSION[x]);
        }
    }
}

/// <summary>
/// Updates the PI Sense HAT with Altair address bus, data bus, and CPU state
/// </summary>
static void *panel_refresh_thread(void *arg)
{
    dx_Log_Debug("Panel refresh thread started\n");

    // Set low priority like altair_thread to avoid interfering with main threads
    // Runtime detection: use QoS on Apple Silicon, nice() elsewhere
    if (is_apple_silicon())
    {
#ifdef __APPLE__
        // On Apple Silicon, use QoS to explicitly request efficiency cores
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
        dx_Log_Debug("Panel refresh thread: Set QoS class to background\n");
#endif
    }
    else
    {
        // On other platforms (Linux, Intel Mac, etc.), use nice value to lower priority
        nice(19);
        dx_Log_Debug("Panel refresh thread: Set nice priority to 19\n");
    }

    uint8_t last_status = 0;
    uint8_t last_data   = 0;
    uint16_t last_bus   = 0;

    while (true)
    {
        if (panel_mode == PANEL_BUS_MODE)
        {
            uint8_t status = cpu.cpuStatus;
            uint8_t data   = cpu.data_bus;
            uint16_t bus   = cpu.address_bus;

            // Only update panel if data has changed
            if (status != last_status || data != last_data || bus != last_bus)
            {
                last_status = status;
                last_data   = data;
                last_bus    = bus;

                status = (uint8_t)(reverse_lut[(status & 0xf0) >> 4] | reverse_lut[status & 0xf] << 4);
                data   = (uint8_t)(reverse_lut[(data & 0xf0) >> 4] | reverse_lut[data & 0xf] << 4);
                bus    = (uint16_t)(reverse_lut[(bus & 0xf000) >> 12] << 8 | reverse_lut[(bus & 0x0f00) >> 8] << 12 | reverse_lut[(bus & 0xf0) >> 4] |
                                 reverse_lut[bus & 0xf] << 4);

                front_panel_manager_io(status, data, bus, process_control_panel_commands);
            }

            nanosleep(&(struct timespec){0, 5 * ONE_MS}, NULL);
        }
        else
        {
            nanosleep(&(struct timespec){0, 500 * ONE_MS}, NULL);
        }
    }
    return NULL;
}

/// <summary>
/// Initialize a single disk drive
/// </summary>
/// <param name="disk_path">Path to the disk image file</param>
/// <param name="fp">Pointer to file descriptor to set</param>
/// <param name="disk_name">Name of the disk for logging</param>
/// <param name="exit_code">Exit code to use if initialization fails</param>
/// <returns>true on success, false on failure</returns>
static bool init_altair_disk(const char *disk_path, int *fp, const char *disk_name, APP_EXIT_CODE exit_code)
{
#ifdef ALTAIR_CLOUD
    if ((*fp = open(disk_path, O_RDONLY)) == -1)
    {
#else
    if ((*fp = open(disk_path, O_RDWR)) == -1)
    {
#endif
        Log_Debug("Failed to open %s disk image: %s\n", disk_name, strerror(errno));
        dx_terminate(exit_code);
        return false;
    }
    return true;
}

/// <summary>
/// Initialize the Altair disks and i8080 cpu
/// </summary>
static void init_altair(void)
{
    Log_Debug("Altair initialization starting...\n");

    memset(memory, 0x00, MEMORY_SIZE_64K); // clear Altair memory.
    Log_Debug("Memory cleared\n");

    disk_controller_t disk_controller;
    disk_controller.disk_function = disk_function;
    disk_controller.disk_select   = disk_select;
    disk_controller.disk_status   = disk_status;
    disk_controller.read          = disk_read;
    disk_controller.write         = disk_write;
    disk_controller.sector        = sector;

    Log_Debug("Disk controller initialized\n");

    // Initialize all disk drives
    Log_Debug("Opening DISK_A: %s\n", DISK_A);
    if (!init_altair_disk(DISK_A, &disk_drive.disk1.fp, "DISK_A", APP_EXIT_DISK_A_INIT_FAILED))
        return;
    disk_drive.disk1.diskPointer = 0;
    disk_drive.disk1.sector      = 0;
    disk_drive.disk1.track       = 0;
    Log_Debug("DISK_A opened successfully\n");

    Log_Debug("Opening DISK_B: %s\n", DISK_B);
    if (!init_altair_disk(DISK_B, &disk_drive.disk2.fp, "DISK_B", APP_EXIT_DISK_B_INIT_FAILED))
        return;
    disk_drive.disk2.diskPointer = 0;
    disk_drive.disk2.sector      = 0;
    disk_drive.disk2.track       = 0;
    Log_Debug("DISK_B opened successfully\n");

    Log_Debug("Opening DISK_C: %s\n", DISK_C);
    if (!init_altair_disk(DISK_C, &disk_drive.disk3.fp, "DISK_C", APP_EXIT_DISK_C_INIT_FAILED))
        return;
    disk_drive.disk3.diskPointer = 0;
    disk_drive.disk3.sector      = 0;
    disk_drive.disk3.track       = 0;
    Log_Debug("DISK_C opened successfully\n");

    Log_Debug("Opening DISK_D: %s\n", DISK_D);
    if (!init_altair_disk(DISK_D, &disk_drive.disk4.fp, "DISK_D", APP_EXIT_DISK_D_INIT_FAILED))
        return;
    disk_drive.disk4.diskPointer = 0;
    disk_drive.disk4.sector      = 0;
    disk_drive.disk4.track       = 0;
    Log_Debug("DISK_D opened successfully\n");

    Log_Debug("Initializing i8080 CPU\n");
    i8080_reset(
        &cpu, (port_in)terminal_read, (port_out)terminal_write, sense, &disk_controller, (azure_sphere_port_in)io_port_in, (azure_sphere_port_out)io_port_out);

    Log_Debug("Loading ROM image: %s\n", DISK_LOADER);
    // load Disk Loader at ROM_LOADER_ADDRESS
    if (!loadRomImage(DISK_LOADER, ROM_LOADER_ADDRESS))
    {
        Log_Debug("Failed to open %s disk load ROM image\n", DISK_LOADER);
        dx_terminate(APP_EXIT_ROM_LOAD_FAILED);
        return;
    }

    Log_Debug("Setting CPU to examine ROM_LOADER_ADDRESS\n");
    i8080_examine(&cpu, ROM_LOADER_ADDRESS); // ROM_LOADER_ADDRESS loads from disk, 0x0000 loads basic

    Log_Debug("Altair initialization completed successfully\n");
}

/// <summary>
/// Cleanup Altair disk file handles to prevent resource leaks
/// </summary>
static void cleanup_altair_disks(void)
{
    if (disk_drive.disk1.fp != -1)
    {
        close(disk_drive.disk1.fp);
        disk_drive.disk1.fp = -1;
    }
    if (disk_drive.disk2.fp != -1)
    {
        close(disk_drive.disk2.fp);
        disk_drive.disk2.fp = -1;
    }
    if (disk_drive.disk3.fp != -1)
    {
        close(disk_drive.disk3.fp);
        disk_drive.disk3.fp = -1;
    }
    if (disk_drive.disk4.fp != -1)
    {
        close(disk_drive.disk4.fp);
        disk_drive.disk4.fp = -1;
    }
}

/// <summary>

/// <summary>
/// Thread to run the i8080 cpu emulator on
/// </summary>
static void *altair_thread(void *arg)
{
    dx_Log_Debug("Altair thread: Starting\n");

    // Signal that thread is starting BEFORE changing priority
    // This prevents deadlocks on single-core systems
    dx_Log_Debug("Altair thread: Signaling main thread\n");
    pthread_mutex_lock(&altair_start_mutex);
    pthread_cond_broadcast(&altair_start_cond);
    pthread_mutex_unlock(&altair_start_mutex);

    dx_Log_Debug("Altair thread: Signal sent, setting thread priority\n");

    // Now set priority - this won't affect the signaling above
    // Runtime detection: use QoS on Apple Silicon, nice() elsewhere
    if (is_apple_silicon())
    {
#ifdef __APPLE__
        // On Apple Silicon, use QoS to explicitly request efficiency cores
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
        dx_Log_Debug("Altair thread: Set QoS class to background\n");
#endif
    }
    else
    {
        // On other platforms (Linux, Intel Mac, etc.), use nice value to lower priority
        nice(19);
        dx_Log_Debug("Altair thread: Set nice priority to 19\n");
    }

    dx_Log_Debug("Altair thread: Entering main CPU loop\n");
    while (!stop_cpu)
    {
        if (get_cpu_operating_mode_fast() == CPU_RUNNING)
        {
            i8080_cycle(&cpu);
        }
    }

    dx_Log_Debug("Altair thread: Exiting CPU loop\n");
    return NULL;
}

/// <summary>
/// Create the a thread with a low priority to encourage a thread to run efficiency cores on Apple Silicon
/// </summary>
/// <param name="daemon"></param>
/// <param name="arg"></param>
/// <param name="daemon_name"></param>
/// <param name="priority"></param>
bool start_altair_thread(void *(*daemon)(void *), void *arg, char *daemon_name, int priority)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Use default scheduling (SCHED_OTHER), no real-time priority
    if (pthread_create(&thread, &attr, daemon, arg))
    {
        printf("ERROR: Failed to start %s daemon.\n", daemon_name);
        return false;
    }
    return true;
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralAndHandlers(int argc, char *argv[])
{
    // https://stackoverflow.com/questions/18935446/program-received-signal-sigpipe-broken-pipe
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    curl_global_init(CURL_GLOBAL_ALL);

    setvbuf(stdout, NULL, _IONBF, 0); // disable stdout/printf buffering

    dx_Log_Debug_Init(Log_Debug_Time_buffer, sizeof(Log_Debug_Time_buffer));
    srand((unsigned int)time(NULL)); // seed the random number generator

    if (!parse_altair_cmd_line_arguments(argc, argv, &altair_config))
    {
        dx_terminate(0); // Exit cleanly if help was displayed or invalid arguments
        return;
    }

    network_interface = altair_config.user_config.network_interface;

    // Initialize MQTT configuration from command line arguments
    mqtt_config.hostname           = altair_config.user_config.mqtt_host;
    mqtt_config.port               = altair_config.user_config.mqtt_port;
    mqtt_config.client_id          = altair_config.user_config.mqtt_client_id;
    mqtt_config.username           = altair_config.user_config.mqtt_username;
    mqtt_config.password           = altair_config.user_config.mqtt_password;
    mqtt_config.keep_alive_seconds = 60;
    mqtt_config.clean_session      = true;

    init_environment(&altair_config);

    if (!dx_isStringNullOrEmpty(altair_config.openai_api_key))
    {
        init_openai(altair_config.openai_api_key);
    }

    dx_Log_Debug("Network interface %s %s\n", network_interface, dx_isNetworkConnected(network_interface) ? "connected" : "NOT connected");

    if (!front_panel_manager_init(altair_config.front_panel_selection))
    {
        dx_Log_Debug("Front panel initialization failed; running without hardware panel\n");
    }

    dx_Log_Debug("Active front panel: %s\n", front_panel_manager_get_active_name());
    dx_Log_Debug("Active front panel type: %d\n", front_panel_manager_get_active_type());

    dx_asyncSetInit(async_bindings, NELEMS(async_bindings));

    // Initialize high-precision monotonic millisecond timer system
    dx_Log_Debug("Initializing high-precision monotonic millisecond timer\n");
    dx_initMonotonicMillisecondTimer();

    // Initialize timers but don't start them yet
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));

    // Initialize MQTT connection only if hostname is specified
    if (!dx_isStringNullOrEmpty(mqtt_config.hostname) && dx_isNetworkConnected(network_interface))
    {
        dx_Log_Debug("Initializing MQTT connection to %s:%s\n", mqtt_config.hostname, mqtt_config.port);
        if (dx_mqttConnect(&mqtt_config, NULL, NULL))
        {
            dx_Log_Debug("Successfully connected to MQTT broker at %s\n", mqtt_config.hostname);
            dx_Log_Debug("Starting MQTT-related timers\n");
            dx_timerStart(&tmr_heart_beat);
            dx_timerStart(&tmr_report_memory_usage);
        }
        else
        {
            dx_Log_Debug("Failed to connect to MQTT broker: %s\n", dx_mqttGetLastError());
        }
    }
    else if (mqtt_config.hostname == NULL)
    {
        dx_Log_Debug("No MQTT broker specified - running without MQTT connectivity\n");
    }

    // Initialize Altair and start CPU thread BEFORE accepting WebSocket connections
    init_altair();

    // Lock mutex BEFORE starting thread to prevent race condition
    pthread_mutex_lock(&altair_start_mutex);

    dx_Log_Debug("Starting altair thread\n");
    start_altair_thread(altair_thread, NULL, "altair_thread", 1);

    // Wait for Altair thread to signal it's ready
    dx_Log_Debug("Waiting for altair thread to signal ready\n");
    pthread_cond_wait(&altair_start_cond, &altair_start_mutex);
    pthread_mutex_unlock(&altair_start_mutex);

    dx_Log_Debug("Altair thread confirmed running, starting WebSocket server\n");

    // NOW start the WebSocket server - Altair is fully initialized and ready
    init_web_socket_server(client_connected_cb);

    // Now start the panel refresh thread if a front panel is active
    if (front_panel_manager_get_active_type() != FRONT_PANEL_TYPE_NONE)
    {
        dx_Log_Debug("Starting panel refresh thread for front panel type %d\n", front_panel_manager_get_active_type());
        dx_startThreadDetached(panel_refresh_thread, NULL, "panel_refresh_thread");
    }
    else
    {
        dx_Log_Debug("No hardware front panel active, skipping panel refresh thread\n");
    }
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralAndHandlers(void)
{
    // Disconnect from MQTT broker if connected
    if (dx_isMqttConnected())
    {
        dx_mqttDisconnect();
        dx_Log_Debug("Disconnected from MQTT broker\n");
        dx_Log_Debug("Stopping MQTT-related timers\n");
        dx_timerStop(&tmr_heart_beat);
        dx_timerStop(&tmr_report_memory_usage);
    }

    dx_timerEventLoopStop();

    cleanup_altair_disks();

    front_panel_manager_shutdown();

    curl_global_cleanup();
}

int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();
    InitPeripheralAndHandlers(argc, argv);

    dx_eventLoopRun();

    ClosePeripheralAndHandlers();

    dx_Log_Debug("APPLICATION EXITED WITH EXIT CODE: %d\n\n", dx_getTerminationExitCode());
    return dx_getTerminationExitCode();
}
