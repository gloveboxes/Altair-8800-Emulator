#!/bin/bash

# Start web server for Terminal interface
cd /app/Terminal 
python3 -m http.server 8080 &
WEB_PID=$!

# Change to app directory
cd /app

# Build the command line arguments
ARGS=""

# MQTT Configuration (optional)
if [ ! -z "$MQTT_HOST" ]; then
    ARGS="$ARGS -m $MQTT_HOST"
fi

if [ ! -z "$MQTT_PORT" ]; then
    ARGS="$ARGS -p $MQTT_PORT"
fi

if [ ! -z "$MQTT_CLIENT_ID" ]; then
    ARGS="$ARGS -c $MQTT_CLIENT_ID"
fi

if [ ! -z "$MQTT_USERNAME" ]; then
    ARGS="$ARGS -U $MQTT_USERNAME"
fi

if [ ! -z "$MQTT_PASSWORD" ]; then
    ARGS="$ARGS -P $MQTT_PASSWORD"
fi

# Network Configuration
if [ ! -z "$NETWORK_INTERFACE" ]; then
    ARGS="$ARGS -n $NETWORK_INTERFACE"
else
    # Default to eth0 if not specified
    ARGS="$ARGS -n eth0"
fi

# Hardware Configuration
if [ ! -z "$FRONT_PANEL" ]; then
    ARGS="$ARGS -f $FRONT_PANEL"
fi

# CPU Behavior Configuration
if [ ! -z "$SLOW_CPU_ON_DISCONNECT" ]; then
    ARGS="$ARGS -s $SLOW_CPU_ON_DISCONNECT"
fi

# External Services
if [ ! -z "$OPEN_WEATHER_MAP_API_KEY" ]; then
    ARGS="$ARGS -o $OPEN_WEATHER_MAP_API_KEY"
fi

if [ ! -z "$OPENAI_API_KEY" ]; then
    ARGS="$ARGS -a $OPENAI_API_KEY"
fi

if [ ! -z "$OPENAI_ENDPOINT" ]; then
    ARGS="$ARGS -e $OPENAI_ENDPOINT"
fi

# Cleanup function
cleanup() {
    echo "Shutting down services..."
    kill $WEB_PID 2>/dev/null
    kill $ALTAIR_PID 2>/dev/null
    exit 0
}

# Set up signal handlers
trap cleanup SIGTERM SIGINT

# Start Altair emulator
/usr/local/bin/Altair_emulator $ARGS &
ALTAIR_PID=$!

# Wait for any process to exit
wait -n

exit $?