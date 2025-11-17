You can run the Altair emulator on your local computer or with a cloud service like [Azure Static website hosting in Azure Storage](https://learn.microsoft.com/azure/storage/blobs/storage-blob-static-website). 

The following instructions are for running the Altair emulator on your local computer.

1. Ensure Python 3 is installed on your computer. 

    ```bash
    python3 --version
    ```

    If Python 3 is not installed, refer to the [Python 3 installation instructions](https://www.python.org/downloads/).
2. Open a terminal window and navigate to the `Terminal` folder of the Altair emulator project you cloned from GitHub.

    ```bash
    cd Terminal
    ```
3. Start the Python web server.

    ```bash
    python3 -m http.server 8088
    ```
4. Open a web browser and navigate to the following URL. This assumes the Altair emulator is running on the same computer as the web browser.

    ```bash
    http://localhost:8088
    ```


1. If the Altair emulator is running on a different computer then add the hostname or the IP address of the computer running the Altair emulator to the  `altair` parameter of the `localhost` URL.

    ```bash
    http://localhost?altair=HOSTNAME_OR_IP_ADDRESS
    ```
