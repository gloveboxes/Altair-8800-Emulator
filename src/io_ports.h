/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "PortDrivers/files_io.h"
#include "PortDrivers/http_io.h"
#include "PortDrivers/led_matrix_io.h"
#include "PortDrivers/onboard_io.h"
#include "PortDrivers/openai.h"
#include "PortDrivers/power_io.h"
#include "PortDrivers/time_io.h"
#include "PortDrivers/utility_io.h"
#include "PortDrivers/weather_io.h"

uint8_t io_port_in(uint8_t port);
void io_port_out(uint8_t port, uint8_t data);
