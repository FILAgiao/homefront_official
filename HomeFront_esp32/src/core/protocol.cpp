#include "protocol.h"
#include "globals.h"
#include "ota.h"
#include "watering.h"
#include "config.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// 字符串分割函数: 按 fen 分隔 str, 取第 index 段
String fenge(String str, String fen, int index)
{
    int weizhi;
    int maxParts = 16;
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

// 拼接遥测数据并发送到 TCP 服务器
void send2clinet()
{
    sprintf(set_begin_time, "%d:%d", wat_begin_hour, wat_begin_min);

    if (1 == ota_status)
    {
        time_status = ota_feedback;
        ota_status = 0;  // 单次发送后复位, 防止 time_status 被永久覆盖
    }

    snprintf(data, sizeof(data), "#%d*%d*%d*%d*%d*%f*%d*%s*%d*%f*%d*%s*%d*%d*%d#",
             solenoid_line, carwash_flag, auto_soil_watering_flag, auto_timing_watering_flag,
             hand_watering_flag, soil_moisture, pump_working_flag, time_status.c_str(),
             reboot_flag, soil_moisture_need, pin_watering_time[0], set_begin_time,
             ota_status, 0, physical_buttons);
    Serial.print("回送的数据为：");
    Serial.println(data);
    if (client.connected())
        client.print(data);
}

// 处理收到的 TCP 消息, 分发到各命令
void handle_incoming_message(const String &ch)
{
    if (0 == ch.compareTo("A"))
    {
        Serial.println("heart beat check\n");
    }
    else if (ch.lastIndexOf('%') != -1)
    {
        Serial.println("lowest wet check\n");
        soil_moisture_need = constrain(ch.substring(0, ch.length() - 1).toFloat(), 0.0f, 100.0f);
        saveAllConfig();
    }
    else if (ch.lastIndexOf('x') != -1)
    {
        Serial.println("least time check\n");
        int v = constrain(ch.substring(0, ch.length() - 1).toInt(), 0, 120);
        for (int i = 0; i < valve_count; i++)
        {
            pin_watering_time[i] = v;
        }
        saveAllConfig();
    }
    else if (ch.lastIndexOf('v') != -1)
    {
        Serial.println("began time check\n");
        wat_begin_hour = constrain(fenge(ch, "v", 0).toInt(), 0, 23);
        wat_begin_min  = constrain(fenge(ch, "v", 1).toInt(), 0, 59);
        saveAllConfig();
    }
    else if (ch.toInt() <= valve_count && ch.toInt() >= 1)
    {
        Serial.println("which pump check\n");
        pump_working_flag = 1;
        Solenoid_OffAll(ch.toInt());
        soil2wat = 1;
        start_work_time = timeinfo;
        work_times = 1;
        net_solenoid_flag = 1;
    }
    else
    {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, ch);
        if (error)
        {
            Serial.println("解析错误！");
        }
        else if (doc.containsKey("carwash"))
        {
            shut_all();
            carwash_flag = doc["carwash"];

            Serial.print("carwash_flag=");
            Serial.println(carwash_flag);

            Solenoid_OffAll(0);
            pump_working_flag = 1;
            if (carwash_flag == 1)
            {
                start_work_time = timeinfo;
            }
            else
            {
                carwash_flag = 0;
                shut_all();
            }
        }
        else if (doc.containsKey("auto_soil"))
        {
            auto_soil_watering_flag = doc["auto_soil"];
            saveAllConfig();
            if (auto_soil_watering_flag == 0)
            {
                shut_all();
            }
        }
        else if (doc.containsKey("auto_timing"))
        {
            auto_timing_watering_flag = doc["auto_timing"];
            saveAllConfig();
            if (auto_timing_watering_flag == 0)
            {
                shut_all();
            }
        }
        else if (doc.containsKey("hand"))
        {
            hand_watering_flag = doc["hand"];
            if (hand_watering_flag == 1)
            {
                pump_working_flag = 1;
                start_work_time = timeinfo;
                work_times = valve_count;
            }
            else
            {
                shut_all();
            }
        }
        else if (doc.containsKey("field"))
        {
            vegetable_flag_net = doc["field"];
            if (vegetable_flag_net == 1)
            {
                shut_all();
                pump_working_flag = 1;
                int idx = constrain(field_valve_num - 1, 0, MAX_VALVES - 1);
                working_solenoid_valve[idx] = 1;
                start_work_time = timeinfo;
            }
            else
            {
                shut_all();
            }
        }
        else if (doc.containsKey("shut"))
        {
            shut_all();
            // 远程紧急停止: 关闭所有自动模式, 防止立即重新触发
            auto_timing_watering_flag = 0;
            auto_soil_watering_flag = 0;
            carwash_flag = 0;
            vegetable_flag_hand = 0;
            vegetable_flag_net = 0;
            hand_watering_flag = 0;
            saveAllConfig();
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
        else if (doc.containsKey("factory_reset"))
        {
            if (doc["factory_reset"] == 1)
            {
                shut_all();
                Preferences p; p.begin("homefront", false);
                p.clear(); p.end();
                delay(200);
                ESP.restart();
            }
        }
    }
}
