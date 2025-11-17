
## Prerequisites

- macOS
- Windows WSL with Ubuntu 25.04 (trixie)
- Ubuntu 25.04 (trixie) 
- Raspberry Pi OS (trixie)

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

## Build the Solution

1. From a terminal window, go to the **Altair-8800-Emulator/src** folder that you cloned to your computer.

1. Run the following commands to compile the Altair project:

    ```bash
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . --target all -j"$(nproc)"
    ```

1. Check the build completion message to confirm a successful build. The build completion message will be similar to `[100%] Built target serializer`. If the build process fails, check that you installed all the required packages.
