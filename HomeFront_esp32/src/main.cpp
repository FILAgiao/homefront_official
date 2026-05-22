/*注意事项
本程序初步决定使用的是四个电磁阀，三个园子一个后面
如果要加电磁阀的话，有些部分的电磁阀遍历规律是不一样的
/////我是分割线///////
md 还是3个电磁阀好了。。。
让电磁阀一个一个工作！，水不够了
*/

/*
现在要做一个使用物理按钮进行浇水的东西,我认为,如果使用物理按钮,是否能同时使用手机?
应该可以,不管是哪个之间都要做好切换


*/
// todo:可以做一个不同地区的不同浇水比例的工具
////我觉得时间变量的销毁有问题!!!
////// really important👆/////已经修改
#include <Arduino.h>
#include <WiFi.h>
#include <Ticker.h>
#include <String.h>

#include "pinmap.h"
#include "hal/relay.h"
#include "core/globals.h"
#include "core/config.h"
#include "core/network.h"
#include "core/protocol.h"
#include "core/watering.h"
#include "core/sensor.h"
#include "core/ota.h"
#include "ui/menu.h"

// ============================================================
// 全局变量定义 (extern 声明在 core/globals.h)
// ============================================================

// ---- OTA ----
String upUrl = "";

// ---- Sensor ----
unsigned char item[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
String data_soil = "";

// ---- 网络 / WiFi ----
String ssid = "";
String password = "";
String device_id = "";
const char *host = "tcp.tlink.io";
const uint16_t httpPort = 8647;
WiFiClient client;
Ticker tk;

// ---- 时间 ----
struct tm timeinfo;
struct tm NET_LOSTING_time;
struct tm start_work_time;
char time_temp[10];
char set_begin_time[10];
unsigned long BEGIN_TIMESTAMP = 0;
bool NET_LOSTING_FLAG = false;

// ---- 浇水参数 ----
int pin_watering_time[MAX_VALVES] = {30, 30, 30, 0, 0, 0};
int working_solenoid_valve[MAX_VALVES] = {0, 0, 0, 0, 0, 0};
int valve_count = 3;           // 实际阀门数 (1~6), NVS 可配
int pump_count = 1;            // 实际水泵数 (0~2), NVS 可配
int wat_begin_hour = 4;
int wat_begin_min = 40;
int work_times = 0;
int time_gap_min = 0;
int solenoid_line = 0;

// ---- 浇水标志 ----
int auto_soil_watering_flag = 1;
int auto_timing_watering_flag = 0;
int carwash_flag = 0;
int hand_watering_flag = 0;
int pump_working_flag = 0;
int vegetable_flag_hand = 0;
int vegetable_flag_net = 0;
int carwash_duration_min = 30;
int field_valve_num = 3;        // 菜地阀门号 1-based, 1~valve_count (NVS 可配)
int net_solenoid_flag = 0;
int soil2wat = 0;
int reboot_flag = 0;

// ---- 状态机 ----
int current_state = STATE_IDLE;
unsigned long state_start_time = 0;

// ---- 心跳/断点 ----
volatile int breakpoint_flag = 1;
int wifi_retry_times = 0;
int wifi_to_reboot_times = 0;

// ---- 网络重连参数 ----
const unsigned long wifiRetryInterval = 300;
const unsigned long clientRetryInterval = 500;

// ---- 传感器 ----
float soil_moisture = 0;
float soil_moisture_need = 32;
int soil_moisture_test_maxsize = 777;
int soil_moisture_list_size = 0;

// ---- 调试/上报 ----
char data[128];
String time_status = "";
int ota_status = 0;
String ota_feedback = "";

// ---- 遥测缓存 ----
bool time_to_go_flag = false;
bool soil_to_go_flag = false;

// ---- 物理按钮 ----
int physical_buttons = 0;
int trigger_pin_status = 0;

// ============================================================
// Ticker 中断回调
// ============================================================
void time_fun()
{
    breakpoint_flag = 1;
}

// ============================================================
// setup / loop
// ============================================================

void setup()
{
    Serial.begin(4800);
    Serial2.begin(115200);
    configTime(8 * 3600, 0, "ntp1.aliyun.com", "ntp2.aliyun.com");
    delay(10);

    loadConfig();
    if (ssid.length() == 0)
    {
        startConfigPortal();
        return;
    }

    relay_init();
    menu_init();  // OLED + 编码器 + 按键

    Serial2.println();
    Serial2.println();
    Serial2.print("Connecting to ");
    Serial2.println(ssid);

    WiFi.begin(ssid.c_str(), password.c_str());
    wifi_reconnect_cx();

    wifi_retry_times = 0;
    Serial2.println("");
    Serial2.println("WiFi connected");
    Serial2.println("IP address: ");
    Serial2.println(WiFi.localIP());
    Serial2.print("connecting to ");
    Serial2.print(host);
    Serial2.print(':');
    Serial2.println(httpPort);

    // Phase3.5-fix: 修复 connect 之前 print 无意义的问题
    if (!client.connect(host, httpPort))
    {
        Serial2.println("connection failed");
        delay(5000);
    }
    else
    {
        Serial2.println("connection sucess!");
        tk.attach(40, time_fun);
        Serial2.println("send device_id");
        client.print(device_id);
    }
}

void loop()
{
    /*
    修改:
    当还在浇水的时候,如果网络断开了,应当:
    1.开启millis(),确认时间--->修改gettime()函数,仍然返回真值
    2.仍然继续与wifi\tlink服务器呼叫,但是不重启
    3.通过gettime()继续推进浇水序号

    完成浇水后(抑或是还没浇水前):
    继续重新连,如果实在不行,就可以重启?
    或:继续通过millis校准时间,实在是太久了,如几个小时未重连,就重启

    */
    menu_tick();  // 处理编码器/按键输入, 刷新 OLED

    wifi_reconnect_cx();
    check_client_connected();
    get_localtime();

    if (1 == breakpoint_flag)
    {
        if (client.connected())
        {
            Serial2.println("send heart beat sense");
            client.print("q");
        }
        breakpoint_flag = 0;
    }

    if (client.available())
    {
        Serial2.print("available\n");
        String ch = client.readString();
        Serial2.println(ch);
        handle_incoming_message(ch);
    }

    Serial2.println("******************");
    Serial2.print("pump_working_flag:");
    Serial2.println(pump_working_flag);
    Serial2.print("hand_watering_flag:");
    Serial2.println(hand_watering_flag);
    Serial2.print("soil2wat:");
    Serial2.println(soil2wat);
    Serial2.println("******************");

    if (0 == carwash_flag && 0 == vegetable_flag_hand && 0 == vegetable_flag_net)
    {
        if (go_watering())
        {
            Serial2.println("watt");
            Serial2.print("水泵正在工作吗:");
            Serial2.println(pump_working_flag);
            if (pump_working_flag == 1)
            {
                if (work_times > 0)
                {
                    soil2wat = 1;

                    for (int i = 0; i < valve_count; i++)
                    {
                        working_solenoid_valve[i] = 0;
                    }

                    working_solenoid_valve[(valve_count - work_times)] = 1;
                }

                Serial2.print("和开始的时间相差分钟数:");
                Serial2.println(time_gap(timeinfo, start_work_time));
                time_gap_min = time_gap(timeinfo, start_work_time);
                if (time_gap_min > pin_watering_time[(valve_count - work_times)])
                {
                    work_times = work_times - 1;
                    Serial2.println("worktimes-1了");
                    start_work_time = timeinfo;
                }
                Serial2.print("在浇倒数第几轮:");
                Serial2.println(work_times);
                if (work_times == 0)
                {
                    shut_all();
                    hand_watering_flag = 0;
                    Serial2.println("worktime等于0了");
                }
            }
        }
    }
    else if (1 == carwash_flag)
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > carwash_duration_min)
        {
            shut_all();
            carwash_flag = 0;
        }
    }
    else if (1 == vegetable_flag_net || 1 == vegetable_flag_hand)
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > pin_watering_time[field_valve_num - 1])
        {
            shut_all();
            vegetable_flag_net = 0;
            vegetable_flag_hand = 0;
        }
    }
    else if (1 == net_solenoid_flag)
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > 10)
        {
            Serial2.println("网络下发电磁阀超时10分钟，自动关闭");
            shut_all();
            net_solenoid_flag = 0;
        }
    }

    for (int i = 0; i < valve_count; i++)
    {
        Serial2.print(working_solenoid_valve[i]);
    }
    delay(100);
    get_localtime();
    delay(100);
    flag_execute();
    delay(100);
    send2clinet();
    delay(1000);
}
