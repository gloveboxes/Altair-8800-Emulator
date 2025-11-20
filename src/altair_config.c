/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "altair_config.h"
#include "EdgeMqttDevX/include/dx_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// Usage text for command line arguments in application manifest.
static const char *cmdLineArgsUsageText =
	"Altair 8800 Emulator - Command Line Options:\n"
	"\n"
	"MQTT Configuration (optional):\n"
	"  -m, --MqttHost <host>          MQTT broker hostname (required for MQTT)\n"
	"  -p, --MqttPort <port>          MQTT broker port (default: 1883)\n"
	"  -c, --MqttClientId <client_id> MQTT client ID (default: AltairEmulator_<timestamp>)\n"
	"  -U, --MqttUsername <username>  MQTT username (default: none)\n"
	"  -P, --MqttPassword <password>  MQTT password (default: none)\n"
	"\n"
	"Network Configuration:\n"
	"  -n, --NetworkInterface <iface> Network interface to use\n"
	"\n"
	"Hardware Configuration:\n"
	"  -f, --FrontPanel <mode>        Front panel selection: sensehat, kit, none (default: none)\n"
	"\n"
	"CPU Behavior:\n"
	"  -s, --StopCpuOnDisconnect <bool> Stop CPU when client disconnects: true, false (default: false)\n"
	"\n"
	"External Services:\n"
	"  -o, --OpenWeatherMapKey <key>  OpenWeatherMap API key\n"
	"  -a, --OpenAIKey <key>          OpenAI API key\n"
	"  -e, --OpenAIEndpoint <url>     OpenAI-compatible API endpoint\n"
	"                                 (default: https://api.openai.com/v1/chat/completions)\n"
	"                                 Use http://localhost:1234/v1/chat/completions for LM Studio\n"
	"\n"
	"Help:\n"
	"  -h, --help                     Show this help message\n"
	"\n"
	"Example:\n"
	"  ./Altair_emulator -m mqtt.example.com -p 1883 -c MyAltair -U myuser -P mypass\n";

static FRONT_PANEL_SELECTION parse_front_panel_selection(const char *value, FRONT_PANEL_SELECTION default_selection)
{
	if (value == NULL)
	{
		return default_selection;
	}

	if (strcasecmp(value, "sensehat") == 0 || strcasecmp(value, "sense-hat") == 0 || strcasecmp(value, "sense_hat") == 0)
	{
		return FRONT_PANEL_SELECTION_SENSE_HAT;
	}
	if (strcasecmp(value, "kit") == 0 || strcasecmp(value, "front-panel-kit") == 0 || strcasecmp(value, "front_panel_kit") == 0)
	{
		return FRONT_PANEL_SELECTION_KIT;
	}
	if (strcasecmp(value, "none") == 0 || strcasecmp(value, "off") == 0)
	{
		return FRONT_PANEL_SELECTION_NONE;
	}

	return default_selection;
}

static const char *front_panel_selection_to_string(FRONT_PANEL_SELECTION selection)
{
	switch (selection)
	{
		case FRONT_PANEL_SELECTION_NONE:
			return "none";
		case FRONT_PANEL_SELECTION_SENSE_HAT:
			return "sensehat";
		case FRONT_PANEL_SELECTION_KIT:
			return "kit";
		default:
			return "unknown";
	}
}

