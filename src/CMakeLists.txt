#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required (VERSION 3.10)
project (Altair_emulator C)

# Enable generation of compile_commands.json for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set optimization flags for different build types
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DALTAIR_DEBUG -Wno-macro-redefined")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")

# Suppress CMake policy warnings from submodules
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()

# SELECT FRONT PANEL CONFIG #######################################################################################
#
# Front panel selection is now done at runtime via command line argument (-f or --FrontPanel)
# All front panel drivers are compiled into the binary for runtime selection
# Options: auto (default), sensehat, kit, none
#
# Example: ./Altair_emulator -f sensehat
#          ./Altair_emulator -f kit
#          ./Altair_emulator -f none
#
###################################################################################################################

###################################################################################################################
#
# set(ALTAIR_CLOUD TRUE "Enable Altair as a service real-time in-memory differencing disk")
###################################################################################################################

if (ALTAIR_CLOUD)
    add_compile_definitions(ALTAIR_CLOUD)
endif(ALTAIR_CLOUD)

# overide wsServer MAX_CLIENTS
add_compile_definitions(MAX_CLIENTS=1)

# set(DISABLE_IO_MOCKING ON CACHE  BOOL "DISABLE IO mocking for AzureSphereRemoteX.Client" FORCE )

# ENABLE_EDGE_DEVX_REMOTEX is enabled as Log_Debug found in appslib is required.
# set(ENABLE_EDGE_DEVX_REMOTEX ON CACHE  BOOL "DISABLE EdgeDevX RemoteX peripheral support" FORCE )

#if(DISABLE_IO_MOCKING)
#    add_compile_definitions(DISABLE_IO_MOCKING=TRUE)
#endif(DISABLE_IO_MOCKING)

message(STATUS "ALTAIR PROJECT CONFIGURATION: All front panel drivers included for runtime selection")

# Include all front panel drivers for runtime selection
# Note: Linux-specific drivers (Sense HAT and Kit) are only included on Linux platforms
set(FrontPanels
    "FrontPanels/front_panel_manager.c"
    "FrontPanels/front_panel_none.c"
)

# Add platform-specific front panel drivers
if(UNIX AND NOT APPLE)
    # Linux-specific drivers that require linux headers
    list(APPEND FrontPanels
        "FrontPanels/front_panel_pi_sense_hat.c"
        "FrontPanels/front_panel_kit.c"
        "Drivers/pi_sense_hat/led_panel.c"
        "Drivers/pi_sense_hat/graphics.c"
        "Drivers/sensehat-driver/src/sense_hat.c"
        "Drivers/spidev-lib/src/spidev_lib.c"
    )
    message(STATUS "Including Linux-specific front panel drivers (Sense HAT and Kit)")
else()
    message(STATUS "Skipping Linux-specific front panel drivers on non-Linux platform")
endif()

source_group("FrontPanels" FILES ${FrontPanels})


set(Source
    "Altair8800/88dcdd.c"
    "Altair8800/intel8080.c"
    "Altair8800/memory.c"
    "altair_config.c"
    "io_ports.c"
    "cpu_monitor.c"
    "difference_disk.c"
    "mqtt_manager.c"
    "web_console.c"
    "main.c"
    "environment.c"
    # "env_air_visual.c"
    "env_open_weather_map.c"
    "location_from_ip.c"
    "utils.c"  
    "PortDrivers/azure_io.c"
    "PortDrivers/led_matrix_io.c"
    "PortDrivers/weather_io.c"
    "PortDrivers/time_io.c"
    "PortDrivers/file_io.c"
    "PortDrivers/utility_io.c"
    "PortDrivers/onboard_io.c"
    "PortDrivers/openai.c"
    "PortDrivers/power_io.c"
    )
source_group("Source" FILES ${Source})

set(wsServer
    wsServer/src/ws.c
    wsServer/src/base64.c
    wsServer/src/sha1.c
    wsServer/src/handshake.c
    wsServer/src/utf8.c
)
source_group("wsServer" FILES ${wsServer})

set(ALL_FILES
    ${Source}
    ${FrontPanels}
    ${wsServer}
)

# Create executable
add_executable(${PROJECT_NAME} ${ALL_FILES})

# Configure MQTT if hostname is specified
if (ALTAIR_MQTT_BROKER_HOSTNAME)
    target_compile_options(${PROJECT_NAME} PRIVATE "-DMQTT_BROKER_HOSTNAME=\"${ALTAIR_MQTT_BROKER_HOSTNAME}\"")
endif(ALTAIR_MQTT_BROKER_HOSTNAME)

################################################################################
# Following lines required for EdgeMqttDevX
# Suppress warnings from submodules
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
add_subdirectory("EdgeMqttDevX" edge_mqtt_devx)
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
target_link_libraries(${PROJECT_NAME} "edge_mqtt_devx")
################################################################################

# target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
target_link_libraries(${PROJECT_NAME} c curl)

# Check if atomic library is needed (required on some ARM architectures for 64-bit atomics)
include(CheckCSourceCompiles)
check_c_source_compiles("
    #include <stdatomic.h>
    #include <stdint.h>
    int main() {
        atomic_uint_fast64_t test = 0;
        atomic_load(&test);
        atomic_store(&test, 1);
        atomic_fetch_add(&test, 1);
        return 0;
    }
" ATOMIC_OPERATIONS_BUILTIN)

if(NOT ATOMIC_OPERATIONS_BUILTIN)
    target_link_libraries(${PROJECT_NAME} atomic)
    message(STATUS "Linking with libatomic for atomic operations support")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC /usr/local/include)
target_link_options(${PROJECT_NAME} PUBLIC "-L/usr/local/lib")
target_include_directories(${PROJECT_NAME} PUBLIC AzureSphereDevX/include )
target_include_directories(${PROJECT_NAME} PRIVATE wsServer/include)
target_include_directories(${PROJECT_NAME} PUBLIC Drivers/pi_sense_hat)
target_include_directories(${PROJECT_NAME} PUBLIC Drivers/front_panel)
target_include_directories(${PROJECT_NAME} PUBLIC Drivers/spidev-lib/src)
target_include_directories(${PROJECT_NAME} PUBLIC Drivers/sensehat-driver/src)
target_include_directories(${PROJECT_NAME} PUBLIC uthash/src)
target_include_directories(${PROJECT_NAME} PUBLIC PortDrivers)


include_directories(${CMAKE_SOURCE_DIR} FrontPanels)
include_directories(${CMAKE_SOURCE_DIR} Altair8800)


target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unknown-pragmas)

set_source_files_properties(Altair8800/intel8080.c PROPERTIES COMPILE_FLAGS -Wno-conversion)
set_source_files_properties(Altair8800/intel8080.h PROPERTIES COMPILE_FLAGS -Wno-conversion)
set_source_files_properties(Altair8800/memory.c PROPERTIES COMPILE_FLAGS -Wno-conversion)
set_source_files_properties(FrontPanels/front_panel_virtual.c PROPERTIES COMPILE_FLAGS -Wno-conversion)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# Add custom clean-build target that removes build artifacts before building
add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config ${CMAKE_BUILD_TYPE} --target all -j 8
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cleaning and rebuilding project..."
    USES_TERMINAL
)
