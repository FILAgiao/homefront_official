#include "watering.h"
#include "globals.h"
#include "sensor.h"
#include "../pinmap.h"
#include "../hal/relay.h"

// ============================================================
// 时间工具
// ============================================================

bool get_localtime()
{
    if (!getLocalTime(&timeinfo))
    {
        if (!NET_LOSTING_FLAG)
        {
            NET_LOSTING_FLAG = true;
            NET_LOSTING_time = timeinfo;
            BEGIN_TIMESTAMP = millis();
        }
        unsigned long elapsed = millis() - BEGIN_TIMESTAMP;
        timeinfo = NET_LOSTING_time;
        timeinfo.tm_sec += elapsed / 1000;
        mktime(&timeinfo);
    }
    else
    {
        NET_LOSTING_FLAG = false;
    }

    sprintf(time_temp, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    time_status = String(time_temp);
    return true;
}

int time_gap(tm now, tm set)
{
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
        return 24 * 60;
    }
}

bool time_plus_check(int wat_begin_hour, int wat_begin_min, tm timeinfo)
{
    int time_2go = 7;
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

// ============================================================
// 浇水决策
// ============================================================

bool time2go()
{
    if (auto_timing_watering_flag == 1)
    {
        if (work_times > 0 || soil2wat == 1)
        {
            return true;
        }
        if (get_localtime())
        {
            if (pump_working_flag == 0)
            {
                Serial2.print("in ttg");
                Serial2.print(pump_working_flag);
                if (time_plus_check(wat_begin_hour, wat_begin_min, timeinfo))
                {
                    pump_working_flag = 1;
                    if (soil2wat == 0)
                    {
                        start_work_time = timeinfo;
                        work_times = valve_count;
                        soil2wat = 1;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool soil_go()
{
    check_soil();
    if (soil2wat == 1 || work_times > 0)
    {
        return true;
    }
    if (soil_moisture_need > soil_moisture && auto_soil_watering_flag == 1 && soil_moisture != 0)
    {
        pump_working_flag = 1;
        if (soil2wat == 0)
        {
            work_times = valve_count;
            soil2wat = 1;
            start_work_time = timeinfo;
        }
        Serial2.println("2");
        return true;
    }
    else
    {
        Serial2.println("3");
        return false;
    }
}

bool go_watering()
{
    time_to_go_flag = time2go();
    soil_to_go_flag = soil_go();

    if (time_to_go_flag && auto_timing_watering_flag)
    {
        return true;
    }
    else if (soil_to_go_flag && auto_soil_watering_flag)
    {
        return true;
    }
    else if (1 == hand_watering_flag)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// ============================================================
// 阀门/水泵控制
// ============================================================

void pump_work()
{
    for (int i = 0; i < pump_count; i++)
    {
        relay_set(RELAY_CH_PUMP + i, pump_working_flag == 1);
    }
}

void Solenoid_OffAll(int a)  // a=0 全关; a=1~7 只开第 a 路
{
    solenoid_line = a;
    hand_watering_flag = 0;
    for (int i = 0; i < valve_count; i++)
    {
        if (a != 0 && i == a - 1)
        {
            working_solenoid_valve[a - 1] = 1;
            continue;
        }
        working_solenoid_valve[i] = 0;
    }
}

void shut_all()
{
    pump_working_flag = 0;
    soil2wat = 0;
    Solenoid_OffAll(0);
    relay_all_off();  // 硬件级别立即关闭所有继电器
    Serial2.println("shut_all()被执行了");
    work_times = 0;
    net_solenoid_flag = 0;
}

// ============================================================
// 5 状态机执行器: 阀门 → 水泵 → 关泵 → 关阀
// ============================================================

void flag_execute()
{
    Serial2.print("fle");
    Serial2.print(pump_working_flag);
    Serial2.print(" state:");
    Serial2.println(current_state);

    switch (current_state)
    {
    case STATE_IDLE:
        if (pump_working_flag == 1)
        {
            current_state = STATE_OPEN_VALVE;
            state_start_time = millis();

            for (int i = 0; i < valve_count; i++)
            {
                if (working_solenoid_valve[i] == 1)
                {
                    relay_set(RELAY_CH_VALVE1 + i, true);
                    solenoid_line = i + 1;
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
        if (millis() - state_start_time >= VALVE_DELAY)
        {
            pump_work();
            current_state = STATE_PUMP_ON;
        }
        break;

    case STATE_PUMP_ON:
        if (pump_working_flag == 0)
        {
            current_state = STATE_CLOSE_PUMP;
            state_start_time = millis();
            pump_work();
        }
        else
        {
            static int last_working_valve = -1;
            static int valve_switch_state = 0;
            static unsigned long switch_start_time = 0;
            int current_working_valve = -1;

            for (int i = 0; i < valve_count; i++)
            {
                if (working_solenoid_valve[i] == 1)
                {
                    current_working_valve = i;
                    break;
                }
            }

            if (valve_switch_state == 0)
            {
                if (current_working_valve != last_working_valve && last_working_valve != -1)
                {
                    relay_set(RELAY_CH_VALVE1 + last_working_valve, true);
                    if (current_working_valve != -1)
                    {
                        relay_set(RELAY_CH_VALVE1 + current_working_valve, true);
                        solenoid_line = current_working_valve + 1;
                    }
                    else
                    {
                        solenoid_line = 0;
                    }

                    valve_switch_state = 1;
                    switch_start_time = millis();
                    Serial2.println("开始切换电磁阀");
                }
                else if (current_working_valve != last_working_valve)
                {
                    for (int i = 0; i < valve_count; i++)
                    {
                        relay_set(RELAY_CH_VALVE1 + i, false);
                    }

                    if (current_working_valve != -1)
                    {
                        relay_set(RELAY_CH_VALVE1 + current_working_valve, true);
                        solenoid_line = current_working_valve + 1;
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
                if (millis() - switch_start_time >= VALVE_DELAY)
                {
                    relay_set(RELAY_CH_VALVE1 + last_working_valve, false);
                    Serial2.print("LOW");
                    Serial2.println(last_working_valve);

                    valve_switch_state = 0;
                    last_working_valve = current_working_valve;
                    Serial2.println("电磁阀切换完成");
                }
            }
        }
        break;

    case STATE_CLOSE_PUMP:
        if (millis() - state_start_time >= VALVE_DELAY)
        {
            current_state = STATE_CLOSE_VALVE;
            state_start_time = millis();
            solenoid_line = 0;

            for (int i = 0; i < valve_count; i++)
            {
                relay_set(RELAY_CH_VALVE1 + i, false);
            }
        }
        break;

    case STATE_CLOSE_VALVE:
        if (millis() - state_start_time >= VALVE_DELAY)
        {
            current_state = STATE_IDLE;
        }
        break;
    }
}
