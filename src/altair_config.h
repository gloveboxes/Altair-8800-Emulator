/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_config.h"
#include "dx_utilities.h"
#include "FrontPanels/front_panel_manager.h"
#include <ctype.h>
#include <getopt.h>
#include <string.h>

	typedef struct
{
	DX_USER_CONFIG user_config;
	char *open_weather_map_api_key;
	char *openai_api_key;
	char *openai_endpoint;
	FRONT_PANEL_SELECTION front_panel_selection;
	bool stop_cpu_on_disconnect;
} ALTAIR_CONFIG_T;

bool parse_altair_cmd_line_arguments(int argc, char *argv[], ALTAIR_CONFIG_T *altairConfig);
