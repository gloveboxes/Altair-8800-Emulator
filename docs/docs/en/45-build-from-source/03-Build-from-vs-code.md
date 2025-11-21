## Install Prerequisites

1. Install
   - [Visual Studio Code](https://code.visualstudio.com&azure-portal=true).
   - Install Docker.
   - Windows: Install WSL and latest version of Ubuntu.

## Clone the Solution Repo

=== "Windows"

    1. Open WSL/Ubuntu terminal window.
    2. Clone the repo

        ```bash
        git clone --recurse-submodules https://github.com/gloveboxes/Altair-8800-Emulator
        ```

=== "Linux and macOS"

    1. Open a terminal window
    1. Clone the repo

        ```bash
        git clone --recurse-submodules https://github.com/gloveboxes/Altair-8800-Emulator
        ```


## Open the Solution

1. From command line, navigate to the **Altair-8800-Emulator** folder
2. Run the following command to open the folder with VS Code.

    ```bash
    code .
    ```

3. Reopen in Container
    - This will build an Ubuntu 25.04 (trixie) environment and install all the dev tools.

4. Optionally update the [args] json property in the **.vscode/launch.json** file for the build configuration. Available options are:
    - `--MqttHost <host>`: MQTT broker hostname (required for MQTT)
    - `--MqttPort <port>`: MQTT broker port (default: 1883)
    - `--MqttClientId <client_id>`: MQTT client ID (default: AltairEmulator_<timestamp>)
    - `--MqttUsername <username>`: MQTT username (default: none)
    - `--MqttPassword <password>`: MQTT password (default: none)
    - `--NetworkInterface <iface>`: Network interface to use
    - `--FrontPanel <mode>`: Front panel selection: sensehat, kit, none (default: none)
    - `--OpenWeatherMapKey <key>`: OpenWeatherMap API key
    - `--OpenAIKey <key>`: OpenAI API key
    - `--OpenAIEndpoint <url>`: OpenAIEnpoint
    - `--SlowCpuOnDisconnect <bool>`: true or false
        - The default endpoint is **https://api.openai.com/v1/chat/completions**.
        - For LM Studio, use **http://IP_ADDRESS:1234/v1/chat/completions**. If Altair runs in a container, use the LM Studio server's IP address—not localhost—since localhost refers to the container itself.

    **Example: Connecting to a ThingsBoard MQTT broker**

    To connect to a ThingsBoard MQTT broker, set the `args` property in your `.vscode/launch.json` like this:

    ```json
    "args": [
        "--MqttHost", "my-thingsboard-host",
        "--MqttClientId", "vscode",
    ]
    ```

5. Save the launch.json file.
6. Press <kbd>F5</kbd> to compile and launch the Altair emulator.
