/*
 * Copyright (c) 2006-2020 Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mbed.h"
#include "TCPSocket.h"

#include "MQTTClient.h"
#include "MQTTEthernet.h"
#include "MQTTmbed.h"
#include <cstdio>
#include <cstring>

#include "MQ2.h"

#define MQTTCLIENT_QOS2 1

#include "ESP8266Interface.h"

#define SAMPLE_RATE     300ms

#define LED_ON 0
#define LED_OFF 1

#define JSON_TRUE "true"
#define JSON_FALSE "false"


ESP8266Interface wifi(PTE22, PTE23);

static BufferedSerial uart(PTA2, PTA1, 115200);

AnalogIn  ain_fire(A1);
DigitalIn din_co(PTC7);
MQ2 mq2(A0);

int arrivedcount = 0;

void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}


int main()
{
    DigitalOut led_red(LED1);
    DigitalOut led_blue(LED3);

    mq2.begin();
    // MQ2_data_t MQ2_data;
    
    ain_fire.set_reference_voltage(3.3);
    din_co.mode(PullUp);

    float mq_CO, fire_vo;

    SocketAddress a;

    printf("WiFi Start\r\n\r\n");

    printf("\r\nConnecting...\r\n");
    int ret = wifi.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\r\nConnection error\r\n");
        return -1;
    }

    printf("Success\r\n\r\n");
    printf("MAC: %s\r\n", wifi.get_mac_address());
    wifi.get_ip_address(&a);
    printf("IP: %s\r\n", a.get_ip_address());
    wifi.get_netmask(&a);
    printf("Netmask: %s\r\n", a.get_ip_address());
    wifi.get_gateway(&a);
    printf("Gateway: %s\r\n", a.get_ip_address());
    printf("RSSI: %d\r\n\r\n", wifi.get_rssi());

    // std::tm time_now;
    // wifi.get_time(&time_now);
    // set_time(mktime(&time_now));
    // time_t seconds = time(NULL);

    // printf("Time as seconds since January 1, 1970 = %u\n", (unsigned int)mktime(&time_now));

    // printf("Time as a basic string = %s", ctime(&seconds));

    // char buffer[32];
    // strftime(buffer, 32, "%I:%M %p\n", localtime(&seconds));
    // printf("Time as a custom formatted string = %s", buffer);


    MQTTSocket eth0(&wifi);
    char* hostname = "broker.emqx.io";
    int port = 1883;
    int rc = eth0.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\n", rc);
    
    MQTT::Client<MQTTSocket, Countdown> client =  MQTT::Client<MQTTSocket, Countdown>(eth0);
    // http_demo(&wifi);
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-sample";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\n", rc);
    
    char* topic = "testtopic/kl25_Data";
    // if ((rc = client.subscribe(topic, MQTT::QOS0, messageArrived)) != 0)
    //     printf("rc from MQTT subscribe is %d\n", rc);

    MQTT::Message message;
    char buf[100];
    char co_alarm[10], fire_alarm[10];

    // sprintf(buf,"hello world");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, message);
    // while (arrivedcount < 10)
    client.yield(100);
    
    while (true) {
        // led_red = !led_red;
        // led_blue = led_red;
        mq_CO = mq2.readLPG();
        fire_vo = ain_fire.read_voltage();
        // printf("-------- %d -----\r\n", din_co.read());
        if(mq_CO>10 || !din_co.read()){
            led_blue.write(LED_ON);
            strcpy(co_alarm, JSON_TRUE);
        }else{
            led_blue.write(LED_OFF);
            strcpy(co_alarm, JSON_FALSE);
        }
        if(fire_vo<3.0){
            led_red.write(LED_ON);
            strcpy(fire_alarm, JSON_TRUE);
        }else{
            led_red.write(LED_OFF);
            strcpy(fire_alarm, JSON_FALSE);
        }        

        sprintf(buf, "{\r\n\"co_data\": %.1f,\r\n\"co_alarm\": %s,\r\n\"fire_alarm\": %s\r\n}", mq_CO, co_alarm, fire_alarm);
        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*)buf;
        message.payloadlen = strlen(buf)+1;
        rc = client.publish(topic, message);
        if(rc!=0){
            printf("rc from MQTT publish is %d\n", rc);
        }
        printf("%s", buf);
        // printf("CO PPM: %.0f\r\n",mq_CO);
        // printf("................................\r\n"); 
        printf("read_voltage=%f\r\n", fire_vo);
        printf("................................\r\n"); 
        client.yield(1000);
        //ThisThread::sleep_for(SAMPLE_RATE);
        
    }

    // wifi.disconnect();

    // printf("\r\nDone\r\n");
}
