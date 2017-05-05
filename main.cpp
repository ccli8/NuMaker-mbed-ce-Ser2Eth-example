/*
 * Copyright (c) 2017 Nuvoton Tecnology Corp. All rights reserved.
 *
 * The code is forward data from UART port to Ethernet, and vice versa.
 * 
 *
 */
 
#include "ste_config.h"

/* If define USE_STATIC_IP, specify the default IP address. */
#if 0
    // private IP address for general purpose
#define IP_ADDRESS      "192.168.1.2"
#define NETWORK_MASK    "255.255.255.0"
#define GATEWAY_ADDRESS "192.168.1.1"
#else
    // private IP address only for test with Windows when DHCP server doesn't exist.
#define IP_ADDRESS      "169.254.108.2"
#define NETWORK_MASK    "255.255.0.0"
#define GATEWAY_ADDRESS "169.254.108.1"
#endif

/* Default configuration for network */
S_NET_CONFIG net_config = {IP_STATIC_MODE, IP_ADDRESS, NETWORK_MASK, GATEWAY_ADDRESS};

#if defined (TARGET_NUMAKER_PFM_M487)
#error Not defined UART ports yet.

#elif defined (TARGET_NUMAKER_PFM_NUC472)
BufferedSerial serial_0(PH_1, PH_0, 256, 4);    // UART4
BufferedSerial serial_1(PG_2, PG_1, 256, 4);    // UART0
BufferedSerial serial_2(PC_11, PC_10, 256, 4);  // UART2

#elif defined (TARGET_NUMAKER_PFM_M453)
#error The board has no Ethernet.

#else
#error define UART ports for your board.
#endif

/* Default configuration for network ports and UART ports, etc. */
S_PORT_CONFIG port_config[MAX_UART_PORTS] = {

#if MAX_UART_PORTS == 1
    {NET_SERVER_MODE, NET_PORT_BASE, &serial_0, DEFAULT_UART_BAUD, 8, 1, SerialBase::None}

#elif MAX_UART_PORTS == 2
    {NET_SERVER_MODE, NET_PORT_BASE + 0, &serial_0, DEFAULT_UART_BAUD, 8, 1, SerialBase::None},
    {NET_SERVER_MODE, NET_PORT_BASE + 1, &serial_1, DEFAULT_UART_BAUD, 8, 1, SerialBase::None}

#elif MAX_UART_PORTS == 3
    {NET_SERVER_MODE, NET_PORT_BASE + 0, &serial_0, DEFAULT_UART_BAUD, 8, 1, SerialBase::None},
    {NET_SERVER_MODE, NET_PORT_BASE + 1, &serial_1, DEFAULT_UART_BAUD, 8, 1, SerialBase::None},
    {NET_SERVER_MODE, NET_PORT_BASE + 2, &serial_2, DEFAULT_UART_BAUD, 8, 1, SerialBase::None}

#else
#error You have to define ports mapping table.
#endif
};

/* UART port to output debug message */
RawSerial output(USBTX, USBRX);             // UART3 on NuMaker-PFM-NUC472
EthernetInterface eth;

#ifdef ENABLE_WEB_CONFIG

/* Declare the SD card as storage */
NuSDBlockDevice bd;
FATFileSystem fs("fs");
bool SD_Card_Mounted = FALSE;

#endif

/* ---                  --- */

/*
 * Forward serial port data to ethernet, and vice versa.
 *
 */
void exchange_data(S_PORT_CONFIG *pmap, TCPSocket *psocket)
{
    unsigned char n_buf[256];
    unsigned char s_buf[256];
    int n_len = 0, n_index = 0;
    int s_len = 0, s_index = 0;
    
    while(1)
    {   
        /*** Network to Serial ***/
        
        if (n_len < 0 || n_len == n_index)
        {
            // net buffer is empty, try to get new data from network.
            n_len = psocket->recv(n_buf, sizeof(n_buf));
            if (n_len == NSAPI_ERROR_WOULD_BLOCK)
            {
            }
            else if (n_len < 0)
            {
                printf("Socket Recv Err (%d)\r\n", n_len);
                psocket->close();
                break;
            }
            else
            {
                n_index = 0;
            }
        }
        else
        {
            // send data to serial port.
            for(;n_index < n_len && pmap->pserial->writeable(); n_index++)
            {
                pmap->pserial->putc(n_buf[n_index]);
            }
        }

        /*** Serial to Network ***/
        
        if (pmap->pserial->readable())
        {
            // try to get more data from serial port
            for(s_index = 0; s_index < sizeof(s_buf) && pmap->pserial->readable(); s_index++)
                s_buf[s_index] = pmap->pserial->getc();
            
            // send all data to network.
            if (s_index > 0)
            {
                s_len = psocket->send(s_buf, s_index); 
                if (s_len == NSAPI_ERROR_WOULD_BLOCK)
                {
                    printf("Socket Send no block.\r\n");
                }               
                else if (s_len < 0)
                {
                    printf("Socket Send Err (%d)\r\n", s_len);
                    psocket->close();
                    break;
                }
                else if (s_len != s_index)
                {
                    printf("Socket Send not complete.\r\n");
                    psocket->close();
                    break;
                }
            }
        }
    }
}   