bool parse_altair_cmd_line_arguments(int argc, char *argv[], ALTAIR_CONFIG_T *altair_config)
{
	bool result                                 = true;
	int option                                  = 0;
	static const struct option cmdLineOptions[] = {
		{.name = "MqttHost", .has_arg = required_argument, .flag = NULL, .val = 'm'},
		{.name = "MqttPort", .has_arg = required_argument, .flag = NULL, .val = 'p'},
		{.name = "MqttClientId", .has_arg = required_argument, .flag = NULL, .val = 'c'},
		{.name = "MqttUsername", .has_arg = required_argument, .flag = NULL, .val = 'U'},
		{.name = "MqttPassword", .has_arg = required_argument, .flag = NULL, .val = 'P'},
		{.name = "NetworkInterface", .has_arg = required_argument, .flag = NULL, .val = 'n'},
		{.name = "FrontPanel", .has_arg = required_argument, .flag = NULL, .val = 'f'},
		{.name = "StopCpuOnDisconnect", .has_arg = required_argument, .flag = NULL, .val = 's'},
		{.name = "OpenWeatherMapKey", .has_arg = required_argument, .flag = NULL, .val = 'o'},
		{.name = "OpenAIKey", .has_arg = required_argument, .flag = NULL, .val = 'a'},
		{.name = "OpenAIEndpoint", .has_arg = required_argument, .flag = NULL, .val = 'e'},
		{.name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'}};

	altair_config->user_config.connectionType = DX_CONNECTION_TYPE_MQTT;
	
	// Set default values - no default MQTT host, only connect when explicitly specified
	altair_config->user_config.mqtt_host = NULL;
	altair_config->user_config.mqtt_port = "1883";
	altair_config->user_config.mqtt_username = NULL;
	altair_config->user_config.mqtt_password = NULL;
	altair_config->front_panel_selection      = FRONT_PANEL_SELECTION_NONE;
	altair_config->openai_endpoint = "https://api.openai.com/v1/chat/completions";
	
	// Default behavior: keep CPU running on disconnect (can be overridden with command line option)
	altair_config->stop_cpu_on_disconnect = true;
	
	// Generate a unique client ID with timestamp to avoid conflicts
	static char unique_client_id[64];
	time_t current_time = time(NULL);
	snprintf(unique_client_id, sizeof(unique_client_id), "AltairEmulator_%ld", current_time);
	altair_config->user_config.mqtt_client_id = unique_client_id;

	// Loop over all of the options.
	bool front_panel_option_set = false;

	while ((option = getopt_long(argc, argv, "m:p:c:U:P:n:f:s:o:a:e:h", cmdLineOptions, NULL)) != -1)
	{
		// Check if arguments are missing. Every option requires an argument.
		if (optarg != NULL && optarg[0] == '-')
		{
			dx_Log_Debug("WARNING: Option %c requires an argument\n", option);
			continue;
		}
		switch (option)
		{
			case 'm':
				altair_config->user_config.mqtt_host = optarg;
				break;
			case 'p':
				altair_config->user_config.mqtt_port = optarg;
				break;
			case 'c':
				altair_config->user_config.mqtt_client_id = optarg;
				break;
			case 'U':
				altair_config->user_config.mqtt_username = optarg;
				break;
			case 'P':
				altair_config->user_config.mqtt_password = optarg;
				break;
		case 'n':
			altair_config->user_config.network_interface = optarg;
			break;
		case 'f':
			altair_config->front_panel_selection = parse_front_panel_selection(optarg, FRONT_PANEL_SELECTION_NONE);
			front_panel_option_set              = true;
			break;
		case 's':
			if (strcasecmp(optarg, "false") == 0 || strcmp(optarg, "0") == 0 || strcasecmp(optarg, "no") == 0)
			{
				altair_config->stop_cpu_on_disconnect = false;
			}
			else if (strcasecmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0 || strcasecmp(optarg, "yes") == 0)
			{
				altair_config->stop_cpu_on_disconnect = true;
			}
			break;
		case 'o':
			altair_config->open_weather_map_api_key = optarg;
			break;
			// ...existing code...
			case 'a':
				altair_config->openai_api_key = optarg;
				break;
			case 'e':
				altair_config->openai_endpoint = optarg;
				break;
			case 'h':
				printf("%s\n", cmdLineArgsUsageText);
				return false;
				break;
			default:
				// Unknown options are ignored.
				break;
		}
	}

	if (!front_panel_option_set)
	{
		const char *front_panel_env = getenv("ALTAIR_FRONT_PANEL");
		altair_config->front_panel_selection = parse_front_panel_selection(front_panel_env, altair_config->front_panel_selection);
	}

	switch (altair_config->user_config.connectionType)
	{
		case DX_CONNECTION_TYPE_NOT_DEFINED:
			result = false;
			break;
		case DX_CONNECTION_TYPE_MQTT:
			// MQTT connection is valid - defaults are set if not specified
			break;
	}

	if (!result)
	{
		printf("%s\n", cmdLineArgsUsageText);
	}
	else
	{
		// Log the final MQTT client ID that will be used
		if (altair_config->user_config.mqtt_host != NULL)
		{
			dx_Log_Debug("Using MQTT client ID: %s\n", altair_config->user_config.mqtt_client_id);
		}
		dx_Log_Debug("Front panel selection: %s\n", front_panel_selection_to_string(altair_config->front_panel_selection));
	}

	return result;
}
