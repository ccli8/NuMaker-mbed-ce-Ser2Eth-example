/*
 * Copyright (c) 2017 Nuvoton Tecnology Corp. All rights reserved.
 *
 * The code is forward data from UART port to Ethernet, and vice versa.
 * 
 *
 */
#include "ste_config.h"

/* If choose static IP, specify the default IP address here. */
#if 0
    // private IP address for general purpose
#define IP_ADDRESS      "192.168.1.2"
#define NETWORK_MASK    "255.255.255.0"
#define GATEWAY_ADDRESS "192.168.1.1"
#else
    // private IP address only for test with Windows when DHCP server doesn't exist.
    // Windows set its LAN IP address to 169.254.xxx.xxx / 255.255.0.0 
#define IP_ADDRESS      "169.254.108.2"
#define NETWORK_MASK    "255.255.0.0"
#define GATEWAY_ADDRESS "169.254.108.1"
#endif

/* Default IP configuration for Ethernet network */
S_NET_CONFIG net_config = {IP_STATIC_MODE, IP_ADDRESS, NETWORK_MASK, GATEWAY_ADDRESS};

#if defined (TARGET_NUMAKER_PFM_M487) || defined(TARGET_NUMAKER_IOT_M487)
#if MBED_MAJOR_VERSION <= 5
BufferSerial serial_0(PB_3, PB_2, 256, 4);    // UART1
BufferSerial serial_1(PA_5, PA_4, 256, 4);    // UART5
//BufferSerial serial_2(PB_13, PB_12, 256, 4);  // UART0, Debug
#else
BufferSerial serial_0(PB_3, PB_2);    // UART1
BufferSerial serial_1(PA_5, PA_4);    // UART5
//BufferSerial serial_2(PB_13, PB_12);  // UART0, Debug
#endif

#elif defined (TARGET_NUMAKER_PFM_NUC472)
#if MBED_MAJOR_VERSION <= 5
BufferSerial serial_0(PH_1, PH_0, 256, 4);    // UART4
BufferSerial serial_1(PG_2, PG_1, 256, 4);    // UART0
BufferSerial serial_2(PC_11, PC_10, 256, 4);  // UART2
#else
BufferSerial serial_0(PH_1, PH_0);    // UART4
BufferSerial serial_1(PG_2, PG_1);    // UART0
BufferSerial serial_2(PC_11, PC_10);  // UART2
#endif

#elif defined (TARGET_NUMAKER_PFM_M453) || defined(TARGET_NUMAKER_PFM_NANO130) || defined(TARGET_NUMAKER_PFM_M2351)
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

EthernetInterface eth;

#if ENABLE_WEB_CONFIG

/* Declare the SD card as storage */
NuSDBlockDevice *bd = new NuSDBlockDevice();
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
    unsigned int eth_tx_count = 0;
    
    while(1)
    {   
        /*** Network to Serial ***/
        
        if (n_len == 0)
        {
            // net buffer is empty, try to get new data from network.
            n_len = psocket->recv(n_buf, sizeof(n_buf));
            if (n_len == 0)
            {
                eth_tx_count += 3;
            }
            else if (n_len == NSAPI_ERROR_WOULD_BLOCK)
            {
                n_len = 0;
            }
            else if (n_len < 0)
            {
                printf("Socket Recv Err (%d)\r\n", n_len);
                break;
            }
        }
        else
        {
            n_index += pmap->pserial->write(n_buf+n_index, n_len-n_index);
            if (n_index == n_len)
            {
                n_len = 0;
                n_index = 0;
            }
        }

        /*** Serial to Network ***/

        // try to get more data from serial port
#if MBED_MAJOR_VERSION <= 5        
        for(; s_index < sizeof(s_buf) && pmap->pserial->readable(); s_index++)
            s_buf[s_index] = pmap->pserial->getc();
#else
        if (pmap->pserial->readable())
        {
            s_len = pmap->pserial->read(s_buf+s_index, sizeof(s_buf)-s_index);
            if (s_len > 0)
                s_index += s_len;
        }
#endif
        
        if (s_index >= 240 || (s_index != 0 && ++eth_tx_count >= 5))
        {
            s_len = psocket->send(s_buf, s_index); 

            if (s_len == NSAPI_ERROR_WOULD_BLOCK)
            {
                printf("Socket Send no block.\r\n");
            }               
            else if (s_len < 0)
            {
                printf("Socket Send Err (%d)\r\n", s_len);
                break;
            }
            else // s_len >= s_index
            {
                unsigned int i;

                // move remain data if existed.
                for(i=0; s_len < s_index; i++, s_len++)
                    s_buf[i] = s_buf[s_len];

                s_index = i;
                eth_tx_count = 0;
            }
        }       
    }
}   
   
