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
#include <ArduinoJson.h>
#include <Ticker.h>
#include <String.h>
#include <HTTPUpdate.h>
#include <Preferences.h>

#include "pinmap.h"
#include "hal/relay.h"

// OTA升级的固件地址（从NVS读取）
String upUrl = "";

/// 以下是湿度传感器所需要的数据
// RS485 土壤传感器: 新PCB上通过 SP3485 连接 GPIO1(TX)/GPIO3(RX) = UART0
// 注意: 与下载串口共用，烧录固件时需断开 RS485 设备
// 若 RE/DE 为自动收发则直接使用 Serial
unsigned char item[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B}; // 16进制测温命令
String data_soil = "";                                                    // 接收到的16进制字符串

// 电磁阀数量（继电器通道数，可根据实际接线调整）
#define NUM_VALVES 7

/// 以下是中心浇水控制器需要的参数
Ticker tk;
StaticJsonDocument<200> doc;
WiFiClient client;
struct tm timeinfo;
struct tm NET_LOSTING_time;
struct tm start_work_time;
// 电磁阀/水泵已改用 74HC595 继电器控制，参见 pinmap.h 和 hal/relay.h
// 旧 GPIO 直驱已移除 (原 Solenoid_Pin[7], Pump_pin)
int pin_watering_time[NUM_VALVES] = {30, 30, 30, 0, 0, 0, 0}; // 每个电磁阀浇水时间(分钟)
int working_solenoid_valve[NUM_VALVES] = {0, 0, 0, 0, 0, 0, 0}; // 当前工作的电磁阀标志
int auto_soil_watering_flag = 1;                            // 到达一定湿度再浇水的flag
int auto_timing_watering_flag = 0;                          // 按照时间进行浇水的引脚

int ota_status = 0;
String ota_feedback = "";

int physical_buttons = 0; // 通过这个来判断是否需要物理按钮

int carwash_flag = 0;
int hand_watering_flag = 0;
int pump_working_flag = 0;
int vegetable_flag_hand = 0;
int vegetable_flag_net = 0;
int pool_watering_flag = 0;
int net_solenoid_flag = 0; // 网络下发数字启动电磁阀标志，用于10分钟超时保护
// int car_wash_trigger_pin = 21;   // 手动洗车开关
// int hand_water_trigger_pin = 22; // 手动浇水开关
// int vegetable_knob_pin = 23;     // 菜地旋钮
// volatile int solenoid_valve4 = 0;
int trigger_pin_status;

int reboot_flag = 0;
String time_status = "";
int solenoid_line = 0;
// volatile int which2debug_num = 0; //正在debug的电磁阀的序号,可能不需要这个变量了
float soil_moisture = 0;
float soil_moisture_need = 32; // 最低土壤湿度
// float soil_moisture_history_avg;
int soil_moisture_test_maxsize = 777; // 这个数值不能太大,否则会导致一次太干,但是浇完水还是没啥反应.
int soil_moisture_list_size = 0;
// int *time_flag = new int(0);  //必须这样做否则没法delete
int work_times = 0;
int wat_begin_hour = 4;
int wat_begin_min = 40;
int soil2wat = 0; // 如果是这个状态代表因为土壤干燥正在浇水
// int pinled = 32;
// const char *ssid = "family_2.4g";
// const char *password = "13505795150";
String ssid = "";
String password = "";
const char *host = "tcp.tlink.io";
const uint16_t httpPort = 8647;
unsigned long BEGIN_TIMESTAMP = 0; // 处理millis的返回时间,起点
// unsigned long END_TIMESTAMP=0;//处理millis()记录的另外一个时间,终点
bool NET_LOSTING_FLAG = false;
bool time_to_go_flag = false;
bool soil_to_go_flag = false;
unsigned long wifiReconnectTimer = 0;
unsigned long clientReconnectTimer = 0;
const unsigned long wifiRetryInterval = 300;   // WiFi 连接重试间隔（毫秒）
const unsigned long clientRetryInterval = 500; // 服务器连接重试间隔（毫秒）
int time_gap_min = 0;                          // 浇水的时间是否已经达到要求？
// const char *device_id = "R6P6K29X5PW1L607";// fixme:这个是测试组
String device_id = "";
//
/* 这些是设置时间的代码,但是现在被换为阿里云了
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 4 * 3600;     //不知道为何是4*60*60
const int daylightOffset_sec = 4 * 3600; //不知道为何是4*60*60
*/
int wifi_retry_times = 0;
int wifi_to_reboot_times = 0;
// int led_switch = 0;
volatile int breakpoint_flag = 1; // 23333这个是控制断点的,不是time_flag
char data[128];           // 回传的一大条数据都在里面，注意数据长度
char time_temp[10];
char set_begin_time[10];
//****************************************************************以下是OTA远程升级代码
// 当升级开始时，打印日志
void update_started()
{
    Serial2.println("CALLBACK:  HTTP update process started");
}

// 当升级结束时，打印日志
void update_finished()
{
    Serial2.println("CALLBACK:  HTTP update process finished");
}

