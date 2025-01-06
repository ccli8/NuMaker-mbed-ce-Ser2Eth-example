/*
 * Copyright (c) 2017 Nuvoton Tecnology Corp. All rights reserved.
 *
 * micro web server for serial to Ethernet configuration.
 * 
 *
 */
  
#include <string.h>
#include "ste_config.h"

#if ENABLE_WEB_CONFIG

#define PAGE_HEADER \
"<html><head>\r\n" \
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n" \
"<title>Serial to Ethernet Configuration</title>\r\n" \
"</head>\r\n"

#define PAGE_HEADER_FOR_REFRESH \
"<html><head>\r\n" \
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n" \
"<meta http-equiv=\"refresh\" content=\"10; url=index.html\" />\r\n" \
"<title>Serial to Ethernet Configuration</title>\r\n" \
"</head>\r\n"

#define PAGE_RESTART_FORM \
"<form action=\"/config.html\" method=\"GET\">\r\n" \
"<input type=\"hidden\" name=\"restart\" value=\"1\">\r\n" \
"<input type=\"submit\" value=\"Restart\"> </form>\r\n"

#define PAGE_RESETCONF_FORM \
"<form action=\"/config.html\" method=\"GET\">\r\n" \
"<input type=\"hidden\" name=\"resetconf\" value=\"1\">\r\n" \
"<input type=\"submit\" value=\"Reset Config\"> </form>\r\n"

#define PAGE_CONFIG_FORM_HEAD \
"<form action=\"/config.html\" method=\"GET\">\r\n"

#define PAGE_CONFIG_SERIAL_FORM \
"<input type=\"hidden\" name=\"uart_port\" value=\"%d\">\r\n" \
" Baud <select type=\"text\" name=\"baud\">\r\n \
    <option value=\"115200\"%s>115200</option>\r\n \
    <option value=\"9600\"%s>9600</option>\r\n \
    <option value=\"4800\"%s>4800</option>\r\n \
    <option value=\"2400\"%s>2400</option>\r\n \
    </select> &nbsp&nbsp;\r\n \
  Data bits <select type=\"text\" name=\"databits\">\r\n \
    <option value=\"8\"%s>8</option>\r\n \
    <option value=\"7\"%s>7</option>\r\n \
    <option value=\"6\"%s>6</option>\r\n \
    <option value=\"5\"%s>5</option>\r\n \
    </select> &nbsp;&nbsp;\r\n \
  Parity <select type=\"text\" name=\"parity\">\r\n \
    <option value=\"0\"%s>None</option>\r\n \
    <option value=\"1\"%s>Odd</option>\r\n \
    <option value=\"2\"%s>Even</option>\r\n \
    </select> &nbsp;&nbsp;\r\n \
  Stop bits <select type=\"text\" name=\"stopbits\">\r\n \
    <option value=\"1\"%s>1</option>\r\n \
    <option value=\"2\"%s>2</option>\r\n \
    </select> <br>\r\n \
Network mode <select type=\"text\" name=\"netmode\">\r\n \
    <option value=\"0\"%s>Server</option>\r\n \
    <option value=\"1\"%s>Client</option>\r\n \
    </select> <br> Set following fields for Client only. <br>\r\n \
Server address <input type=\"text\" name=\"sipaddr\" size=\"16\" value=\"%s\"> &nbsp&nbsp;\r\n \
Server port <input type=\"text\" name=\"sport\" size=\"5\" value=\"%d\">\r\n"

#define PAGE_CONFIG_NET_FORM \
"<input type=\"hidden\" name=\"net_config\" value=\"1\">\r\n \
IP Address Mode <select type=\"text\" name=\"ipmode\">\r\n \
    <option value=\"0\"%s>Static</option>\r\n \
    <option value=\"1\"%s>DHCP</option>\r\n \
    </select> <br> Set following fields for Static IP only. <br>\r\n \
IP Address <input type=\"text\" name=\"ipaddr\" size=\"16\" value=\"%s\">\r\n \
Netmask <input type=\"text\" name=\"mask\" size=\"16\" value=\"%s\">\r\n \
Gateway Address <input type=\"text\" name=\"gwaddr\" size=\"16\" value=\"%s\">\r\n"

typedef void (*PROC_PAGE_PFN)(TCPSocket *, char *);

typedef struct {
    const char      *vhtmlpage; // virtual page name, eg. index.html ...
    PROC_PAGE_PFN   pfn;
} S_WEB_PAGE_SETTING;

extern void process_http_index_html(TCPSocket *, char *);
extern void process_http_restart_html(TCPSocket *, char *);
extern void process_http_resetconf_html(TCPSocket *, char *);
extern void process_http_config_html(TCPSocket *, char *);
extern void process_http_err_html(TCPSocket *, char *);

