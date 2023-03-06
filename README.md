# Serial to Ethernet

This example shows how to transfer data between serial port and ethernet.

## Required hardware
* A supported target -
 [NuMaker-IoT-M487](https://os.mbed.com/platforms/NUMAKER-IOT-M487/)
 [NuMaker-PFM-M487](https://os.mbed.com/platforms/NUMAKER-PFM-M487/)
 [Numaker-PFM-NUC472](https://os.mbed.com/platforms/Nuvoton-NUC472/)
 
* MicroSD card is optional. It uses to store configuration when existed.

## Compile Settings

* Default socket number is 4, please add following settings to mbed_app.json to increase it.

    "target_overrides": {
        "*": {
          "lwip.socket-max": 8,
          "lwip.tcp-socket-max": 8, 
          "lwip.udp-socket-max": 8          
        }

## Configuration

* Mbed OS version

The template set Mbed OS 5 as defalut but it has been updated for Mbed OS 6.
Because BufferedSerial has built in Mbed OS 6, the BufferedSerial library in the template has to be removed if you switch Mbed OS to version 6 to avoid conflict.

* Following configurations are set in ste_config.h

ENABLE_WEB_CONFIG
    Define ENABLE_WEB_CONFIG to active a simple web server for UART ports and Ethernet port configuration.

MAX_UART_PORTS
    Maximum UART ports supported. It should be 1, 2, or 3. Please also define mapping table "port_config[]" in main.c

DEFAULT_UART_BAUD
    Default UART baud

NET_PORT_BASE
    Network base port number to listen. The base port maps to the 1st UART port, the (base port + 1) maps to the 2nd UART port, etc.

SER_CONFIG_FILE // for serial ports
NET_CONFIG_FILE // for network
    Files in SD card to store settings via web configuration

MAX_SERVER_ADDRESS_SIZE
    Maximum size of server address for web configuration

MAX_IPV4_ADDRESS_SIZE
Maximum size of IP address for web configuration

            