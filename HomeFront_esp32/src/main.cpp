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
#include <esp_task_wdt.h>

#include "pinmap.h"
#include "hal/relay.h"
#include "hal/oled.h"
#include "core/globals.h"
#include "core/config.h"
#include "core/network.h"
#include "core/protocol.h"
#include "core/watering.h"
#include "core/sensor.h"
#include "core/ota.h"
#include "ui/menu.h"

// ============================================================
// LED 状态更新 (U9 Q0~Q3, 595 bit 8~11)
// LED1=WiFi, LED2=TCP, LED3=浇水活动, LED4=告警预留
// ============================================================
static void update_status_leds()
{
    bool wifi_ok = (WiFi.status() == WL_CONNECTED);
    bool tcp_ok = client.connected();
    bool watering = (soil2wat == 1);

    led_set(1, wifi_ok);
    led_set(2, tcp_ok);
    led_set(3, watering);
    led_set(4, false);  // 预留告警指示灯
}

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
char data[256];
String time_status = "";
int ota_status = 0;
String ota_feedback = "";

// ---- 物理按钮 (预留, 未来接硬件按钮后更新) ----
int physical_buttons = 0;

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
    Serial.begin(115200);                        // USB 调试输出 (CH340)
    // RS485 共用 UART0 (GPIO1/3), 仅在 check_soil() 中临时切换波特率到 4800
    delay(500);  // 等待串口监视器连接

    // 硬核启动横幅, 防止被 Modbus 数据淹没
    Serial.println("\n\n\n");
    Serial.println("===================================");
    Serial.println("  HomeFront 启动诊断");
    Serial.println("===================================");

    Serial.println("[BOOT] setup start");
    configTime(8 * 3600, 0, "ntp1.aliyun.com", "ntp2.aliyun.com");
    delay(10);

    Serial.println("[BOOT] loading config...");
    loadConfig();
    if (ssid.length() == 0)
    {
        Serial.println("[BOOT] entering AP mode...");
        startConfigPortal();
        Serial.println("[BOOT] AP mode done");
    }

    Serial.println("[BOOT] relay init...");
    relay_init();
    Serial.println("[BOOT] menu init (OLED)...");
    menu_init();  // OLED + 编码器 + 按键
    Serial.println("[BOOT] menu init done");

    // 启用硬件看门狗 (10 秒超时)
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid.c_str(), password.c_str());

    // 等待 WiFi 连接 (期间保持 UI 响应, 最多等 15 秒)
    {
        const char *lines[] = {"WiFi连接中..."};
        oled_draw_page("HomeFront", lines, 1, -1, false);
    }
    {
        unsigned long wifi_start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifi_start < 15000)
        {
            menu_tick();
            esp_task_wdt_reset();
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        char ip_line[22];
        snprintf(ip_line, sizeof(ip_line), "IP: %s", WiFi.localIP().toString().c_str());
        const char *lines[] = {"WiFi已连接", ip_line};
        oled_draw_page("HomeFront", lines, 2, -1, false);

        // 短暂展示结果, 保持 UI 响应
        unsigned long done_ms = millis();
        while (millis() - done_ms < 2000)
        {
            menu_tick();
            esp_task_wdt_reset();
        }
    }
    else
    {
        Serial.println("WiFi not connected, will retry in loop");
        char ssid_line[22], pass_line[22];
        snprintf(ssid_line, sizeof(ssid_line), "WiFi: %s", ssid.c_str());
        int plen = password.length();
        if (plen <= 2)
            snprintf(pass_line, sizeof(pass_line), "密码: %s", password.c_str());
        else
            snprintf(pass_line, sizeof(pass_line), "密码: %c***%c",
                     password.charAt(0), password.charAt(plen - 1));
        const char *lines[] = {"WiFi连接失败", ssid_line, pass_line, "检查后重启设备"};
        oled_draw_page("HomeFront", lines, 4, -1, false);

        unsigned long done_ms = millis();
        while (millis() - done_ms < 3000)
        {
            menu_tick();
            esp_task_wdt_reset();
        }
    }

    // TCP 连接交给 loop() 里的 check_client_connected() 处理, 不在 setup 里阻塞
}