#define MAX_WEB_GET_PAGE_SETTINGS   5
S_WEB_PAGE_SETTING web_get_pages_setings[MAX_WEB_GET_PAGE_SETTINGS] = {
    {"/ ",                          process_http_index_html},
    {"/index.html ",                process_http_index_html},
    {"/config.html?restart=1 ",     process_http_restart_html},
    {"/config.html?resetconf=1 ",   process_http_resetconf_html},
    {"/config.html?",               process_http_config_html}   
};

#define MAX_WEB_POST_PAGE_SETTINGS  1
S_WEB_PAGE_SETTING web_post_pages_setings[MAX_WEB_POST_PAGE_SETTINGS] = {
    {"/config.html ", process_http_config_html},
};

typedef struct {
    int         type;       // 0: integer, 1:string
    int         value;      // integer value for integer type
    char       *str;        // point to string for string type
    int         min;
    int         max;        // max value of integer or max length of string
    const char *cmd;
    int         cmd_len;
} S_WEB_CMD_PARAMS;

#define MAX_WEB_CMD_PARAMS  13
S_WEB_CMD_PARAMS web_cmd_params[MAX_WEB_CMD_PARAMS] = {
    {0, 0, NULL, 1, MAX_UART_PORTS, "uart_port=", 10},
    {0, 0, NULL, 2400, 115200,      "baud=", 5},
    {0, 0, NULL, 5, 8,              "databits=", 9},
    {0, 0, NULL, 0, 2,              "parity=", 7},
    {0, 0, NULL, 1, 2,              "stopbits=", 9},
    {0, 0, NULL, 0, 1,              "netmode=", 8},
    {1, 0, NULL, 0, 16,             "sipaddr=", 8},
    {0, 0, NULL, 0, 65535,          "sport=", 6},   
    {0, 0, NULL, 1, 1,              "net_config=", 11},
    {0, 0, NULL, 0, 1,              "ipmode=", 7},
    {1, 0, NULL, 0, 1,              "ipaddr=", 7},
    {1, 0, NULL, 0, 1,              "mask=", 5},
    {1, 0, NULL, 0, 1,              "gwaddr=", 7}
};

const char *parity_string[] = {"None", "Odd", "Even"};
const char *netmode_string[] = {"Server", "Client"};
const char *ipmode_string[] = {"Static", "DHCP"};


int inspect_each_setting(char *pbuf)
{
    int i;
    int v;

    for(i=0; i<MAX_WEB_CMD_PARAMS; i++)
    {
        web_cmd_params[i].value = 0;
        web_cmd_params[i].str = NULL;
    }
    
    while(*pbuf != '\0')
    {
        for(i=0; i<MAX_WEB_CMD_PARAMS; i++)
        {
            if (strncmp(pbuf, web_cmd_params[i].cmd, web_cmd_params[i].cmd_len) == 0)
            {
                pbuf += web_cmd_params[i].cmd_len;
                
                if (web_cmd_params[i].type == 0)
                {
                    v = atoi(pbuf);
                    if (v >= web_cmd_params[i].min && v <= web_cmd_params[i].max)
                        web_cmd_params[i].value = v;                    
                }
                else
                {
                    web_cmd_params[i].str = pbuf;
                }

                for(; *pbuf != '\0'; pbuf++)
                {
                    if (*pbuf == '&')
                    {
                        *pbuf++ = '\0';
                        break;
                    }
                    else if (*pbuf == ' ')
                    {
                        *pbuf = '\0';
                        break;
                    }       
//                  else if (*pbuf == '\r')
//                  {
//                      *pbuf++ = '\0';
//                      if (*pbuf == '\n')
//                          *pbuf++ = '\0';
//                      break;
//                  }
//                  else if (*pbuf == '\n' || *pbuf == ' ')
//                  {
//                      *pbuf++ = '\0';
//                      break;
//                  }
                }
                
                break;
            }   
        }
        
        if (i >= MAX_WEB_CMD_PARAMS)
            return -1;
    }
    
    return 0;
}
    
