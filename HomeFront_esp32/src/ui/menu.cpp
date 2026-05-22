/*
 * 中文菜单系统
 * - 字体: u8g2_font_wqy12_t_gb2312 (12x12 中文)
 * - 操作: EC11 旋转=移动/调值, 按下=进入/确认
 *         KEY1=返回,  KEY2=快捷控制,  KEY3=紧急停止
 */

#include "menu.h"
#include <Preferences.h>
#include "../hal/oled.h"
#include "../hal/encoder.h"
#include "../hal/buttons.h"
#include "../core/globals.h"
#include "../core/watering.h"
#include "../core/config.h"

// ---- 页面 ID ----
enum Page {
    PAGE_MAIN = 0,
    PAGE_STATUS,
    PAGE_CONTROL,
    PAGE_MODE,
    PAGE_PARAMS,
    PAGE_SYSTEM,
    PAGE_ABOUT,
};

// ---- 编辑状态 ----
static int current_page = PAGE_MAIN;
static int cursor = 0;
static bool editing = false;    // true=正在编辑数值
static int  edit_val = 0;

// ============================================================
// 中文辅助
// ============================================================

static const char *YN(int v) { return v ? "开" : "关"; }

// 用于数值项: 显示 "Label: 值"
static void fmt_val(char *buf, int sz, const char *fmt, int v)
{
    snprintf(buf, sz, fmt, v);
}

// ============================================================
// 每页项数
// ============================================================
static int page_count(int page)
{
    switch (page)
    {
        case PAGE_MAIN:    return 5;
        case PAGE_STATUS:  return 4;
        case PAGE_CONTROL: return valve_count + pump_count;  // 动态
        case PAGE_MODE:    return 5;
        case PAGE_PARAMS:  return 7 + valve_count;           // 阀门时长 ×N + 菜地号 + 定时 + 湿度 + 洗车
        case PAGE_SYSTEM:  return 5;
        case PAGE_ABOUT:   return 4;
        default:           return 0;
    }
}

