/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "altair_panel.h"
#include "dx_json_serializer.h"
#include "dx_mqtt.h"
#include "dx_utilities.h"
#include "environment.h"
#include "web_console.h"

extern char msgBuffer[MSG_BUFFER_BYTES];

void publish_telemetry(ENVIRONMENT_TELEMETRY *environment);
void update_geo_location(ENVIRONMENT_TELEMETRY *environment);