int process_settings(char *pbuf)
{
    FILE *fp;
    
    printf("Verify Configure setting...\r\n");
    printf("---Dump Settings---\r\n%s\r\n---End of dump---\r\n", pbuf);

    if (SD_Card_Mounted == FALSE)
    {
        printf("SD card doesn't exist. Ignore settings.\r\n");
        return -1;
    }
    
    if (inspect_each_setting(pbuf) < 0)
    {
        printf("Unsupported parameters.\r\n");
        return -1;
    }
    
    if (web_cmd_params[0].value > 0)
    {
        int port = web_cmd_params[0].value - 1;
        port_config[port].baud = web_cmd_params[1].value;
        port_config[port].data = web_cmd_params[2].value;
        port_config[port].stop = web_cmd_params[4].value;
        port_config[port].parity = (mbed::SerialBase::Parity) web_cmd_params[3].value;
        port_config[port].mode = (E_NetMode) web_cmd_params[5].value;
        
        if (web_cmd_params[6].str != NULL)
        {
            strncpy(port_config[port].server_addr, web_cmd_params[6].str, MAX_SERVER_ADDRESS_SIZE);
            port_config[port].server_addr[MAX_SERVER_ADDRESS_SIZE] = '\0';
        }
            
        port_config[port].server_port = web_cmd_params[7].value;
        
        printf("Save Serial Config setting...\r\n");    
        fp = fopen(SER_CONFIG_FILE, "w");
        if (fp != NULL)
        {
            char pBuf[2] = {'N', 'T'};
            printf("Write config file to SD card.\r\n");
            fwrite(pBuf, 2, 1, fp);
            fwrite(port_config, sizeof(port_config), 1, fp);
            fclose(fp);
        }
        else
        {
            printf("Can't write to SD card.\r\n");
        }
    }
    else if (web_cmd_params[8].value > 0)
    {
        net_config.mode = (E_IPMode)web_cmd_params[9].value;
        if (web_cmd_params[10].str != NULL)
        {
            strncpy(net_config.ip, web_cmd_params[10].str, MAX_IPV4_ADDRESS_SIZE);
            net_config.ip[MAX_IPV4_ADDRESS_SIZE] = '\0';
        }
        if (web_cmd_params[11].str != NULL)
        {
            strncpy(net_config.mask, web_cmd_params[11].str, MAX_IPV4_ADDRESS_SIZE);
            net_config.mask[MAX_IPV4_ADDRESS_SIZE] = '\0';
        }
        if (web_cmd_params[12].str != NULL)
        {
            strncpy(net_config.gateway, web_cmd_params[12].str, MAX_IPV4_ADDRESS_SIZE);
            net_config.gateway[MAX_IPV4_ADDRESS_SIZE] = '\0';
        }
        printf("Save Network Config setting...\r\n");   
        fp = fopen(NET_CONFIG_FILE, "w");
        if (fp != NULL)
        {
            char pBuf[2] = {'N', 'T'};
            printf("Write config file to SD card.\r\n");
            fwrite(pBuf, 2, 1, fp);
            fwrite(&net_config, sizeof(net_config), 1, fp);
            fclose(fp);
        }
        else
        {
            printf("Can't write to SD card.\r\n");
        }
    }
    
    return 0;
}


void send_web_data(TCPSocket *socket, char *pbuf)
{
    char *p = pbuf;
    int size = strlen(pbuf);
    
    while(size > 0)
    {
        int ret = socket->send(p, size);
        if (ret < 0)
            break;

        p += ret;
        size -= ret;
    }
}

void send_web_header(TCPSocket *socket, int num)
{
    static const char *webpage_header[] = {
        PAGE_HEADER,
        PAGE_HEADER_FOR_REFRESH
    };

    send_web_data(socket, (char *)webpage_header[num]);
}

const char *status_messages[] = {
    "\r\n",
    "Configuration update successfully. </p>\r\n",
    "Configuration setting error.</p>\r\n",
    "Reset Configuration successfully. </p>\r\n",
    "Web page will reload automatically after 10 seconds. </p>\r\n",
    "Unknown settings. </p>\r\n"
};

void send_web_body(TCPSocket *socket, int num)
{
    int i;
    static char pbuf[3072];

    /* send page body */
    send_web_data(socket, (char *)"<body>Serial to Ethernet Configuration </p>\r\n");
    
    /* send status message */
    send_web_data(socket, (char *)status_messages[num]); 

    send_web_data(socket, (char *)PAGE_RESTART_FORM); 
    send_web_data(socket, (char *)PAGE_RESETCONF_FORM); 
    
    /* send network configuration form */
    send_web_data(socket, (char *)PAGE_CONFIG_FORM_HEAD);
    
    sprintf(pbuf, PAGE_CONFIG_NET_FORM,
        (net_config.mode == IP_STATIC_MODE)?" selected":"",
        (net_config.mode == IP_DHCP_MODE)?" selected":"",
        net_config.ip,
        net_config.mask,
        net_config.gateway);
    
    send_web_data(socket, pbuf);
    send_web_data(socket, (char *)"<br><input type=\"submit\" value=\"Config Net\"> </form><br>\r\n");
    
    /* send serial ports configuration form */
    send_web_data(socket, (char *)PAGE_CONFIG_FORM_HEAD);
    for(i=0; i<MAX_UART_PORTS; i++)
    {
        S_PORT_CONFIG *s = &port_config[i];
        
        sprintf(pbuf, "Serial port #%d<br>\r\n", i+1); 
        send_web_data(socket, pbuf);
        sprintf(pbuf, PAGE_CONFIG_SERIAL_FORM,
            i+1,
            (s->baud == 115200)?" selected":"",
            (s->baud == 9600)?" selected":"",
            (s->baud == 4800)?" selected":"",
            (s->baud == 2400)?" selected":"",
            (s->data == 8)?" selected":"",
            (s->data == 7)?" selected":"",
            (s->data == 6)?" selected":"",
            (s->data == 5)?" selected":"",
            (s->parity == 0)?" selected":"",
            (s->parity == 1)?" selected":"",
            (s->parity == 2)?" selected":"",
            (s->stop == 1)?" selected":"",
            (s->stop == 2)?" selected":"",
            (s->mode == 0)?" selected":"",
            (s->mode == 1)?" selected":"",
            s->server_addr,
            s->server_port);
        
        send_web_data(socket, pbuf);
        
#if MAX_UART_PORTS == 1
        sprintf(pbuf, "<br><input type=\"submit\" value=\"Config Port\"> </form><br>\r\n");
#else       
        sprintf(pbuf, "<br><input type=\"submit\" value=\"Config Port #%d\"> </form><br>\r\n", i);
#endif
        send_web_data(socket, pbuf);
    }
    
    /* send end of page body */
    send_web_data(socket, (char *)"</body></html>\r\n");
}

