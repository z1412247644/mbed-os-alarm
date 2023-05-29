#include <cstdio>
#if !defined(MQTTSOCKET_H)
#define MQTTSOCKET_H

#include "MQTTmbed.h"
#include <EthernetInterface.h>
#include <Timer.h>

#include "netsocket/NetworkInterface.h"

class MQTTSocket
{
public:
    MQTTSocket(NetworkInterface *anet)
    {
        net = anet;
        open = false;
    }
    
    int connect(char* hostname, int port, int timeout=1000)
    {
        if (open)
            disconnect();
        nsapi_error_t rc = mysock.open(net);
        // printf("==============> open_error = %d\r\n", rc);
        open = true;
        mysock.set_blocking(true);
        mysock.set_timeout((unsigned int)timeout);

        SocketAddress a;
        net->gethostbyname(hostname, &a);
        printf("DEBUG==========>  = %s\r\n", a.get_ip_address());
        a.set_port(port);
        rc = mysock.connect(a);
        // printf("==============> connect_error = %d\r\n", rc);
        mysock.set_blocking(false);  // blocking timeouts seem not to work
        return rc;
    }

    // common read/write routine, avoiding blocking timeouts
    int common(unsigned char* buffer, int len, int timeout, bool read)
    {
        timer.start();
        mysock.set_blocking(false); // blocking timeouts seem not to work
        int bytes = 0;
        bool first = true;
        do 
        {
            if (first)
                first = false;
            else
                ThisThread::sleep_for(timeout < 100 ? timeout : 100);
            int rc;
            if (read)
                rc = mysock.recv((char*)buffer, len);
            else
                rc = mysock.send((char*)buffer, len);
            if (rc < 0)
            {
                if (rc != NSAPI_ERROR_WOULD_BLOCK)
                {
                    bytes = -1;
                    break;
                }
            } 
            else
                bytes += rc;
        }
        while (bytes < len && timer.read_ms() < timeout);
        timer.stop();
        return bytes;
    }

    /* returns the number of bytes read, which could be 0.
       -1 if there was an error on the socket
    */
    int read(unsigned char* buffer, int len, int timeout)
    {
        return common(buffer, len, timeout, true);
    }

    int write(unsigned char* buffer, int len, int timeout)
    {
        return common(buffer, len, timeout, false);
    }

    int disconnect()
    {
        open = false;
        return mysock.close();
    }

    /*bool is_connected()
    {
        return mysock.is_connected();
    }*/

private:

    bool open;
    TCPSocket mysock;
    NetworkInterface *net;
    Timer timer;

};

#endif
