## Install Visual Studio Code

1. Install [Visual Studio Code](https://code.visualstudio.com&azure-portal=true).

2. Open a Terminal command prompt.
    If you are using Windows Subsystem for Linux then open a WSL command prompt.
3. Go to the **Altair-8800-Emulator/src** folder
4. Run the following command to open the folder with VS Code.

    ```bash
    code .
    ```

5. Install the following Visual Studio Code extensions:
    - If using WSL, then install the [Visual Studio Code Remote-WSL extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl&azure-portal=true).
    - [Visual Studio Code CodeLLDB extension](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
    - [CMake Tools extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
    - [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)

6. Open the .vscode/launch.json file in VS Code.
7. Optionally update the [args] json property for the build configuration. Available options are:
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

8. Save the launch.json file.
9. Select the **GCC** or **Clang** Kit.
10. Press <kbd>F5</kbd> to compile and launch the Altair emulator.