void process_http_index_html(TCPSocket *socket, char *pbuf)
{
    send_web_header(socket, 0);
    send_web_body(socket, 0);
}

void process_http_restart_html(TCPSocket *socket, char *pbuf)
{
    send_web_header(socket, 1);
    send_web_body(socket, 4);

    printf("Restart system...\r\n");
    ThisThread::sleep_for(1000);    // wait 1 second
//  SYS_ResetCPU();
    SYS_ResetChip();
    ThisThread::sleep_for(100000);
}

void process_http_resetconf_html(TCPSocket *socket, char *pbuf)
{
    if (SD_Card_Mounted)
    {                   
        remove(SER_CONFIG_FILE);
        remove(NET_CONFIG_FILE);
        printf("Deleted config files.\r\n");
    }
    else
    {
        printf("SD card doesn't exist.\r\n");
    }
    
    send_web_header(socket, 0);
    send_web_body(socket, 3);
}

void process_http_config_html(TCPSocket *socket, char *pbuf)
{
    send_web_header(socket, 0);

    if (process_settings(pbuf) >= 0)
        send_web_body(socket, 1);
    else
        send_web_body(socket, 2);
}

void process_http_err_html(TCPSocket *socket, char *pbuf)
{
    send_web_header(socket, 0);
    send_web_body(socket, 5);
}

void process_http_get_request(TCPSocket *socket, char *pbuf)
{
    int i;
    
    for(i=0; i<MAX_WEB_GET_PAGE_SETTINGS; i++)
    {
        int len = strlen(web_get_pages_setings[i].vhtmlpage);
        if (strncmp(pbuf, web_get_pages_setings[i].vhtmlpage, len) == 0)
        {
            if (web_get_pages_setings[i].pfn != NULL)
            {
                web_get_pages_setings[i].pfn(socket, pbuf+len);
                break;
            }
        }
    }
}

void process_http_post_request(TCPSocket *socket, char *pbuf)
{
}

void process_http_request(TCPSocket *socket)
{
    char w_buf[2048];
    int w_len;
    
    w_len = socket->recv(w_buf, 2047);
    if (w_len <= 0)
        return;
    
    w_buf[w_len] = '\0';
    printf("\n\r---DUMP HTTP DATA (%d)---\r\n%s\r\n---END OF DUMP---\r\n", w_len, w_buf);
    
    if (strncmp(w_buf, "GET ", 4) == 0)
    {
        process_http_get_request(socket, w_buf+4);
    }
    else if (strncmp(w_buf, "POST ", 5) == 0)
    {
        process_http_post_request(socket, w_buf+5);
    }
    else
    {
        process_http_get_request(socket, (char *)"/ ");
    }
}

void start_httpd(void)
{
    TCPSocket http_server;
    TCPSocket *http_socket;
    SocketAddress http_address;
    nsapi_error_t err_t;

    if (http_server.open(&eth) < 0)
    {
        printf("http server can't be created.\r\n");
        return;
    }
    if (http_server.bind(80) < 0)
    {
        printf("http server can't bind address and port.\r\n");
        return;
    }
    if (http_server.listen(1) < 0)
    {
        printf("http server can't listen.\r\n");
        return;
    }
    
    printf("Start http server...\r\n");

    while(1)
    {
        http_socket = http_server.accept(&err_t);
        if (err_t < 0)
        {
            printf("http server fail to accept connection.\r\n");
            return;
        }
        
        http_socket->getpeername(&http_address);
        printf("http from %s:%d ...\r\n", http_address.get_ip_address(), http_address.get_port());
        process_http_request(http_socket);
        http_socket->close();
    }
}

#endif

            