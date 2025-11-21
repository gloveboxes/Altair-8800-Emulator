1. From a terminal window, change to the **Altair-8800-Emulator/src** folder that you cloned to your computer.
2. You can run the Altair emulator in standalone mode.

    ```bash
    ./build/Altair_emulator
    ```

## Command Line Arguments

The Altair emulator accepts the following command line arguments:

- `-m`, `--MqttHost <host>`: MQTT broker hostname (required for MQTT)
- `-p`, `--MqttPort <port>`: MQTT broker port (default: 1883)
- `-c`, `--MqttClientId <client_id>`: MQTT client ID (default: `AltairEmulator_<timestamp>`)
- `-U`, `--MqttUsername <username>`: MQTT username (default: none)
- `-P`, `--MqttPassword <password>`: MQTT password (default: none)
- `-n`, `--NetworkInterface <iface>`: Network interface to use
- `-f`, `--FrontPanel <mode>`: Front panel selection: sensehat, kit, none (default: none)
- `-o`, `--OpenWeatherMapKey <key>`: OpenWeatherMap API key
- `-a`, `--OpenAIKey <key>`: OpenAI API key
- `-e`, `--OpenAIEndpoint <url>`: OpenAIEnpoint
- `-s`, `--SlowCpuOnDisconnect <bool>`: true or false

    - The default endpoint is **https://api.openai.com/v1/chat/completions**.
    - For LM Studio, use **http://IP_ADDRESS:1234/v1/chat/completions**. If Altair runs in a container, use the LM Studio server's IP address—not localhost—since localhost refers to the container itself.
- `-h`, `--help`: Show help message

### Example usage

Run the emulator with MQTT and weather integration:

```bash
./build/Altair_emulator --MqttHost mqtt_host --MqttPort 1883 --MqttClientId MyAltair --OpenWeatherMapKey <your_api_key> --NetworkInterface wlan0 --FrontPanel sensehat
```

### Example usage (short argument names)

Run the emulator with MQTT and weather integration using short argument names:

```bash
./build/Altair_emulator -m mqtt_host -p 1883 -c MyAltair -o <your_api_key> -n wlan0 -f sensehat
```