void bridge_net_client(S_PORT_CONFIG *pmap)
{
    TCPSocket socket;
    SocketAddress server_ipaddr;
    nsapi_error_t err;

    printf("Thread %x in TCP client mode.\r\n", (unsigned int)pmap);

    if ((err=socket.open(&eth)) != NSAPI_ERROR_OK)
    {
        printf("TCP socket can't open (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    
    printf("Connecting server %s:%d ...\r\n", pmap->server_addr, pmap->server_port);
    while(1)
    {
        server_ipaddr.set_ip_address(pmap->server_addr);
        server_ipaddr.set_port(pmap->server_port);
        if ((err=socket.connect(server_ipaddr)) >= 0)
        {
            printf("\r\nConnected.");
            break;
        }
    }
    
    socket.set_timeout(1);
    exchange_data(pmap, &socket);
    socket.close();
}

void bridge_net_server(S_PORT_CONFIG *pmap)
{
    TCPSocket tcp_server;
    TCPSocket *client_socket;
    SocketAddress client_ipaddr;
    nsapi_error_t err;
    
    printf("Thread %x in TCP server mode.\r\n", (unsigned int)pmap);
    
    if ((err=tcp_server.open(&eth)) != NSAPI_ERROR_OK)
    {
        printf("TCP server can't open (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    if ((err=tcp_server.bind(pmap->port)) != NSAPI_ERROR_OK)
    {
        printf("TCP server can't bind address and port (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }
    if ((err=tcp_server.listen(1)) < 0)
    {
        printf("TCP server can't listen (%d)(%x).\r\n", err, (unsigned int)pmap);
        return;
    }

    while(1)
    {   
        client_socket = tcp_server.accept(&err);
        if (err != NSAPI_ERROR_OK && err != NSAPI_ERROR_WOULD_BLOCK)
        {
            printf("TCP server fail to accept connection (%d)(%x).\r\n", err, (unsigned int)pmap);
            return;
        }
        else
        {
            client_socket->getpeername(&client_ipaddr);
            printf("Connect (%d) from %s:%d ...\r\n", pmap->port, client_ipaddr.get_ip_address(), client_ipaddr.get_port());
  
            client_socket->set_timeout(1);
            exchange_data(pmap, client_socket);
            client_socket->close();
        }
    }
}   

int main()
{
    SocketAddress ip_addr;
    SocketAddress ip_mask;
    SocketAddress ip_gwaddr;
    
#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif
    printf("Start Serial-to-Ethernet...\r\n");

#if ENABLE_WEB_CONFIG

    /* Restore configuration from SD card */
    
    printf("Mounting SD card...\r\n");
    SD_Card_Mounted = (fs.mount(bd) == 0);
    if (SD_Card_Mounted)
    {
        printf("SD card mounted. Read configuration file...\r\n");
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
    
    /* Configure serial ports */
    printf("Configure UART ports...\r\n");
    for(int i=0; i<MAX_UART_PORTS; i++)
    {
        port_config[i].pserial->baud(port_config[i].baud);
        port_config[i].pserial->format(port_config[i].data, port_config[i].parity, port_config[i].stop);
    }
    
    /* Configure network IP address */
    if (net_config.mode == IP_STATIC_MODE)
    {
        printf("Start Ethernet in Static mode.\r\n");
        eth.disconnect();
        
        ip_addr.set_ip_address(net_config.ip);
        ip_mask.set_ip_address(net_config.mask);
        ip_gwaddr.set_ip_address(net_config.gateway);
        ((NetworkInterface *)&eth)->set_network(ip_addr, ip_mask, ip_gwaddr);
    }
    else
        printf("Start Ethernet in DHCP mode.\r\n");
    
    eth.connect();
    eth.get_ip_address(&ip_addr);
    printf("IP Address is %s\r\n", ip_addr.get_ip_address());

    Thread thread[MAX_UART_PORTS];
    
    /* Folk thread for each serial port */  
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

#if ENABLE_WEB_CONFIG
    
    /*** main thread to be a web server for configuration ***/
    start_httpd();

#endif
    
    while(1);

    /* end of main task */    
    //eth.disconnect();
}
