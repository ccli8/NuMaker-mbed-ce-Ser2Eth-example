/*
 * Copyright (c) 2017 Nuvoton Tecnology Corp. All rights reserved.
 *
 * Header for Serial-To-Ethernet configuration.
 *
 */

#ifndef _STE_CONFIG_H
#define _STE_CONFIG_H

#include "mbed.h"
#include "EthernetInterface.h"
#include "FATFileSystem.h"
#include "NuSDBlockDevice.h"


#include "BufferSerial.h"


/* A simple web server for network and UART configuration
   0: Disable the web server.
   1: Enable the web server
*/
#define ENABLE_WEB_CONFIG   1

/* Maximum UART ports supported.
   Must define enough tuples of mapping table "port_config[]" in main.c
*/
#define MAX_UART_PORTS      2

/* Default UART baud */
#define DEFAULT_UART_BAUD   115200

/* Network base port number to listen.
   The base port maps to the 1st UART port,
   the (base port + 1) maps to the 2nd UART port, etc.
*/
#define NET_PORT_BASE   10001

/* Files in SD card to store settings via web configuration */
#define SER_CONFIG_FILE "/fs/STE_SER.TXT"   // for serial ports
#define NET_CONFIG_FILE "/fs/STE_NET.TXT"   // for network

/* Maximum size of server domain name or address for web configuration */
#define MAX_SERVER_ADDRESS_SIZE     63

/* Maximum size of IP address for web configuration */
#define MAX_IPV4_ADDRESS_SIZE       15


/* Functions and global variables declaration. */

typedef enum {
    NET_SERVER_MODE = 0,
    NET_CLIENT_MODE
} E_NetMode;

typedef enum {
    IP_STATIC_MODE = 0,
    IP_DHCP_MODE
} E_IPMode;

typedef struct {
    E_IPMode        mode;
    char            ip[MAX_IPV4_ADDRESS_SIZE+1];
    char            mask[MAX_IPV4_ADDRESS_SIZE+1];
    char            gateway[MAX_IPV4_ADDRESS_SIZE+1];
} S_NET_CONFIG;

typedef struct {
    E_NetMode       mode;       // Network server or client mode
    int             port;       // Network port number (Server mode)
    BufferSerial  *pserial;   // UART number
    int             baud;       // UART baud
    int             data;       // UART data bits
    int             stop;       // UART stop bits
    mbed::SerialBase::Parity parity; // UART parity bit
    char            server_addr[MAX_SERVER_ADDRESS_SIZE+1]; // Server address for TCP client mode
    unsigned short  server_port;    // Server port for TCP client mode
} S_PORT_CONFIG;

//extern RawSerial output;        // for debug output
extern EthernetInterface eth;
extern S_PORT_CONFIG port_config[MAX_UART_PORTS];
extern S_NET_CONFIG net_config;

extern bool SD_Card_Mounted;
void start_httpd(void);

#endif
