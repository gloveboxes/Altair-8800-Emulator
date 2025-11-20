/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "iotc_manager.h"
#include "dx_mqtt.h"

// External reference to global MQTT configuration
extern DX_MQTT_CONFIG mqtt_config;

void update_geo_location(ENVIRONMENT_TELEMETRY *environment)
{
    static bool updated = false;

    if (!updated && environment->locationInfo.updated)
    {
        updated = true;
        // Device twin reporting removed - using MQTT only
        dx_Log_Debug("Country: %s, City: %s\n", environment->locationInfo.country, environment->locationInfo.city);
    }
}

void publish_telemetry(ENVIRONMENT_TELEMETRY *environment)
{
    if (!dx_isMqttConnected() || !environment->valid)
    {
        return;
    }

    memset(msgBuffer, 0, MSG_BUFFER_BYTES);

    // Use dx_jsonSerialize for type-safe serialization
    // This prevents varargs alignment issues on 32-bit systems where floats/doubles
    // can be misaligned causing data corruption
    if (dx_jsonSerialize(msgBuffer, MSG_BUFFER_BYTES, 18,
        DX_JSON_STRING, "device", mqtt_config.client_id,
        DX_JSON_LONG,   "timestamp", time(NULL),
        DX_JSON_INT,    "temperature", environment->latest.weather.temperature,
        DX_JSON_INT,    "pressure", environment->latest.weather.pressure,
        DX_JSON_INT,    "humidity", environment->latest.weather.humidity,
        DX_JSON_FLOAT,  "windspeed", environment->latest.weather.wind_speed,
        DX_JSON_INT,    "winddirection", environment->latest.weather.wind_direction,
        DX_JSON_FLOAT,  "aqi", environment->latest.pollution.air_quality_index,
        DX_JSON_FLOAT,  "co", environment->latest.pollution.carbon_monoxide,
        DX_JSON_FLOAT,  "no", environment->latest.pollution.nitrogen_monoxide,
        DX_JSON_FLOAT,  "no2", environment->latest.pollution.nitrogen_dioxide,
        DX_JSON_FLOAT,  "o3", environment->latest.pollution.ozone,
        DX_JSON_FLOAT,  "so2", environment->latest.pollution.sulphur_dioxide,
        DX_JSON_FLOAT,  "nh3", environment->latest.pollution.ammonia,
        DX_JSON_FLOAT,  "pm2_5", environment->latest.pollution.pm2_5,
        DX_JSON_FLOAT,  "pm10", environment->latest.pollution.pm10,
        DX_JSON_DOUBLE, "latitude", environment->locationInfo.lat,
        DX_JSON_DOUBLE, "longitude", environment->locationInfo.lng))
    {
        // Publish telemetry via MQTT instead of Azure IoT Hub
        DX_MQTT_MESSAGE mqtt_msg = {
            .topic = "v1/devices/me/telemetry", 
            .payload = msgBuffer, 
            .payload_length = strlen(msgBuffer), 
            .qos = 0, 
            .retain = false
        };
        printf("Publishing telemetry: %s\n", msgBuffer);
        dx_mqttPublish(&mqtt_msg);
    }
    else
    {
        Log_Debug("Failed to serialize telemetry JSON. Msg not sent.\n");
    }
}