// ============================================================
// 状态页 (只读)
// ============================================================
static void draw_status()
{
    char b1[22], b2[22], b3[22], b4[22];
    snprintf(b1, sizeof(b1), "土壤湿度: %.1f%%", soil_moisture);
    snprintf(b2, sizeof(b2), "阈值: %.0f%%", soil_moisture_need);
    const char *mode = "空闲";
    if (carwash_flag)                         mode = "洗车";
    else if (vegetable_flag_net || vegetable_flag_hand) mode = "菜地";
    else if (soil2wat)                        mode = "浇水中";
    snprintf(b3, sizeof(b3), "模式: %s", mode);
    snprintf(b4, sizeof(b4), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    const char *lines[] = {b1, b2, b3, b4};
    oled_draw_page("状态信息", lines, 4, -1, false);
}

// ============================================================
// 控制页 (手动开关阀门/水泵)
// ============================================================
static void draw_control()
{
    char bufs[MAX_VALVES + MAX_PUMPS][22];
    const char *lines[MAX_VALVES + MAX_PUMPS];

    for (int i = 0; i < valve_count; i++)
    {
        snprintf(bufs[i], sizeof(bufs[i]), "阀门%d: %s", i + 1,
                 working_solenoid_valve[i] ? "开" : "关");
        lines[i] = bufs[i];
    }
    for (int i = 0; i < pump_count; i++)
    {
        snprintf(bufs[valve_count + i], sizeof(bufs[valve_count + i]),
                 "水泵%d: %s", i + 1, pump_working_flag ? "开" : "关");
        lines[valve_count + i] = bufs[valve_count + i];
    }
    oled_draw_page("阀门控制", lines, valve_count + pump_count, cursor, true);
}

static void handle_control_select()
{
    if (cursor < valve_count)
    {
        // 阀门开关
        if (working_solenoid_valve[cursor])
        {
            working_solenoid_valve[cursor] = 0;
            bool any = false;
            for (int i = 0; i < valve_count; i++)
                if (working_solenoid_valve[i]) { any = true; break; }
            if (!any) shut_all();
        }
        else
        {
            shut_all();
            working_solenoid_valve[cursor] = 1;
            pump_working_flag = 1;
            soil2wat = 1;
            start_work_time = timeinfo;
            net_solenoid_flag = 1;
        }
    }
    else
    {
        // 水泵开关
        if (pump_working_flag) { shut_all(); }
        else
        {
            bool any = false;
            for (int i = 0; i < valve_count; i++)
                if (working_solenoid_valve[i]) { any = true; break; }
            if (any)
            {
                pump_working_flag = 1;
                start_work_time = timeinfo;
                soil2wat = 1;
            }
        }
    }
}

// ============================================================
// 模式选择页
// ============================================================
static void draw_mode()
{
    char b1[22], b2[22], b3[22], b4[22], b5[22];
    snprintf(b1, sizeof(b1), "定时浇水: %s", YN(auto_timing_watering_flag));
    snprintf(b2, sizeof(b2), "湿度浇水: %s", YN(auto_soil_watering_flag));
    snprintf(b3, sizeof(b3), "▸ 手动浇水");
    snprintf(b4, sizeof(b4), "▸ 洗车模式");
    snprintf(b5, sizeof(b5), "▸ 菜地浇水");
    const char *lines[] = {b1, b2, b3, b4, b5};
    oled_draw_page("模式选择", lines, 5, cursor, true);
}

static void handle_mode_select()
{
    switch (cursor)
    {
        case 0: // 定时浇水开关
            auto_timing_watering_flag = !auto_timing_watering_flag;
            if (!auto_timing_watering_flag && !soil2wat) shut_all();
            break;
        case 1: // 湿度浇水开关
            auto_soil_watering_flag = !auto_soil_watering_flag;
            if (!auto_soil_watering_flag && !soil2wat) shut_all();
            break;
        case 2: // 手动浇水
            shut_all();
            pump_working_flag = 1;
            hand_watering_flag = 1;
            start_work_time = timeinfo;
            work_times = valve_count;
            soil2wat = 1;
            break;
        case 3: // 洗车
            shut_all();
            carwash_flag = 1;
            pump_working_flag = 1;
            start_work_time = timeinfo;
            soil2wat = 1;
            break;
        case 4: // 菜地
            shut_all();
            vegetable_flag_net = 1;
            pump_working_flag = 1;
            working_solenoid_valve[field_valve_num - 1] = 1;
            start_work_time = timeinfo;
            soil2wat = 1;
            break;
    }
    saveAllConfig();  // 开关类参数立即持久化
}

// ============================================================
// 参数设置页 (含数值编辑器)
// ============================================================

struct ParamInfo {
    const char *label;
    int *value;
    int min_val, max_val;
};

static ParamInfo get_param_info(int idx)
{
    ParamInfo p = {nullptr, nullptr, 0, 0};

    if (idx == 0)
    {
        p.label = "阀门数量"; p.value = &valve_count; p.min_val = 1; p.max_val = MAX_VALVES;
    }
    else if (idx == 1)
    {
        p.label = "水泵数量"; p.value = &pump_count; p.min_val = 0; p.max_val = MAX_PUMPS;
    }
    else if (idx < 2 + valve_count)
    {
        int vi = idx - 2;
        static char buf[18];  // safe: single-threaded, redrawn each frame
        snprintf(buf, sizeof(buf), "阀门%d时长(分)", vi + 1);
        p.label = buf;
        p.value = &pin_watering_time[vi];
        p.min_val = 1; p.max_val = 120;
    }
    else
    {
        int base = 2 + valve_count;
        switch (idx - base)
        {
            case 0: p.label = "菜地阀门号"; p.value = &field_valve_num;       p.min_val = 1; p.max_val = valve_count; break;
            case 1: p.label = "定时-时";     p.value = &wat_begin_hour;        p.min_val = 0; p.max_val = 23;         break;
            case 2: p.label = "定时-分";     p.value = &wat_begin_min;         p.min_val = 0; p.max_val = 59;         break;
            case 3: {
                static int soil_tmp;
                soil_tmp = (int)soil_moisture_need;
                p.label = "湿度阈值(%)"; p.value = &soil_tmp; p.min_val = 0; p.max_val = 100;
                break;
            }
            case 4: p.label = "洗车时长(分)"; p.value = &carwash_duration_min;  p.min_val = 1; p.max_val = 120;       break;
        }
    }
    return p;
}

static void draw_params()
{
    int n = page_count(PAGE_PARAMS);
    #define PARAM_BUF_COUNT (MAX_VALVES + 7)
    char bufs[PARAM_BUF_COUNT][22];
    const char *lines[PARAM_BUF_COUNT];
    if (n > PARAM_BUF_COUNT) n = PARAM_BUF_COUNT;

    for (int i = 0; i < n; i++)
    {
        ParamInfo p = get_param_info(i);
        if (!p.value)
        {
            bufs[i][0] = 0;
            lines[i] = bufs[i];
            continue;
        }

        snprintf(bufs[i], sizeof(bufs[i]), "%s: %d%s",
                 p.label,
                 (editing && cursor == i) ? edit_val : *p.value,
                 (editing && cursor == i) ? " ◀" : "");
        lines[i] = bufs[i];
    }
    oled_draw_page("参数设置", lines, n, cursor, !editing);
}

static void enter_edit()
{
    ParamInfo p = get_param_info(cursor);
    if (p.value)
    {
        editing = true;
        edit_val = *p.value;
    }
}

static void commit_edit()
{
    if (!editing) return;
    ParamInfo p = get_param_info(cursor);
    if (p.value)
    {
        edit_val = constrain(edit_val, p.min_val, p.max_val);
        *p.value = edit_val;

        // 阀门数量改变 → 约束菜地号
        if (cursor == 0)
            field_valve_num = constrain(field_valve_num, 1, valve_count);

        // 湿度阈值需要同步到 float 变量
        int base = 2 + valve_count;
        if (cursor == base + 3)
            soil_moisture_need = (float)edit_val;
    }
    editing = false;
    saveAllConfig();
}

static void adj_val(int dir)
{
    if (!editing) return;
    ParamInfo p = get_param_info(cursor);
    if (p.value)
        edit_val = constrain(edit_val + dir, p.min_val, p.max_val);
}

// ============================================================
// 系统管理页
// ============================================================
static void draw_system()
{
    const char *items[] = {
        "▸ 保存设置",
        "▸ WiFi重连",
        "▸ 关于本机",
        "▸ 重启设备",
        "▸ 恢复出厂"
    };
    oled_draw_page("系统管理", items, 5, cursor, true);
}

static void handle_system_select()
{
    switch (cursor)
    {
        case 0: saveAllConfig(); break;                    // 保存设置
        case 1: WiFi.reconnect(); break;                  // WiFi 重连
        case 2: current_page = PAGE_ABOUT; cursor = 0; break; // 关于
        case 3: saveAllConfig(); delay(200); ESP.restart(); break;
        case 4: // 恢复出厂
        {
            Preferences p; p.begin("homefront", false);
            p.clear(); p.end();
            delay(200); ESP.restart();
            break;
        }
    }
}

// ============================================================
// 关于本机
// ============================================================
static void draw_about()
{
    char b1[22], b2[22], b3[22], b4[22];
    snprintf(b1, sizeof(b1), "HomeFront v2.0");
    const char *wifi_state = (WiFi.status() == WL_CONNECTED) ? "已连接" : "未连接";
    snprintf(b2, sizeof(b2), "WiFi: %s", wifi_state);
    snprintf(b3, sizeof(b3), "IP: %s", WiFi.localIP().toString().c_str());
    snprintf(b4, sizeof(b4), "ID: %.14s", device_id.c_str());
    const char *lines[] = {b1, b2, b3, b4};
    oled_draw_page("关于本机", lines, 4, -1, false);
}

// ============================================================
// 主菜单
// ============================================================
static void draw_main()
{
    const char *items[] = {"状态信息", "阀门控制", "模式选择", "参数设置", "系统管理"};
    oled_draw_page("HomeFront", items, 5, cursor, true);
}

static void handle_main_select()
{
    switch (cursor)
    {
        case 0: current_page = PAGE_STATUS;  break;
        case 1: current_page = PAGE_CONTROL; break;
        case 2: current_page = PAGE_MODE;    break;
        case 3: current_page = PAGE_PARAMS;  break;
        case 4: current_page = PAGE_SYSTEM;  break;
    }
    cursor = 0;
}

// ============================================================
// 统一绘制
// ============================================================
static void draw_current()
{
    switch (current_page)
    {
        case PAGE_MAIN:    draw_main();    break;
        case PAGE_STATUS:  draw_status();  break;
        case PAGE_CONTROL: draw_control(); break;
        case PAGE_MODE:    draw_mode();    break;
        case PAGE_PARAMS:  draw_params();  break;
        case PAGE_SYSTEM:  draw_system();  break;
        case PAGE_ABOUT:   draw_about();   break;
    }
}

// ============================================================
// 公开接口
// ============================================================

void menu_init()
{
    oled_init();
    encoder_init();
    buttons_init();
    draw_current();
}

void menu_tick()
{
    EncoderEvent enc = encoder_get_event();
    int maxItems = page_count(current_page);

    // ---- 编辑模式 ----
    if (editing)
    {
        if (enc == ENC_UP)        { adj_val(1);  draw_current(); }
        else if (enc == ENC_DOWN) { adj_val(-1); draw_current(); }
        else if (enc == ENC_CLICK) { commit_edit(); draw_current(); }

        // KEY1 取消编辑
        if (button_get_event(0) == BTN_PRESS) { editing = false; cursor = 0; draw_current(); }
        return; // 编辑中忽略按键2/3
    }

    // ---- 正常导航 ----
    if (enc == ENC_UP)
    {
        cursor = (cursor - 1 + maxItems) % maxItems;
        draw_current();
    }
    else if (enc == ENC_DOWN)
    {
        cursor = (cursor + 1) % maxItems;
        draw_current();
    }
    else if (enc == ENC_CLICK)
    {
        switch (current_page)
        {
            case PAGE_MAIN:    handle_main_select();     break;
            case PAGE_CONTROL: handle_control_select();  break;
            case PAGE_MODE:    handle_mode_select();     break;
            case PAGE_PARAMS:  enter_edit();             break;  // 进入编辑模式
            case PAGE_SYSTEM:  handle_system_select();   break;
            case PAGE_ABOUT:   /* no action */           break;
        }
        draw_current();
    }

    // KEY1 → 返回
    if (button_get_event(0) == BTN_PRESS)
    {
        if (current_page != PAGE_MAIN) { current_page = PAGE_MAIN; cursor = 0; }
        draw_current();
    }

    // KEY2 → 快捷控制
    if (button_get_event(1) == BTN_PRESS)
    {
        current_page = PAGE_CONTROL; cursor = 0;
        draw_current();
    }

    // KEY3 → 紧急停止
    if (button_get_event(2) == BTN_PRESS || button_get_event(2) == BTN_LONG_PRESS)
    {
        shut_all();
        current_page = PAGE_MAIN; cursor = 0;
        draw_current();
    }
}
