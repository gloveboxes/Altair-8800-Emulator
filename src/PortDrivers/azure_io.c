#include "azure_io.h"

#include "mqtt_manager.h"

typedef struct
{
    char buffer[256];
    int index;
} JSON_UNIT_T;

extern ENVIRONMENT_TELEMETRY environment;

static JSON_UNIT_T ju;
static volatile bool publish_json_pending    = false;
static volatile bool publish_weather_pending = false;

DX_ASYNC_HANDLER(async_publish_weather_handler, handle)
{
    if (environment.valid && dx_isMqttConnected())
    {
#ifndef ALTAIR_CLOUD
        publish_telemetry(&environment);
#endif
    }
    publish_weather_pending = false;
}
DX_ASYNC_HANDLER_END

DX_ASYNC_HANDLER(async_publish_json_handler, handle)
{
    if (dx_isMqttConnected())
    {
#ifndef ALTAIR_CLOUD
        // Publish JSON data via MQTT instead of Azure IoT Hub
        DX_MQTT_MESSAGE mqtt_msg = {
            .topic = "altair/json", 
            .payload = ju.buffer, 
            .payload_length = strlen(ju.buffer), 
            .qos = 0, 
            .retain = false
        };
        dx_mqttPublish(&mqtt_msg);
        dx_Log_Debug("Published JSON data via MQTT\n");
#endif
    }
    publish_json_pending = false;
}
DX_ASYNC_HANDLER_END

size_t azure_output(int port_number, uint8_t data, char *buffer, size_t buffer_length)
{
    switch (port_number)
    {
        case 31: // publish weather json
            if (!publish_json_pending)
            {
                if (ju.index == 0)
                {
                    memset((void *)ju.buffer, 0x00, sizeof(ju.buffer));
                }

                if (data != 0 && ju.index < sizeof(ju.buffer))
                {
                    ju.buffer[ju.index++] = (char)data;
                }

                if (data == 0)
                {
                    publish_json_pending = true;
                    ju.index             = 0;
                    dx_asyncSend(&async_publish_json, NULL);
                }
            }
            break;
        case 32: // publish weather
            if (!publish_weather_pending)
            {
                publish_weather_pending = true;
                dx_asyncSend(&async_publish_weather, NULL);
            }
            break;
    }

    return 0;
}

uint8_t azure_input(uint8_t port_number)
{
    uint8_t retVal = 0;

    switch (port_number)
    {
        case 31: // publish weather json pending
            retVal = (uint8_t)publish_json_pending;
            break;
        case 32: // publish weather pending
            retVal = (uint8_t)publish_weather_pending;
            break;
    }
    return retVal;
}