/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#ifndef _SPHERE_PANEL_H_
#define _SPHERE_PANEL_H_

#include "types.h"

#define MSG_BUFFER_BYTES 128 * 4 // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define OneMS            1000000 // used to simplify timer defn.

// #define LOW  false
// #define HIGH true

//#define SWITCHES_LOAD   42
//#define SWITCHES_CS     41
//#define LEDSTORE        29
//#define OE              26
//#define MR              27
//#define MEMORY_CS		30
//#define SD_CS			28

typedef enum
{
	CPU_RUNNING = 1,
	CPU_STOPPED = 2,
	CPU_LOW_POWER = 3
} CPU_OPERATING_MODE;

typedef enum
{
	CONTROL_MODE,
	INPUT_MODE
} CLICK_4X4_BUTTON_MODE;

typedef enum
{
	NOP               = 0x00,
	RUN_CMD           = 0x01,
	STOP_CMD          = 0x02,
	SINGLE_STEP       = 0x08,
	EXAMINE           = 0x20,
	EXAMINE_NEXT      = 0x10,
	DEPOSIT           = 0x80,
	DEPOSIT_NEXT      = 0x40,
	DISASSEMBLE       = 0x100,
	TRACE             = 0x101,
	RESET             = 0x102,
	LOAD_ALTAIR_BASIC = 0x103
} ALTAIR_COMMAND;

#endif
