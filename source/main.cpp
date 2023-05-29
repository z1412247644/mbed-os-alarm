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

#include "MQ2.h"    //烟雾报警传感器库

#define MQTTCLIENT_QOS2 1

#include "ESP8266Interface.h"   //WiFi模组库

// #define SAMPLE_RATE     300ms

#define LED_ON 0
#define LED_OFF 1

#define JSON_TRUE "true"
#define JSON_FALSE "false"


ESP8266Interface wifi(PTE22, PTE23);

static BufferedSerial uart(PTA2, PTA1, 115200);     //创建串口,对应硬件 UART0， 波特率 115200

AnalogIn  ain_fire(A1);     //明火红外探测器模拟输入引脚
DigitalIn din_co(PTC7);     //烟雾探测器数字输入管脚（来自模块内部比较器输出）
MQ2 mq2(A0);    //烟雾探测器模拟输入管脚(用于计算烟雾浓度)

int arrivedcount = 0;

// void messageArrived(MQTT::MessageData& md)  //MQTT接收数据包回调
// {
//     MQTT::Message &message = md.message;
//     printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
//     printf("Payload %.*s\n", message.payloadlen, (char*)message.payload);
//     ++arrivedcount;
// }


int main()
{
    DigitalOut led_red(LED1);   //初始化红报警灯
    DigitalOut led_blue(LED3);  //初始化蓝报警灯

    mq2.begin();    //初始化MQ2传感器
    // MQ2_data_t MQ2_data;
    
    ain_fire.set_reference_voltage(3.3);    //由于ADC参考电压AVREF为3.3V，所以这里需要设置一下，ADC API才能正确输出电压值
    din_co.mode(PullUp);    

    float mq_CO, fire_vo;

    SocketAddress a;    //创建Socket地址类，用于保存目标服务器的IP地址和端口号

    printf("WiFi Start\r\n\r\n");

    printf("\r\nConnecting...\r\n");
    int ret = wifi.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);      //连接WIFI，WIFI名称和密码在 mbed_app.json中配置
    if (ret != 0) {
        printf("\r\nConnection error\r\n");
        return -1;
    }

    printf("Success\r\n\r\n");
    printf("MAC: %s\r\n", wifi.get_mac_address());
    wifi.get_ip_address(&a);    //获取WIFI IP地址
    printf("IP: %s\r\n", a.get_ip_address());
    wifi.get_netmask(&a);   //获取WIFI 子网掩码
    printf("Netmask: %s\r\n", a.get_ip_address());
    wifi.get_gateway(&a);   //获取WIFI 网关
    printf("Gateway: %s\r\n", a.get_ip_address());
    printf("RSSI: %d\r\n\r\n", wifi.get_rssi());    //获取WIFI信号强度



    MQTTSocket eth0(&wifi);     //新建MQTT socket类，绑定其到WIFI网卡
    char* hostname = "broker.emqx.io";      //设置MQTT服务器地址
    int port = 1883;        //设置MQTT服务器端口
    int rc = eth0.connect(hostname, port);      //连接MQTT服务器（TCP）
    if (rc != 0)
        printf("rc from TCP connect is %d\n", rc);
    
    MQTT::Client<MQTTSocket, Countdown> client =  MQTT::Client<MQTTSocket, Countdown>(eth0);       //创建MQTT客户端类，用于订阅和发布信息

    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-sample";
    if ((rc = client.connect(data)) != 0)       //登录MQTT服务器
        printf("rc from MQTT connect is %d\n", rc);
    
    char* topic = "testtopic/kl25_Data";    //用于通讯的MQTT TOPIC
    // if ((rc = client.subscribe(topic, MQTT::QOS0, messageArrived)) != 0)
    //     printf("rc from MQTT subscribe is %d\n", rc);

    MQTT::Message message;
    char buf[100];
    char co_alarm[10], fire_alarm[10];

    // message.qos = MQTT::QOS0;
    // message.retained = false;
    // message.dup = false;
    // message.payload = (void*)buf;
    // message.payloadlen = strlen(buf)+1;
    // rc = client.publish(topic, message);    //
    // while (arrivedcount < 10)
    client.yield(100);      //MQTT事务处理，等效于用定时器delay ms
    
    while (true) {
        mq_CO = mq2.readLPG();      //读取液化气浓度
        fire_vo = ain_fire.read_voltage();      //读取红外探测器电压 电压越小，红外能量越高，火越旺
        if(mq_CO>10 || !din_co.read()){     //判断烟雾浓度是否超标
            led_blue.write(LED_ON);
            strcpy(co_alarm, JSON_TRUE);
        }else{
            led_blue.write(LED_OFF);
            strcpy(co_alarm, JSON_FALSE);
        }
        if(fire_vo<3.0){        //判断红外能量是否超标，这里设定为低于3.0V报警，经验值
            led_red.write(LED_ON);
            strcpy(fire_alarm, JSON_TRUE);
        }else{
            led_red.write(LED_OFF);
            strcpy(fire_alarm, JSON_FALSE);
        }        

        sprintf(buf, "{\r\n\"co_data\": %.1f,\r\n\"co_alarm\": %s,\r\n\"fire_alarm\": %s\r\n}", mq_CO, co_alarm, fire_alarm);   //构造MQTT上报 JSON
        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*)buf;
        message.payloadlen = strlen(buf)+1;
        rc = client.publish(topic, message);    //发送 MQTT JSON数据
        if(rc!=0){
            printf("rc from MQTT publish is %d\n", rc);
        }
        printf("%s", buf);
        // printf("CO PPM: %.0f\r\n",mq_CO);
        // printf("................................\r\n"); 
        printf("\r\nread_voltage=%f\r\n", fire_vo);
        printf("................................\r\n"); 
        client.yield(1000);     //MQTT事务处理，等效于用定时器delay ms
        //ThisThread::sleep_for(SAMPLE_RATE);
        
    }

    // wifi.disconnect();

    // printf("\r\nDone\r\n");
}