void loop()
{
    // 喂硬件看门狗
    esp_task_wdt_reset();

    // UI 每次迭代都更新, 保证响应灵敏
    menu_tick();

    // === GPIO 诊断: 每 2 秒打印按键/编码器/候选引脚原始电平 ===
    {
        static unsigned long last_diag = 0;
        if (millis() - last_diag >= 2000)
        {
            last_diag = millis();
            int k1 = digitalRead(PIN_KEY1);      // GPIO4
            int k2 = digitalRead(PIN_KEY2);      // GPIO27
            int k3 = digitalRead(PIN_KEY3);      // GPIO33
            int ea = digitalRead(PIN_EC11_A);    // GPIO19 (✅)
            int eb = digitalRead(PIN_EC11_B);    // GPIO18 (✅)
            int ek = digitalRead(PIN_EC11_KEY);  // GPIO32 (✅)
            Serial.print("BTN: K1(4)="); Serial.print(k1);
            Serial.print(" K2(27)="); Serial.print(k2);
            Serial.print(" K3(33)="); Serial.print(k3);
            Serial.print(" EA(19)="); Serial.print(ea);
            Serial.print(" EB(18)="); Serial.print(eb);
            Serial.print(" EK(32)="); Serial.println(ek);
        }
    }

    // === 周期性工作 (1 秒间隔) ===
    static unsigned long last_cycle = 0;
    if (millis() - last_cycle >= 1000)
    {
        last_cycle = millis();

        wifi_reconnect_cx();
        check_client_connected();
        update_status_leds();
        get_localtime();

        // 每 2 小时重试 NTP 同步, 防止 millis() 漂移
        static unsigned long last_ntp_sync = 0;
        if (millis() - last_ntp_sync > 7200000)
        {
            last_ntp_sync = millis();
            configTime(8 * 3600, 0, "ntp1.aliyun.com", "ntp2.aliyun.com");
        }

        if (1 == breakpoint_flag)
        {
            if (client.connected())
            {
                Serial.println("send heart beat sense");
                client.print("q");
            }
            breakpoint_flag = 0;
        }

        if (client.available())
        {
            Serial.print("available\n");
            String ch = client.readString();
            Serial.println(ch);
            handle_incoming_message(ch);
        }

        Serial.println("******************");
        Serial.print("pump_working_flag:");
        Serial.println(pump_working_flag);
        Serial.print("hand_watering_flag:");
        Serial.println(hand_watering_flag);
        Serial.print("soil2wat:");
        Serial.println(soil2wat);
        Serial.println("******************");

        // 浇水逻辑 (网络单阀命令不参与自动浇水调度)
        if (0 == carwash_flag && 0 == vegetable_flag_hand && 0 == vegetable_flag_net && 0 == net_solenoid_flag)
        {
            if (go_watering())
            {
                Serial.println("watt");
                Serial.print("水泵正在工作吗:");
                Serial.println(pump_working_flag);
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

                    Serial.print("和开始的时间相差分钟数:");
                    Serial.println(time_gap(timeinfo, start_work_time));
                    time_gap_min = time_gap(timeinfo, start_work_time);
                    bool time_exceed = time_gap_min > pin_watering_time[(valve_count - work_times)];
                    if (time_exceed)
                    {
                        work_times = work_times - 1;
                        Serial.println("worktimes-1了");
                        start_work_time = timeinfo;
                    }
                    Serial.print("在浇倒数第几轮:");
                    Serial.println(work_times);
                    if (work_times == 0)
                    {
                        shut_all();
                        hand_watering_flag = 0;
                        Serial.println("worktime等于0了");
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
                Serial.println("网络下发电磁阀超时10分钟，自动关闭");
                shut_all();
                net_solenoid_flag = 0;
            }
        }

        for (int i = 0; i < valve_count; i++)
        {
            Serial.print(working_solenoid_valve[i]);
        }
        flag_execute();

        // 每 5 秒发送一次遥测 (每 1 秒太频繁, 给服务器和网络减压)
        {
            static int telemetry_div = 0;
            telemetry_div++;
            if (telemetry_div >= 5)
            {
                telemetry_div = 0;
                send2clinet();
            }
        }
    }

    delay(1);  // 让出 CPU 给 RTOS
}
