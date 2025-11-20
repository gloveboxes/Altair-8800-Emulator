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

    // Extract all values to local variables to ensure proper alignment and avoid 
    // varargs issues on 32-bit ARM systems with hard-float ABI
    long timestamp = (long)time(NULL);
    
    // Extract floats - they will be promoted to double in varargs
    double wind_speed = (double)environment->latest.weather.wind_speed;
    double aqi = (double)environment->latest.pollution.air_quality_index;
    double co = (double)environment->latest.pollution.carbon_monoxide;
    double no = (double)environment->latest.pollution.nitrogen_monoxide;
    double no2 = (double)environment->latest.pollution.nitrogen_dioxide;
    double o3 = (double)environment->latest.pollution.ozone;
    double so2 = (double)environment->latest.pollution.sulphur_dioxide;
    double nh3 = (double)environment->latest.pollution.ammonia;
    double pm2_5 = (double)environment->latest.pollution.pm2_5;
    double pm10 = (double)environment->latest.pollution.pm10;
    
    // Extract doubles
    double latitude = environment->locationInfo.lat;
    double longitude = environment->locationInfo.lng;

    // Use dx_jsonSerialize for type-safe serialization
    // All floats are pre-converted to doubles to avoid varargs promotion issues
    if (dx_jsonSerialize(msgBuffer, MSG_BUFFER_BYTES, 18,
        DX_JSON_STRING, "device", mqtt_config.client_id,
        DX_JSON_LONG,   "timestamp", timestamp,
        DX_JSON_INT,    "temperature", environment->latest.weather.temperature,
        DX_JSON_INT,    "pressure", environment->latest.weather.pressure,
        DX_JSON_INT,    "humidity", environment->latest.weather.humidity,
        DX_JSON_DOUBLE, "windspeed", wind_speed,
        DX_JSON_INT,    "winddirection", environment->latest.weather.wind_direction,
        DX_JSON_DOUBLE, "aqi", aqi,
        DX_JSON_DOUBLE, "co", co,
        DX_JSON_DOUBLE, "no", no,
        DX_JSON_DOUBLE, "no2", no2,
        DX_JSON_DOUBLE, "o3", o3,
        DX_JSON_DOUBLE, "so2", so2,
        DX_JSON_DOUBLE, "nh3", nh3,
        DX_JSON_DOUBLE, "pm2_5", pm2_5,
        DX_JSON_DOUBLE, "pm10", pm10,
        DX_JSON_DOUBLE, "latitude", latitude,
        DX_JSON_DOUBLE, "longitude", longitude))
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