void bridge_net_client(S_PORT_CONFIG *pmap)
{
    TCPSocket socket;
    SocketAddress server_address;
    nsapi_error_t err;

    printf("Thread %x in TCP client mode.\r\n", (unsigned int)pmap);

    if ((err=socket.open(&eth)) < 0)
    {
        printf("TCP socket can't open (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    
    printf("Connecting server %s:%d ...\r\n", pmap->server_addr, pmap->server_port);
    while(1)
    {
        if ((err=socket.connect(pmap->server_addr, pmap->server_port)) >= 0)
            break;
    }
    
    printf("\r\nConnected.");

    socket.set_timeout(1);
    exchange_data(pmap, &socket);
}

void bridge_net_server(S_PORT_CONFIG *pmap)
{
    TCPServer tcp_server;
    TCPSocket client_socket;
    SocketAddress client_address;
    nsapi_error_t err;
    
    printf("Thread %x in TCP server mode.\r\n", (unsigned int)pmap);
    
    if ((err=tcp_server.open(&eth)) < 0)
    {
        printf("TCP server can't open (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    if ((err=tcp_server.bind(eth.get_ip_address(), pmap->port)) < 0)
    {
        printf("TCP server can't bind address and port (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    if ((err=tcp_server.listen(1)) < 0)
    {
        printf("TCP server can't listen (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }

    client_socket.set_timeout(1);

    while(1)
    {   
        if ((err=tcp_server.accept(&client_socket, &client_address)) < 0)
        {
            printf("TCP server fail to accept connection (%d)(%x).\r\n", err, (unsigned int)pmap);
            return;
        }

        printf("Connect (%d) from %s:%d ...\r\n", pmap->port, client_address.get_ip_address(), client_address.get_port());

        exchange_data(pmap, &client_socket);        
    }
}   

int main()
{
    /* Set the console baud-rate */
    output.baud(115200);
    printf("\r\nmbed OS version is %d.\r\n", MBED_VERSION);
    printf("Start Serial-to-Ethernet...\r\n");

#ifdef ENABLE_WEB_CONFIG

    /* Restore configuration from SD card */
    
    SD_Card_Mounted = (fs.mount(&bd) >= 0);
    if (SD_Card_Mounted)
    {
        FILE *fd = fopen(SER_CONFIG_FILE, "r");
        if (fd != NULL)
        {
            char pBuf[sizeof(port_config)+2];
            int len = fread(pBuf, 1, sizeof(port_config)+2, fd);
            if (len == (sizeof(port_config)+2) && pBuf[0] == 'N' && pBuf[1] == 'T') 
            {
                printf("Set Serial ports from config file in SD card.\r\n");
                memcpy(port_config, pBuf+2, sizeof(port_config));
            }
            else
                printf("Incorrect serial config file.\r\n");
            
            fclose(fd);
        }
        else
            printf("Can't open serial config file.\r\n");
        
        fd = fopen(NET_CONFIG_FILE, "r");
        if (fd != NULL)
        {
            char pBuf[sizeof(net_config)+2];
            int len = fread(pBuf, 1, sizeof(net_config)+2, fd);
            if (len == (sizeof(net_config)+2) && pBuf[0] == 'N' && pBuf[1] == 'T') 
            {
                printf("Set network from config file in SD card.\r\n");
                memcpy(&net_config, pBuf+2, sizeof(net_config));
            }
            else
                printf("Incorrect network config file.\r\n");
            
            fclose(fd);
        }
        else
            printf("Can't open network config file.\r\n");
    }
    else
    {
        printf("Can't find SD card.\r\n");
    }

#endif
    
    printf("Configure UART ports...\r\n");
    for(int i=0; i<MAX_UART_PORTS; i++)
    {
        port_config[i].pserial->baud(port_config[i].baud);
        port_config[i].pserial->format(port_config[i].data, port_config[i].parity, port_config[i].stop);
    }
    
    if (net_config.mode == IP_STATIC_MODE)
    {
        printf("Start Ethernet in Static mode.\r\n");
        eth.disconnect();
        ((NetworkInterface *)&eth)->set_network(net_config.ip, net_config.mask, net_config.gateway);
    }
    else
        printf("Start Ethernet in DHCP mode.\r\n");
    
    eth.connect();
    printf("IP Address is %s\r\n", eth.get_ip_address());
    
    Thread thread[MAX_UART_PORTS];
    
    // Folk thread for each port
    for(int i=0; i<MAX_UART_PORTS; i++)
    {
        if (port_config[i].mode == NET_SERVER_MODE)
        {
            thread[i].start(callback(bridge_net_server, &(port_config[i])));
        }
        else // if (port_config[i].mode == TCP_CLIENT_MODE)
        {
            thread[i].start(callback(bridge_net_client, &(port_config[i])));
        }
    }

#ifdef ENABLE_WEB_CONFIG
    
    /*** main thread to be a web server for configuration ***/
    start_httpd();

#endif
    
    while(1);

    /* end of main task */    
    //eth.disconnect();
}