// 当升级中，打印日志
void update_progress(int cur, int total)
{
    Serial2.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

// 当升级失败时，打印日志
void update_error(int err)
{
    Serial2.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

/**
 * 固件升级函数
 * 在需要升级的地方，加上这个函数即可，例如setup中加的updateBin();
 * 原理：通过http请求获取远程固件，实现升级
 */
void updateBin()
{
    Serial2.println("start update");
    WiFiClient UpdateClient;

    // 如果是旧版esp32 SDK，需要删除下面四行，旧版不支持，不然会报错
    //...看来我是旧版本esp32.。。23333333
    //   httpUpdate.onStart(update_started);//当升级开始时
    //   httpUpdate.onEnd(update_finished);//当升级结束时
    //   httpUpdate.onProgress(update_progress);//当升级中
    //   httpUpdate.onError(update_error);//当升级失败时

    t_httpUpdate_return ret = httpUpdate.update(UpdateClient, upUrl);
    switch (ret)
    {
    case HTTP_UPDATE_FAILED: // 当升级失败
        Serial2.println("[update] Update failed.");
        ota_feedback = "[update] Update failed.";
        break;
    case HTTP_UPDATE_NO_UPDATES: // 当无升级
        Serial2.println("[update] Update no Update.");
        ota_feedback = "[update] Update no Update.";
        break;
    case HTTP_UPDATE_OK: // 当升级成功
        Serial2.println("[update] Update ok.");
        ota_feedback = "[update] Update ok.";
        break;
    }
}
//***************************************************************OTA远程升级代码结束

void time_fun()

{ // 中断之后跳到这里来，不要搞的太复杂
    breakpoint_flag = 1;
}
String fenge(String str, String fen, int index)
{
    int weizhi;
    int maxParts = 16;  // Phase3-fix: 限制最大分割数，防止栈溢出
    String temps[maxParts];
    int i = 0;
    do
    {
        weizhi = str.indexOf(fen);
        if (weizhi != -1)
        {
            if (i >= maxParts) break;
            temps[i] = str.substring(0, weizhi);
            str = str.substring(weizhi + fen.length(), str.length());
            i++;
        }
        else
        {
            if (str.length() > 0 && i < maxParts)
                temps[i] = str;
        }
    } while (weizhi >= 0);

    if (index > i)
        return "-1";
    return temps[index];
}

// (length() 模板已移除，改用 NUM_VALVES 常量)
void send2clinet()
{
    int field_t = 0;
    int corner_t = 0;
    sprintf(set_begin_time, "%d:%d", wat_begin_hour, wat_begin_min); // 回传+读取

    if (1 == ota_status)
    {
        time_status = ota_feedback; // 如果有在更新，则收到的升级反馈会显示在物联网平台中的时间框框
    }
    if (pin_watering_time[6] > 0)
    { // 这里也要修改引脚!!!
        field_t = 1;
    }
    else
    {
        field_t = 0;
    }
    if (pin_watering_time[4] > 0)
    {
        corner_t = 1;
    }
    else
    {
        corner_t = 0;
    }
    sprintf(data, "#%d*%d*%d*%d*%d*%f*%d*%s*%d*%f*%d*%s*%d*%d*%d#",
         solenoid_line, carwash_flag, auto_soil_watering_flag, auto_timing_watering_flag,hand_watering_flag, soil_moisture, pump_working_flag, time_status.c_str(), reboot_flag, soil_moisture_need, pin_watering_time[0], set_begin_time,  ota_status,0, physical_buttons);
    // unsigned
    // sprintf(data, "#1*%d*%d*%d*%d*%d#", carwash_flag, auto_watering_flag, hand_watering_flag, soil_moisture, pump_working_flag);
    Serial2.print("回送的数据为：");
    Serial2.println(data);
    client.print(data);
    // delay(2000);
}
// **非阻塞WiFi重连**
void wifi_reconnect_cx()
{
    static unsigned long lastAttemptTime = 0;

    if (WiFi.status() == WL_CONNECTED)
    {
        wifi_retry_times = 0;
        NET_LOSTING_FLAG = false;
        return;
    }

    // 只有当超过设定时间间隔时才尝试连接
    if (millis() - lastAttemptTime >= wifiRetryInterval)
    {
        lastAttemptTime = millis();
        wifi_retry_times++;

        if (wifi_retry_times < 150)
        {
            Serial2.print(".");
            // WiFi.begin(ssid, password); // 如果需要，可在此重新尝试连接WiFi
        }
        else
        {
            if (soil2wat == 1)
            {
                Serial2.println("使用millis()进行猜测时间");
                NET_LOSTING_FLAG = true;
                NET_LOSTING_time = timeinfo;
                BEGIN_TIMESTAMP = millis(); // 记录断网时间
            }
            else
            {
                wifi_to_reboot_times++;
                if (wifi_to_reboot_times > 500)
                {
                    Serial2.println("准备重启");
                    ESP.restart();
                }
            }
        }
    }
}

// **非阻塞检测客户端（tlink服务器）连接**
void check_client_connected()
{
    static unsigned long lastAttemptTime = 0;

    if (client.connected() || WiFi.status() != WL_CONNECTED)
    {
        wifi_to_reboot_times = 0;
        return;
    }

    // 只有当超过设定时间间隔时才尝试连接服务器
    if (millis() - lastAttemptTime >= clientRetryInterval)
    {
        lastAttemptTime = millis();
        wifi_retry_times++;

        Serial2.println("尝试重新连接服务器...");
        client.connect(host, httpPort);

        if (client.connected())
        {
            Serial2.println("发送设备ID");
            client.print(device_id);
        }

        if (wifi_retry_times > 20)
        {
            wifi_retry_times = 0;
        }
    }

    if (!client.connected())
    {
        wifi_to_reboot_times++;
        if (wifi_to_reboot_times > 500)
        {
            Serial2.println("准备重启");
            ESP.restart();
        }
    }
}

void pump_work()
{
    if (pump_working_flag == 1)
    {
        relay_set(RELAY_CH_PUMP, true);
    }
    else
    {
        relay_set(RELAY_CH_PUMP, false);
    }
}
void Solenoid_OffAll(int a = 0) // 设置电磁阀标志位(1-based编号),a=0全关
{
    solenoid_line = a;
    hand_watering_flag = 0;
    for (int i = 0; i < NUM_VALVES; i++)
    {
        if (a != 0 && i == a - 1)
        {
            working_solenoid_valve[a - 1] = 1;
            continue;
        }
        working_solenoid_valve[i] = 0;
    }
}
int time_gap(tm now, tm set)
{ // 现在这个点和启动的时候的时差有多少?,返回分钟
    if (now.tm_hour == set.tm_hour)
    {
        if (set.tm_min >= now.tm_min)
        {
            return set.tm_min - now.tm_min;
        }
        else
        {
            return now.tm_min - set.tm_min;
        }
    }
    else if (now.tm_hour == set.tm_hour - 1)
    {
        return (60 - now.tm_min) + set.tm_min;
    }
    else if (now.tm_hour - 1 == set.tm_hour)
    {
        return (60 - set.tm_min) + now.tm_min;
    }
    else
    {
        return 24 * 60; // 代表一个很长的时间,24h*60min,即超出后面的时间区间范围
    }
}

bool time_plus_check(int wat_begin_hour, int wat_begin_min, tm timeinfo)
{                     //?这个gap不能超过30?不能让客户操作这个数值吗?
    int time_2go = 7; // 在7分钟内必须进行反应
    struct tm wat_begin;
    wat_begin.tm_hour = wat_begin_hour;
    wat_begin.tm_min = wat_begin_min;
    if (time_gap(timeinfo, wat_begin) < time_2go)
    {
        Serial2.println("reached the target timezone:Start");
        return true;
    }
    else
    {
        return false;
    }
}

// (time_by_millis 已移除 — 逻辑已内联到 get_localtime())

// DeepSeek:修改 get_localtime 函数
bool get_localtime()
{
    if (!getLocalTime(&timeinfo))
    { // 尝试获取网络时间失败
        if (!NET_LOSTING_FLAG)
        { // 首次进入断网状态
            NET_LOSTING_FLAG = true;
            NET_LOSTING_time = timeinfo; // 记录断网时的准确时间
            BEGIN_TIMESTAMP = millis();  // 记录断网时刻的 millis()
        }
        // 使用 millis() 推算当前时间
        unsigned long elapsed = millis() - BEGIN_TIMESTAMP;
        timeinfo = NET_LOSTING_time;
        timeinfo.tm_sec += elapsed / 1000;

        // 规范化时间结构
        mktime(&timeinfo); // 自动处理溢出（如秒转分钟等）
    }
    else
    {
        NET_LOSTING_FLAG = false; // 网络恢复
    }

    // 更新状态显示
    sprintf(time_temp, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    time_status = String(time_temp);
    return true;
}

// bool get_starttime(){

// }

bool time2go()
{
    if (auto_timing_watering_flag == 1)
    {
        if (work_times > 0 || soil2wat == 1)
        { // 如果手头上还有工作先做掉
            return true;
        }
        if (get_localtime())
        {
            if (pump_working_flag == 0)
            {
                Serial2.print("in ttg");
                Serial2.print(pump_working_flag);
                // if (timeinfo.tm_hour > wat_begin_hour && timeinfo.tm_min < wat_begin_min)//fixme:做多次循环的话这里还回来吗
                if (time_plus_check(wat_begin_hour, wat_begin_min, timeinfo))
                {
                    pump_working_flag = 1;
                    // *time_flag = millis();
                    if (soil2wat == 0)
                    {
                        start_work_time = timeinfo;
                        work_times = NUM_VALVES;
                        soil2wat = 1;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// 状态定义
#define STATE_IDLE 0          // 空闲状态
#define STATE_OPEN_VALVE 1     // 正在打开阀门
#define STATE_PUMP_ON 2        // 水泵已开启
#define STATE_CLOSE_PUMP 3     // 正在关闭水泵
#define STATE_CLOSE_VALVE 4    // 正在关闭阀门

// 控制参数
#define VALVE_DELAY 5000        // 阀门操作延迟时间(ms)

// 全局变量
int current_state = STATE_IDLE;
unsigned long state_start_time = 0;

void flag_execute() // 这个函数只负责给电,不负责别的操作
{
    Serial2.print("fle");
    Serial2.print(pump_working_flag);
    Serial2.print(" state:");
    Serial2.println(current_state);
    
    switch(current_state)
    {
        case STATE_IDLE:
            if (pump_working_flag == 1)
            {
                // 开始打开阀门
                current_state = STATE_OPEN_VALVE;
                state_start_time = millis();
                
                // 打开需要工作的电磁阀
                for (int i = 0; i < NUM_VALVES; i++)
                {
                    if (working_solenoid_valve[i] == 1)
                    {
                        relay_set(RELAY_CH_VALVE1 + i, true);
                        solenoid_line = i + 1; // 处理上报的正在工作的电磁阀
                        Serial2.print("HIGH");
                        Serial2.println(i);
                    }
                    else
                    {
                        relay_set(RELAY_CH_VALVE1 + i, false);
                        Serial2.print("LOW");
                        Serial2.println(i);
                    }
                }
            }
            break;
            
        case STATE_OPEN_VALVE:
            // 等待阀门完全打开
            if (millis() - state_start_time >= VALVE_DELAY)
            {
                // 打开水泵
                pump_work();
                current_state = STATE_PUMP_ON;
            }
            break;
            
        case STATE_PUMP_ON:
            if (pump_working_flag == 0)
            {
                // 开始关闭水泵
                current_state = STATE_CLOSE_PUMP;
                state_start_time = millis();
                pump_work(); // 关闭水泵
            }
            else
            {
                // 检查电磁阀状态是否需要更新
                static int last_working_valve = -1;
                static int valve_switch_state = 0; // 0: 正常状态, 1: 正在切换
                static unsigned long switch_start_time = 0;
                int current_working_valve = -1;
                
                // 找到当前工作的电磁阀
                for (int i = 0; i < NUM_VALVES; i++)
                {
                    if (working_solenoid_valve[i] == 1)
                    {
                        current_working_valve = i;
                        break;
                    }
                }
                
                // 处理电磁阀切换
                if (valve_switch_state == 0)
                {
                    // 如果电磁阀状态发生变化，开始切换过程
                    if (current_working_valve != last_working_valve && last_working_valve != -1)
                    {
                        // 同时打开两个阀门
                        relay_set(RELAY_CH_VALVE1 + last_working_valve, true);
                        if (current_working_valve != -1)
                        {
                            relay_set(RELAY_CH_VALVE1 + current_working_valve, true);
                            solenoid_line = current_working_valve + 1; // 处理上报的正在工作的电磁阀
                        }
                        else
                        {
                            solenoid_line = 0;
                        }
                        
                        // 开始切换计时
                        valve_switch_state = 1;
                        switch_start_time = millis();
                        Serial2.println("开始切换电磁阀");
                    }
                    else if (current_working_valve != last_working_valve)
                    {
                        // 首次启动或从无到有，直接打开新阀门
                        // 关闭所有电磁阀
                        for (int i = 0; i < NUM_VALVES; i++)
                        {
                            relay_set(RELAY_CH_VALVE1 + i, false);
                        }
                        
                        // 打开当前工作的电磁阀
                        if (current_working_valve != -1)
                        {
                            relay_set(RELAY_CH_VALVE1 + current_working_valve, true);
                            solenoid_line = current_working_valve + 1; // 处理上报的正在工作的电磁阀
                            Serial2.print("HIGH");
                            Serial2.println(current_working_valve);
                        }
                        else
                        {
                            solenoid_line = 0;
                        }
                        
                        last_working_valve = current_working_valve;
                    }
                }
                else
                {
                    // 等待切换延迟时间
                    if (millis() - switch_start_time >= VALVE_DELAY)
                    {
                        // 关闭之前的阀门
                        relay_set(RELAY_CH_VALVE1 + last_working_valve, false);
                        Serial2.print("LOW");
                        Serial2.println(last_working_valve);
                        
                        // 完成切换
                        valve_switch_state = 0;
                        last_working_valve = current_working_valve;
                        Serial2.println("电磁阀切换完成");
                    }
                }
            }
            break;
            
        case STATE_CLOSE_PUMP:
            // 等待水泵完全停止
            if (millis() - state_start_time >= VALVE_DELAY)
            {
                // 开始关闭阀门
                current_state = STATE_CLOSE_VALVE;
                state_start_time = millis();
                solenoid_line = 0;

                // 关闭所有电磁阀
                for (int i = 0; i < NUM_VALVES; i++)
                {
                    relay_set(RELAY_CH_VALVE1 + i, false);
                }
            }
            break;
            
        case STATE_CLOSE_VALVE:
            // 等待阀门完全关闭
            if (millis() - state_start_time >= VALVE_DELAY)
            {
                // 回到空闲状态
                current_state = STATE_IDLE;
            }
            break;
    }
}
float getTemp(String temp)
{
    int commaPosition = -1;
    String info[9]; // 用字符串数组存储
    for (int i = 0; i < 9; i++)
    {
        commaPosition = temp.indexOf(',');
        if (commaPosition != -1)
        {
            info[i] = temp.substring(0, commaPosition);
            temp = temp.substring(commaPosition + 1, temp.length());
        }
        else
        {
            if (temp.length() > 0)
            { // 最后一个会执行这个
                info[i] = temp.substring(0, commaPosition);
            }
        }
    }
    return (info[3].toInt() * 256 + info[4].toInt()) / 10.00; ////这里传回的是一个整数,我们需要小数
}

void soil_moisture_into_list(float soil_m)
{
    // 将土壤湿度写入到数组中,防止其对土壤变化太敏感
    // 但是会导致你把传感器拔出来之后,仍然一直有显示土壤湿度:正确的应该是把检测器放到水里
    if (0 != soil_m) // 这里不只是等于0要排除,而且数值超过10%都要排除!!!!
    {
        if (soil_moisture_list_size < soil_moisture_test_maxsize)
        {
            soil_moisture_list_size += 1;
        }
        soil_moisture = (soil_moisture * (soil_moisture_list_size - 1)) / soil_moisture_list_size + soil_m / soil_moisture_list_size;
    }
}

void check_soil() // 检测土壤湿度
{
    // delay(500); // 放慢输出频率
    for (int i = 0; i < 8; i++)
    {                              // 发送测温命令
        Serial.write(item[i]); // write输出
        // Serial.println("正在写入TX数据");
        // Serial.println(item[i]);
    }
    delay(100); // 等待测温数据返回
    data_soil = "";
    while (Serial.available())
    { // 从串口中读取数据
        // 这里读不出数据
        unsigned char in = (unsigned char)Serial.read(); // read读取
        // Serial.print(in, HEX);
        // Serial.print(',');
        data_soil += in;
        data_soil += ',';
    }

    if (data_soil.length() > 0)
    { // 先输出一下接收到的数据
        // Serial.println();
        // Serial.println(data);
        soil_moisture_into_list(getTemp(data_soil));
        Serial2.print(soil_moisture);
        Serial2.println("%water");
    }
}

////需要再写一个判断土壤湿度是否适宜的程序,也从网上读取参数
bool soil_go()
{
    check_soil();
    if (soil2wat == 1 || work_times > 0)
    {                // 表示目前水还在浇水
        return true; //?这样写对吗
    }
    if (soil_moisture_need > soil_moisture && auto_soil_watering_flag == 1 && soil_moisture != 0) // 还要判断一下土壤湿度是不是没有正常返回回来
    {
        pump_working_flag = 1;
        // *time_flag = millis();
        if (soil2wat == 0)
        {
            work_times = NUM_VALVES;
            soil2wat = 1;
            start_work_time = timeinfo;
        }
        Serial2.println("2");
        return true;
    }
    else
    {
        // pump_working_flag = 0;这样写有很大的问题!
        Serial2.println("3");
        return false;
    }
}

bool go_watering()
{
    time_to_go_flag = time2go();
    soil_to_go_flag = soil_go();

    if (time_to_go_flag && auto_timing_watering_flag)
    { // 时间到了
        return true;
    }
    else if (soil_to_go_flag && auto_soil_watering_flag)
    { // 干湿度达标了
        return true;
    }
    else if (1 == hand_watering_flag)
    {
        return true; // 收到了手动浇水的指令
    }
    else
    {
        return false;
    }
}

void shut_all()
{ // 紧急停止: 立即关闭所有继电器 + 重置所有标志
    pump_working_flag = 0;
    soil2wat = 0;
    Solenoid_OffAll(0);
    relay_all_off();  // 硬件级别立即关闭所有继电器
    Serial2.println("shut_all()被执行了");
    work_times = 0;
    net_solenoid_flag = 0; // 重置网络下发电磁阀标志
}

// 再写一个把正在工作的电磁阀放到一个数组中收集好的函数
// 要写一个多少时间后就自动关闭的功能，以方万一啊忘记关了
// ↑ done

bool lower_noise(int pin_to_listen, int pin_status_wanted)
{
    int score = 0;
    int test_times = 10; // 总共测试次数

    for (int i = 0; i < test_times; i++)
    {
        trigger_pin_status = digitalRead(pin_to_listen);
        // Serial.print("此处的引脚电平");
        Serial2.print(trigger_pin_status);
        if (trigger_pin_status == pin_status_wanted)
        { // 由于input_pullup的出现,导致了相关行为的反转!!!
            score = score + 1;
        }
        delay(77); // 这里需要采样吗？
    }
    if (score > 9)
    { // 这里修改得分！！！
        Serial2.println("启动");
        return true;
    }
    else
    {
        Serial2.println("未启动");
        return false;
    }
}
// 关闭此功能
//  void physical_listener()
//  { // 指对现实世界的开关动作进行追踪+降低噪音
//      // trigger_pin_status = digitalRead(car_wash_trigger_pin);
//      Serial2.print("carwash电平");
//      // Serial.println(trigger_pin_status);

//     if (lower_noise(car_wash_trigger_pin, HIGH))
//     {
//         Serial2.println("高电平洗车启动");
//         shut_all();
//         carwash_flag = 1;
//         Serial2.print("carwash_flag=");
//         Serial2.println(carwash_flag);
//         Solenoid_OffAll(0);    // 无论如何，都把所有的都给关上
//         pump_working_flag = 1; // do not use 'delay'
//         // carwash应该启动的时候禁止使用别的浇水!
//         start_work_time = timeinfo;
//     }

//     // trigger_pin_status = digitalRead(hand_water_trigger_pin);
//     Serial2.print("手动浇水电平");
//     // Serial.println(trigger_pin_status);
//     if (lower_noise(hand_water_trigger_pin, HIGH))
//     {
//         shut_all();
//         pump_working_flag = 1;
//         hand_watering_flag = 1;
//         start_work_time = timeinfo;
//         work_times = 7;
//     }
//     // //这里的代码在没有得到田里的参数时候不能测,会有问题
//     // //是否在执行层,只对pin的好低电平进行检测,而代表flag

//     Serial2.print("菜地电平");
//     if (lower_noise(vegetable_knob_pin, HIGH))
//     { // 由于input_pullup的出现,导致了相关行为的反转!!!
//         vegetable_flag_hand = 1;
//         pump_working_flag = 1; // 打开泵,记录flag
//         Solenoid_OffAll(7);
//         start_work_time = timeinfo;
//     }
//     else
//     {
//         if (1 == vegetable_flag_hand)
//         {
//             shut_all();
//         }
//         vegetable_flag_hand = 0;
//     }
// }

// 从NVS读取配置
void loadConfig()
{
    Preferences prefs;
    prefs.begin("homefront", true);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("password", "");
    device_id = prefs.getString("device_id", "");
    upUrl = prefs.getString("upUrl", "");
    prefs.end();
    Serial2.println("loadConfig done");
    if (ssid.length() > 0)
    {
        Serial2.print("ssid: ");
        Serial2.println(ssid);
    }
    else
    {
        Serial2.println("no saved ssid, need config portal");
    }
}

// 启动配置门户：AP模式 + HTTP服务器提供网页配置
void startConfigPortal()
{
    Serial2.println("startConfigPortal: no saved WiFi config, starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Homefront-Setup");
    Serial2.print("AP IP: ");
    Serial2.println(WiFi.softAPIP());

    WiFiServer server(80);
    server.begin();
    Serial2.println("HTTP config server started on port 80");

    while (true)
    {
        WiFiClient client = server.available();
        if (!client)
        {
            delay(10);
            continue;
        }

        String request = "";
        unsigned long timeout = millis() + 5000;
        while (client.connected() && millis() < timeout)
        {
            if (client.available())
            {
                char c = client.read();
                request += c;
                if (request.endsWith("\r\n\r\n"))
                    break;
            }
        }

        if (request.indexOf("POST /save") >= 0)
        {
            // 解析POST body
            String body = "";
            int bodyStart = request.indexOf("\r\n\r\n");
            if (bodyStart >= 0)
                body = request.substring(bodyStart + 4);

            String newSsid = "", newPass = "", newDeviceId = "", newUpUrl = "";
            int pos = 0;
            while (pos < body.length())
            {
                int eqPos = body.indexOf('=', pos);
                int ampPos = body.indexOf('&', pos);
                if (ampPos == -1)
                    ampPos = body.length();
                if (eqPos >= 0 && eqPos < ampPos)
                {
                    String key = body.substring(pos, eqPos);
                    String value = body.substring(eqPos + 1, ampPos);
                    value.replace("+", " ");
                    value.replace("%25", "%");
                    value.replace("%21", "!");
                    value.replace("%23", "#");
                    value.replace("%24", "$");
                    value.replace("%26", "&");
                    value.replace("%27", "'");
                    value.replace("%28", "(");
                    value.replace("%29", ")");
                    value.replace("%2A", "*");
                    value.replace("%2B", "+");
                    value.replace("%2C", ",");
                    value.replace("%2F", "/");
                    value.replace("%3A", ":");
                    value.replace("%3B", ";");
                    value.replace("%3D", "=");
                    value.replace("%3F", "?");
                    value.replace("%40", "@");

                    if (key == "ssid")
                        newSsid = value;
                    else if (key == "password")
                        newPass = value;
                    else if (key == "device_id")
                        newDeviceId = value;
                    else if (key == "upUrl")
                        newUpUrl = value;
                }
                pos = ampPos + 1;
            }

            // 保存到NVS
            if (newSsid.length() > 0 || newPass.length() > 0 || newDeviceId.length() > 0 || newUpUrl.length() > 0)
            {
                Preferences prefs;
                prefs.begin("homefront", false);
                if (newSsid.length() > 0)
                    prefs.putString("ssid", newSsid);
                if (newPass.length() > 0)
                    prefs.putString("password", newPass);
                if (newDeviceId.length() > 0)
                    prefs.putString("device_id", newDeviceId);
                if (newUpUrl.length() > 0)
                    prefs.putString("upUrl", newUpUrl);
                prefs.end();
            }

            String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
            resp += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Saved</title>";
            resp += "<meta http-equiv='refresh' content='3;url=/'></head><body>";
            resp += "<h1>Config Saved! Restarting...</h1></body></html>";
            client.print(resp);
            client.stop();

            delay(2000);
            ESP.restart();
        }
        else
        {
            String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
            html += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Homefront Setup</title>";
            html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
            html += "<style>body{font-family:Arial;max-width:400px;margin:20px auto;padding:20px;}";
            html += "input{width:100%;padding:8px;margin:5px 0;box-sizing:border-box;}";
            html += "input[type=submit]{background:#4CAF50;color:#fff;border:none;padding:12px;font-size:16px;}</style></head><body>";
            html += "<h2>Homefront Config Portal</h2>";
            html += "<form method='POST' action='/save'>";
            html += "<label>WiFi SSID:</label><input name='ssid' value='" + ssid + "' required>";
            html += "<label>WiFi Password:</label><input name='password' type='password' value='" + password + "'>";
            html += "<label>Device ID:</label><input name='device_id' value='" + device_id + "'>";
            html += "<label>OTA URL:</label><input name='upUrl' value='" + upUrl + "'>";
            html += "<input type='submit' value='Save & Restart'>";
            html += "</form></body></html>";
            client.print(html);
            client.stop();
        }
    }
}

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(4800);
    Serial2.begin(115200);  // 调试输出用 Serial2 (GPIO16/17, LoRa 引脚)
    configTime(8 * 3600, 0, "ntp1.aliyun.com", "ntp2.aliyun.com");
    delay(10);

    // 从NVS加载配置；如果没有WiFi凭据则启动配置门户
    loadConfig();
    if (ssid.length() == 0)
    {
        startConfigPortal();
        return;
    }

    relay_init();  // 初始化 74HC595，所有继电器默认关闭
    // pinMode(car_wash_trigger_pin, INPUT);
    // pinMode(hand_water_trigger_pin, INPUT);
    // pinMode(vegetable_knob_pin, INPUT);
    // pinMode(car_wash_trigger_pin, INPUT_PULLDOWN);
    // pinMode(hand_water_trigger_pin, INPUT_PULLDOWN);
    // pinMode(vegetable_knob_pin, INPUT_PULLDOWN);
    // 取消此功能
    //  若在设置种设置按钮输入功能打开,设置输入上拉
    //  if(physical_buttons){
    //  pinMode(car_wash_trigger_pin, INPUT_PULLUP);
    //  pinMode(hand_water_trigger_pin, INPUT_PULLUP);
    //  pinMode(vegetable_knob_pin, INPUT_PULLUP);
    //  }

    // todo:以后可以考虑加入一个开关，来保证esp32挂机的时候仍然可以手动启动水泵！！！！！
    // 加入时间、wifi校验显示位置
    //  We start by connecting to a WiFi network

    Serial2.println();
    Serial2.println();
    Serial2.print("Connecting to ");
    Serial2.println(ssid);

    WiFi.begin(ssid.c_str(), password.c_str());
    wifi_reconnect_cx();
    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     delay(500);
    //     Serial2.print(".");
    // }
    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     wifi_retry_times += 1;
    //     if (wifi_retry_times >= 0 && wifi_retry_times <150)
    //     {
    //         // WiFi.begin(ssid, password);
    //         Serial2.print(".");
    //     }
    //     else
    //     {
    //         Serial2.println("准备重启");
    //         ESP.restart();
    //     }
    //     delay(300);
    // }
    wifi_retry_times = 0;
    Serial2.println("");
    Serial2.println("WiFi connected");
    Serial2.println("IP address: ");
    Serial2.println(WiFi.localIP());
    Serial2.print("connecting to ");
    Serial2.print(host);
    Serial2.print(':');
    Serial2.println(httpPort);
    // init and get the time
    // Use WiFiClient class to create TCP connections
    client.print("执行第一次空中升级,我爱你,就像你爱我一样");
    if (!client.connect(host, httpPort))
    {
        Serial2.println("connection failed");
        delay(5000);
    }
    else
    {
        Serial2.println("connection sucess!");
        tk.attach(40, time_fun); // s,中断服务函数，告诉你中断之后要跑到哪里去工作
        Serial2.println("send device_id");
        client.print(device_id);
    } // 设置这个中断而不是用delay的好处在于，中断之后改变flag，达到处理的目的
    // 同时，对于整个函数，还是处于监听的状态，而不是像delay一样全部都停止了
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
    // todo:1.将代码移植到带天线的esp32上
    // 2.增加几个开关,用于离线控制
    wifi_reconnect_cx();
    check_client_connected();
    get_localtime();
    // Serial.println(pump_working_flag);
    // Serial.println(type(pump_working_flag));
    if (1 == breakpoint_flag)
    {
        if (client.connected())
        {
            // Serial.println("send device_id");
            // client.print(device_id); /*指定设备id*/
            //                         //这个函数指的不是print，而是发送！
            Serial2.println("send heart beat sense");
            client.print("q");
        }
        breakpoint_flag = 0;
    }
    if (client.available()) // 别把语句写到if与else这些东西之间
    {                       /*接收数据，client.available()这个函数是指服务器有数据回传*/
        Serial2.print("available\n");
        String ch = client.readString();
        Serial2.println(ch);
        if (0 == ch.compareTo("A")) // 直接相等好像不大可以的
        {
            Serial2.println("heart beat check\n");
        }
        else if (ch.lastIndexOf('%') != -1)
        { // 读取你所需要的最低湿度
            Serial2.println("lowest wet check\n");
            soil_moisture_need = ch.substring(0, ch.length() - 1).toFloat();
        }
        else if (ch.lastIndexOf('x') != -1)
        { //! 读取你所需要的最少时间,小心字母重复!
            // 输入分钟数量+x,如:25x
            Serial2.println("least time check\n");
            for (int i = 0; i < NUM_VALVES - 1; i++)
            { // 只修改数组的前几个，不修改最后一个代表浇菜的引脚
                pin_watering_time[i] = ch.substring(0, ch.length() - 1).toInt();
            }
        }
        else if (ch.lastIndexOf('v') != -1)
        { // 应当输入5v31
            Serial2.println("began time check\n");
            wat_begin_hour = fenge(ch, "v", 0).toInt();
            wat_begin_min = fenge(ch, "v", 1).toInt();
        }
        else if (ch.toInt() <= NUM_VALVES && ch.toInt() >= 1) // 如果是数字的话，就开启的debug模式
        {
            Serial2.println("which pump check\n");
            pump_working_flag = 1; // 打开泵,记录flag
            Solenoid_OffAll(ch.toInt());
            soil2wat = 1; // 设置浇水标志，确保WiFi断开后能继续浇水
            start_work_time = timeinfo; // 记录开始时间
            work_times = 1; // 单个电磁阀模式，只浇一轮
            net_solenoid_flag = 1; // 设置网络下发标志，启用10分钟超时保护
            // Serial.print(working_solenoid_valve[0]);
            // Serial.print(working_solenoid_valve[1]);
            // Serial.println(working_solenoid_valve[2]);
            // Serial.print("blo");
            // Serial.print(pump_working_flag);
        } //
        // else if (0 == ch.toInt())
        // {
        //     shut_all();
        // }
        else
        {
            DeserializationError error = deserializeJson(doc, ch); // 将数据ch解析到doc中，然后在最后返回是解析成功与否
            if (error)
            {
                Serial2.println("解析错误！");
            }
            else if (doc.containsKey("carwash"))
            { // 得到洗车的信号
                shut_all();
                carwash_flag = doc["carwash"];

                Serial2.print("carwash_flag=");
                Serial2.println(carwash_flag);

                Solenoid_OffAll(0);    // 无论如何，都把所有的都给关上
                pump_working_flag = 1; // do not use 'delay'
                if (carwash_flag == 1) ////carwash应该启动的时候禁止使用别的浇水!
                {
                    // *time_flag = millis();
                    start_work_time = timeinfo;
                }
                else
                {
                    carwash_flag = 0;
                    shut_all();
                }

                /////////////////////////////////////////
            }
            else if (doc.containsKey("auto_soil"))
            {
                auto_soil_watering_flag = doc["auto_soil"]; // 设置好之后等待定时就好了
                if (auto_soil_watering_flag == 0)
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("auto_timing"))
            {
                auto_timing_watering_flag = doc["auto_timing"]; // 设置好之后等待定时就好了
                if (auto_timing_watering_flag == 0)
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("hand"))
            {
                hand_watering_flag = doc["hand"]; // 收到这个信号的时候代表我应该马上就需要浇水了
                // Serial.print("inhand");
                // Serial.print(hand_watering_flag);
                if (hand_watering_flag == 1)
                {
                    //                     Serial2.print("cnm");
                    // Serial.print(pump_working_flag);
                    pump_working_flag = 1;
                    // *time_flag = millis();
                    start_work_time = timeinfo;
                    work_times = NUM_VALVES;
                }
                else
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("field"))
            { // 此代表菜地,所选择的英文单词中不能包含字母x或者v

                vegetable_flag_net = doc["field"];
                if (vegetable_flag_net == 1) ////carwash应该启动的时候禁止使用别的浇水!
                {
                    shut_all();
                    pump_working_flag = 1;
                    working_solenoid_valve[6] = 1; // 此时最后一个是代表田里的电磁阀
                                                   // working_solenoid_valve[NUM_VALVES - 1] 不能这么用!
                    start_work_time = timeinfo;
                }
                else
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("pool_2_wat")) // 如果数组中电磁阀又有变化,我就不再定义新变量了
            {
                pool_watering_flag = doc["pool_2_wat"];
                if (pool_watering_flag == 1) ////carwash应该启动的时候禁止使用别的浇水!
                {
                    shut_all();
                    pump_working_flag = 1;
                    working_solenoid_valve[5] = 1; // 此时最后一个是代表田里的电磁阀
                                                   // working_solenoid_valve[NUM_VALVES - 1] 不能这么用!
                    start_work_time = timeinfo;
                }
                else
                {
                    shut_all();
                }
            }
            else if (doc.containsKey("field_2_wat")) // 如果数组中电磁阀又有变化,我就不再定义新变量了
            {
                if (1 == doc["field_2_wat"])
                {
                    pin_watering_time[6] = 10;
                }
                else if (0 == doc["field_2_wat"])
                {
                    pin_watering_time[6] = 0;
                }
            }
            else if (doc.containsKey("corner_2_wat"))
            {
                if (1 == doc["corner_2_wat"])
                {
                    pin_watering_time[4] = 20;
                }
                else if (0 == doc["corner_2_wat"])
                {
                    pin_watering_time[4] = 0;
                }
            }
            // 取消此功能//////////////////
            //  else if (doc.containsKey("physical_buttons"))
            //  {
            //      if(1==doc["physical_buttons"]){
            //      physical_buttons=1;
            //      Serial2.println("上拉");
            //          pinMode(car_wash_trigger_pin, INPUT_PULLUP);
            //          pinMode(hand_water_trigger_pin, INPUT_PULLUP);
            //          pinMode(vegetable_knob_pin, INPUT_PULLUP);
            //      }
            //      else if(0==doc["physical_buttons"]){
            //      Serial2.println("上拉取消,改为输出");
            //          shut_all();
            //          physical_buttons=0;
            //              pinMode(car_wash_trigger_pin, OUTPUT);
            //              pinMode(hand_water_trigger_pin, OUTPUT);
            //              pinMode(vegetable_knob_pin, OUTPUT);
            //      }
            //  }
            else if (doc.containsKey("shut"))
            {
                shut_all();
            }
            else if (doc.containsKey("ota_upload"))
            {
                ota_status = doc["ota_upload"];
                if (1 == ota_status)
                {
                    updateBin();
                }
            }
            else if (doc.containsKey("restart"))
            {
                reboot_flag = doc["restart"];
                if (reboot_flag == 1)
                {
                    ESP.restart();
                }
            }
        }
        // 处理时间
    }
    // 这些好像和下面的代码矛盾了吧
    //// if (auto_watering_flag == 1 && time2go())
    ////     {
    ////         pump_working_flag = 1;
    ////         time_flag = millis();
    ////         work_times = 3;
    ////     }
    // if(physical_buttons)
    // {
    // physical_listener(); // 对现在的按钮进行监听！！！
    // }

    // Phase3-fix: 仅读取状态变量，不调用有副作用的函数
    Serial2.println("******************");
    Serial2.print("pump_working_flag:");
    Serial2.println(pump_working_flag);
    Serial2.print("hand_watering_flag:");
    Serial2.println(hand_watering_flag);
    Serial2.print("soil2wat:");
    Serial2.println(soil2wat);
    Serial2.println("******************");

    if (0 == carwash_flag && 0 == vegetable_flag_hand && 0 == vegetable_flag_net) // 此刻不在菜地浇水,也不在洗车
    {
        if (go_watering()) // 是时候浇水了
        // fixme:time2go可能需要再大一点；1.算好delay和time2go;2.做一个每天几次，或者上下午几次的东西
        // 这里如果auto_watering_flag==1在前面,当其为0的时候time2go()和soil_go()不会执行了,所以应该放在后面
        {
            // if(1==hand_watering_flag && auto_watering_flag==1){
            //     auto_watering_flag=999
            // }
            Serial2.println("watt");
            Serial2.print("水泵正在工作吗:");
            Serial2.println(pump_working_flag);
            if (pump_working_flag == 1)
            {
                if (work_times > 0)
                {
                    soil2wat = 1; // 代表此刻正在浇水
                    for (int i = 0; i < NUM_VALVES; i++)
                    {

                        working_solenoid_valve[i] = 0;
                    }

                    working_solenoid_valve[(NUM_VALVES - work_times)] = 1; // 这里的先打开再关闭时无所谓的，因为只是一个flag，都在最后的地方可以操作

                    // Serial.println(work_times);
                    // Serial.println('else分界线');
                }

                // mid_time = time_flag - mid_time;
                // *time_flag = millis();
                // Serial.println(*time_flag);

                Serial2.print("和开始的时间相差分钟数:");
                Serial2.println(time_gap(timeinfo, start_work_time));
                time_gap_min = time_gap(timeinfo, start_work_time);
                if (time_gap_min > pin_watering_time[(NUM_VALVES - work_times)])
                { ////这里是不是没有做减法?建议调试之后再操作这里

                    work_times = work_times - 1;
                    Serial2.println("worktimes-1了");
                    start_work_time = timeinfo;
                }
                Serial2.print("在浇倒数第几轮:");
                Serial2.println(work_times);
                if (work_times == 0) // 不能是-1否则会在最后一个引脚多执行一次
                {
                    shut_all();
                    hand_watering_flag = 0;
                    Serial2.println("worktime等于0了");
                }
            }
        }
    }
    else if (1 == carwash_flag) // 1 == carwash_flag
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > pin_watering_time[0])
        {
            shut_all();
            carwash_flag = 0;
        }
    }
    else if (1 == vegetable_flag_net || 1 == vegetable_flag_hand)
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > pin_watering_time[5])
        {
            shut_all();
            vegetable_flag_net = 0;
            vegetable_flag_hand = 0;
        }
    }
    else if (1 == net_solenoid_flag) // 网络下发数字模式，10分钟超时保护
    {
        soil2wat = 1;
        if (time_gap(timeinfo, start_work_time) > 10) // 超过10分钟自动关闭
        {
            Serial2.println("网络下发电磁阀超时10分钟，自动关闭");
            shut_all();
            net_solenoid_flag = 0;
        }
    }
    for (int i = 0; i < NUM_VALVES; i++)
    {

        Serial2.print(working_solenoid_valve[i]);
    }
    delay(100);  // Phase3-fix: check_soil() 已在 go_watering()→soil_go() 中调用
    get_localtime();
    delay(100);
    flag_execute();
    delay(100);
    send2clinet();
    delay(1000); // don't flood remote service
    // 不要用mills代替，否则感觉让他休息的目的都达不到了，就是要阻塞在这里起
}