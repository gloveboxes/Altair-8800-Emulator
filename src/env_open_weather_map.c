/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "env_open_weather_map.h"

static DX_DECLARE_TIMER_HANDLER(weather_cache_expired);

static const char *weatherURLTemplate =
	"https://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon=%.6f&appid=%s&units=metric";
static char weatherUrl[200];

static const char *pollutionURLTemplate =
	"https://api.openweathermap.org/data/2.5/air_pollution?lat=%.6f&lon=%.6f&appid=%s";
static char pollutionUrl[200];

static bool owm_initialized = false;

static DX_TIMER_BINDING tmr_weather_cache_expired = {
	.name = "tmr_weather_cache_expired", .handler = weather_cache_expired};
static const int weather_cache_minutes =
	15 * 60; // 1 * 60 seconds = 15 minutes. OWM updates data every 10 minutes
static bool owm_cached = false;

static void generate_fake_telemetry(ENVIRONMENT_TELEMETRY *telemetry)
{
	// static location_info greenwich = {.lat = 51.477928, .lng = -0.001545};
	// So create some random weather data with geo location on Greenwich
	int rnd = (rand() % 10) - 5;

	telemetry->latest.weather.temperature = 25 + rnd;

	rnd                                      = (rand() % 50) - 25;
	telemetry->latest.weather.pressure       = 1000 + rnd;
	telemetry->latest.weather.humidity       = 20 + (rand() % 60);
	telemetry->latest.weather.wind_speed     = 0.0;
	telemetry->latest.weather.wind_direction = 0;
	snprintf(telemetry->latest.weather.description, 80, "%s", "OWM not available. Random data generated");
	telemetry->latest.weather.updated        = true;
}

static DX_TIMER_HANDLER(weather_cache_expired)
{
	owm_cached = false;
}
DX_TIMER_HANDLER_END

static void update_owm_weather(ENVIRONMENT_TELEMETRY *telemetry)
{
	JSON_Array *weatherArray    = NULL;
	JSON_Object *mainProperties = NULL;
	JSON_Object *weather        = NULL;
	JSON_Object *windProperties = NULL;
	JSON_Value *rootProperties  = NULL;
	double temp                 = 0.0;

	if (!owm_initialized)
	{
		generate_fake_telemetry(telemetry);
		return;
	}

	char *data = dx_getHttpData(weatherUrl, 6);

	if (data == NULL)
	{
		// do we don't have weather data cached from before then generate fake
		if (!telemetry->latest.weather.updated)
		{
			generate_fake_telemetry(telemetry);
		}
	}
	else
	{

		/*
		{
		  "coord": {
			"lon": -122.08,
			"lat": 37.39
		  },
		  "weather": [
			{
			  "id": 800,
			  "main": "Clear",
			  "description": "clear sky",
			  "icon": "01d"
			}
		  ],
		  "base": "stations",
		  "main": {
			"temp": 282.55,
			"feels_like": 281.86,
			"temp_min": 280.37,
			"temp_max": 284.26,
			"pressure": 1023,
			"humidity": 100
		  },
		  "visibility": 16093,
		  "wind": {
			"speed": 1.5,
			"deg": 350
		  },
		  "clouds": {
			"all": 1
		  },
		  "dt": 1560350645,
		  "sys": {
			"type": 1,
			"id": 5122,
			"message": 0.0139,
			"country": "US",
			"sunrise": 1560343627,
			"sunset": 1560396563
		  },
		  "timezone": -25200,
		  "id": 420006353,
		  "name": "Mountain View",
		  "cod": 200
		  }
		*/

		rootProperties = json_parse_string(data);
		if (rootProperties == NULL)
		{
			goto cleanup;
		}

		JSON_Object *rootObject = json_value_get_object(rootProperties);
		if (rootObject == NULL)
		{
			goto cleanup;
		}

		// get main weather details - temperature etc

		if ((mainProperties = json_object_dotget_object(rootObject, "main")) != NULL)
		{
			if (json_object_has_value_of_type(mainProperties, "temp", JSONNumber))
			{
				telemetry->latest.weather.temperature =
					FLOAT_TO_INT(json_object_get_number(mainProperties, "temp"));
			}

			if (json_object_has_value_of_type(mainProperties, "pressure", JSONNumber))
			{
				telemetry->latest.weather.pressure =
					FLOAT_TO_INT(json_object_get_number(mainProperties, "pressure"));
			}

			if (json_object_has_value_of_type(mainProperties, "humidity", JSONNumber))
			{
				telemetry->latest.weather.humidity =
					FLOAT_TO_INT(json_object_get_number(mainProperties, "humidity"));
			}
		}

		// get wind

		if ((windProperties = json_object_dotget_object(rootObject, "wind")) != NULL)
		{
			if (json_object_has_value_of_type(windProperties, "speed", JSONNumber))
			{
				telemetry->latest.weather.wind_speed = json_object_get_number(windProperties, "speed");
			}

			if (json_object_has_value_of_type(windProperties, "deg", JSONNumber))
			{
				telemetry->latest.weather.wind_direction =
					FLOAT_TO_INT(json_object_get_number(windProperties, "deg"));
			}
		}

		// Get weather description

		if ((weatherArray = json_object_dotget_array(rootObject, "weather")) != NULL)
		{
			size_t numWeatherItems = json_array_get_count(weatherArray); // needs to be more than 0.

			if (numWeatherItems > 0)
			{
				// get the first item in the array
				weather = json_array_get_object(weatherArray, 0);

				// if we have a description...
				if (weather != NULL && json_object_has_value_of_type(weather, "description", JSONString))
				{
					const char *description = json_object_get_string(weather, "description");
					snprintf(telemetry->latest.weather.description, 80, "%s %d%c", description,
						telemetry->latest.weather.temperature, 'C');
				}
			}
		}
		telemetry->latest.weather.updated = true;
	}

cleanup:

	if (rootProperties != NULL)
	{
		json_value_free(rootProperties);
		rootProperties = NULL;
	}

	if (data != NULL)
	{
		free(data);
		data = NULL;
	}
}

