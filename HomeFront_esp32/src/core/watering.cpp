#include "watering.h"
#include "globals.h"
#include "sensor.h"
#include "../pinmap.h"
#include "../hal/relay.h"

// 逻辑阀门号 (0~5) → 物理 595 通道 (以网表为准)
static const uint8_t valve_ch[MAX_VALVES] = {
    HC595_CH_VALVE1,   // 阀门1 → CH0 (RLY1, 24V)
    HC595_CH_VALVE2,   // 阀门2 → CH3 (RLY2, 24V)
    HC595_CH_VALVE3,   // 阀门3 → CH4 (RLY3, 24V)
    HC595_CH_VALVE4,   // 阀门4 → CH5 (RLY4, 12V)
    HC595_CH_VALVE5,   // 阀门5 → CH6 (RLY5, 12V)
    HC595_CH_VALVE6    // 阀门6 → CH7 (RLY6, 12V)
};

// 逻辑水泵号 (0~1) → 物理 595 通道 (以网表为准)
static const uint8_t pump_ch[MAX_PUMPS] = {
    HC595_CH_PUMP1,    // 水泵1 → CH2 (RLY7, AC)
    HC595_CH_PUMP2     // 水泵2 → CH1 (RLY8, AC)
};

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
        int total_secs = NET_LOSTING_time.tm_hour * 3600
                       + NET_LOSTING_time.tm_min * 60
                       + NET_LOSTING_time.tm_sec
                       + (int)(elapsed / 1000);
        // 圆环归一化到 0..86399, 不依赖 mktime()
        total_secs %= 24 * 3600;
        if (total_secs < 0) total_secs += 24 * 3600;
        timeinfo.tm_hour = total_secs / 3600;
        timeinfo.tm_min  = (total_secs % 3600) / 60;
        timeinfo.tm_sec  = total_secs % 60;
    }
    else
    {
        NET_LOSTING_FLAG = false;
    }

    sprintf(time_temp, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    time_status = String(time_temp);
    return true;
}

// 计算两个 tm 结构之间的圆环分钟距离 (取较短方向)
// 正确处理跨天边界, 不依赖 mktime()
int time_gap(tm now, tm set)
{
    int now_mins = now.tm_hour * 60 + now.tm_min;
    int set_mins = set.tm_hour * 60 + set.tm_min;

    // 同一小时 — 快速路径 (保留原逻辑, 保持行为一致)
    if (now.tm_hour == set.tm_hour)
    {
        if (set.tm_min >= now.tm_min)
            return set.tm_min - now.tm_min;
        else
            return now.tm_min - set.tm_min;
    }

    // 计算两个方向的分钟差
    int d1 = now_mins - set_mins;
    if (d1 < 0) d1 += 24 * 60;
    int d2 = set_mins - now_mins;
    if (d2 < 0) d2 += 24 * 60;

    return (d1 < d2) ? d1 : d2;
}

bool time_plus_check(int wat_begin_hour, int wat_begin_min, tm timeinfo)
{
    int time_2go = 7;
    // 拷贝当前时间再覆盖时分, time_gap() 仅用 hh:mm 无需日期字段
    struct tm wat_begin = timeinfo;
    wat_begin.tm_hour = wat_begin_hour;
    wat_begin.tm_min  = wat_begin_min;
    wat_begin.tm_sec  = 0;
    if (time_gap(timeinfo, wat_begin) < time_2go)
    {
        Serial.println("reached the target timezone:Start");
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
                Serial.print("in ttg");
                Serial.print(pump_working_flag);
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
    // 浇水进行中直接返回, 避免每秒阻塞 ~100ms 读传感器
    if (soil2wat == 1 || work_times > 0)
    {
        return true;
    }
    check_soil();
    if (soil_moisture_need > soil_moisture && auto_soil_watering_flag == 1 && soil_moisture != 0)
    {
        pump_working_flag = 1;
        if (soil2wat == 0)
        {
            work_times = valve_count;
            soil2wat = 1;
            start_work_time = timeinfo;
        }
        Serial.println("2");
        return true;
    }
    else
    {
        Serial.println("3");
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
        relay_set(pump_ch[i], pump_working_flag == 1);
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
    Serial.println("shut_all()被执行了");
    work_times = 0;
    net_solenoid_flag = 0;
    // 重置状态机, 防止软硬件不一致导致阀门异常切换
    current_state = STATE_IDLE;
}

// ============================================================
// 5 状态机执行器: 阀门 → 水泵 → 关泵 → 关阀
// ============================================================

static bool s_pump_on_entered = false;  // 标记刚进入 STATE_PUMP_ON, 需重置切换状态

void flag_execute()
{
    Serial.print("fle");
    Serial.print(pump_working_flag);
    Serial.print(" state:");
    Serial.println(current_state);

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
                    relay_set(valve_ch[i], true);
                    solenoid_line = i + 1;
                    Serial.print("HIGH");
                    Serial.println(i);
                }
                else
                {
                    relay_set(valve_ch[i], false);
                    Serial.print("LOW");
                    Serial.println(i);
                }
            }
        }
        break;

    case STATE_OPEN_VALVE:
        if (millis() - state_start_time >= VALVE_DELAY)
        {
            pump_work();
            s_pump_on_entered = true;
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

            // 每次从 STATE_OPEN_VALVE 新进入时, 重置切换状态
            // 防止上一次浇水周期的残留状态导致错误的双阀开启
            if (s_pump_on_entered)
            {
                last_working_valve = -1;
                valve_switch_state = 0;
                switch_start_time = 0;
                s_pump_on_entered = false;
            }
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
                    relay_set(valve_ch[last_working_valve], true);
                    if (current_working_valve != -1)
                    {
                        relay_set(valve_ch[current_working_valve], true);
                        solenoid_line = current_working_valve + 1;
                    }
                    else
                    {
                        solenoid_line = 0;
                    }

                    valve_switch_state = 1;
                    switch_start_time = millis();
                    Serial.println("开始切换电磁阀");
                }
                else if (current_working_valve != last_working_valve)
                {
                    for (int i = 0; i < valve_count; i++)
                    {
                        relay_set(valve_ch[i], false);
                    }

                    if (current_working_valve != -1)
                    {
                        relay_set(valve_ch[current_working_valve], true);
                        solenoid_line = current_working_valve + 1;
                        Serial.print("HIGH");
                        Serial.println(current_working_valve);
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
                    relay_set(valve_ch[last_working_valve], false);
                    Serial.print("LOW");
                    Serial.println(last_working_valve);

                    valve_switch_state = 0;
                    last_working_valve = current_working_valve;
                    Serial.println("电磁阀切换完成");
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
                relay_set(valve_ch[i], false);
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