float get_float_by_key(JSON_Object *properties, const char *key)
{
	float result;

	if (json_object_has_value_of_type(properties, key, JSONNumber))
	{
		result = json_object_get_number(properties, key);
	}

	return result;
}

static void update_owm_pollution(ENVIRONMENT_TELEMETRY *telemetry)
{
	JSON_Array *list_array;
	JSON_Object *main_properties, *component_properties, *list_properties;
	JSON_Value *rootProperties = NULL;
	POLLUTION_T *pollution     = &telemetry->latest.pollution;
	double temp                = 0.0;

	if (!owm_initialized)
	{
		return;
	}

	char *data = dx_getHttpData(pollutionUrl, 6);

	if (data != NULL)
	{
		/*
		{
		  "coord": {
			"lon": 151.2101,
			"lat": -33.8601
		  },
		  "list": [
			{
			  "main": {
				"aqi": 3
			  },
			  "components": {
				"co": 250.34,
				"no": 0.72,
				"no2": 7.45,
				"o3": 133.04,
				"so2": 12.52,
				"pm2_5": 15.94,
				"pm10": 23.07,
				"nh3": 0.9
			  },
			  "dt": 1645153200
			}
		  ]
		}

		*/

		rootProperties = json_parse_string(data);
		if (rootProperties == NULL)
		{
			goto cleanup;
		}

		JSON_Object *rootObject = json_value_get_object(rootProperties);
		if (rootObject == NULL)
		{
			goto cleanup;
		}

		if ((list_array = json_object_dotget_array(rootObject, "list")) != NULL)
		{
			size_t numItems = json_array_get_count(list_array); // needs to be more than 0.

			if (numItems > 0)
			{
				// get the first item in the array
				list_properties = json_array_get_object(list_array, 0);

				if ((main_properties = json_object_get_object(list_properties, "main")) != NULL)
				{
					pollution->air_quality_index = get_float_by_key(main_properties, "aqi");
				}

				if ((component_properties = json_object_get_object(list_properties, "components")) != NULL)
				{
					pollution->carbon_monoxide   = get_float_by_key(component_properties, "co");
					pollution->nitrogen_monoxide = get_float_by_key(component_properties, "no");
					pollution->nitrogen_dioxide  = get_float_by_key(component_properties, "no2");
					pollution->ozone             = get_float_by_key(component_properties, "o3");
					pollution->sulphur_dioxide   = get_float_by_key(component_properties, "so2");
					pollution->ammonia           = get_float_by_key(component_properties, "nh3");
					pollution->pm2_5             = get_float_by_key(component_properties, "pm2_5");
					pollution->pm10              = get_float_by_key(component_properties, "pm10");
				}
			}
		}
		pollution->updated = true;
	}

cleanup:

	if (rootProperties != NULL)
	{
		json_value_free(rootProperties);
		rootProperties = NULL;
	}

	if (data != NULL)
	{
		free(data);
		data = NULL;
	}
}

void update_owm(ENVIRONMENT_TELEMETRY *environment)
{
	if (owm_cached)
	{
		return;
	}

	update_owm_weather(environment);
	update_owm_pollution(environment);

	owm_cached = environment->latest.pollution.updated && environment->latest.weather.updated;

	if (owm_cached)
	{
		dx_timerOneShotSet(&tmr_weather_cache_expired, &(struct timespec){weather_cache_minutes, 0});
	}
}

void init_open_weather_map_api_key(ALTAIR_CONFIG_T *altair_config, ENVIRONMENT_TELEMETRY *environment)
{
	if (!dx_isStringNullOrEmpty(altair_config->open_weather_map_api_key) && !owm_initialized &&
		environment->locationInfo.updated)
	{
		snprintf(weatherUrl, sizeof(weatherUrl), weatherURLTemplate, environment->locationInfo.lat,
			environment->locationInfo.lng, altair_config->open_weather_map_api_key);
		snprintf(pollutionUrl, sizeof(pollutionUrl), pollutionURLTemplate, environment->locationInfo.lat,
			environment->locationInfo.lng, altair_config->open_weather_map_api_key);

		owm_initialized = true;
		dx_timerStart(&tmr_weather_cache_expired);
	}
}