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
#include "../core/version.h"
#include "../core/ota.h"

// ---- 页面 ID ----
enum Page {
    PAGE_MAIN = 0,
    PAGE_STATUS,
    PAGE_CONTROL,
    PAGE_MODE,
    PAGE_PARAMS,
    PAGE_SYSTEM,
    PAGE_ABOUT,
    PAGE_CONFIRM,   // 破坏性操作确认页
    PAGE_MSG,       // 临时消息页 (紧急停止等)
};

// ---- 编辑状态 ----
static int current_page = PAGE_MAIN;
static int cursor = 0;
static bool editing = false;    // true=正在编辑数值
static int  edit_val = 0;
static int  edit_dir = 0;    // -1=减, 0=刚进入, +1=加 — 用于方向指示动画

// ---- 确认页状态 (必须在 get_parent_page 前声明) ----
static int confirm_action = 0;  // 0=重启, 1=恢复出厂, 2=紧急停止
static int confirm_return_page = PAGE_MAIN;
static int confirm_return_cursor = 0;

// ---- 导航 ----
// 每个页面的"上级页面", 使用函数而非可变变量, 避免深层导航时状态被覆盖
static int get_parent_page(int page)
{
    switch (page)
    {
        case PAGE_MAIN:    return PAGE_MAIN;     // 主页不能再返回
        case PAGE_ABOUT:   return PAGE_SYSTEM;   // 关于 ← 系统管理
        case PAGE_CONFIRM: return (confirm_action == 2) ? confirm_return_page : PAGE_SYSTEM;
        default:           return PAGE_MAIN;     // 其余页面均返回主菜单
    }
}

// ---- 临时消息页 ----
static const char *msg_text = nullptr;
static unsigned long msg_start_time = 0;
static int msg_prev_page = PAGE_MAIN;
static int msg_prev_cursor = 0;

static void show_msg(const char *text, int return_page, int return_cursor)
{
    msg_text = text;
    msg_start_time = millis();
    msg_prev_page = return_page;
    msg_prev_cursor = return_cursor;
    current_page = PAGE_MSG;
    cursor = 0;
}

// ============================================================
// 中文辅助
// ============================================================

static const char *YN(int v) { return v ? "开" : "关"; }

// 页面中文名
static const char *page_cn(int page)
{
    switch (page)
    {
        case PAGE_MAIN:    return "主菜单";
        case PAGE_STATUS:  return "状态信息";
        case PAGE_CONTROL: return "阀门控制";
        case PAGE_MODE:    return "模式选择";
        case PAGE_PARAMS:  return "参数设置";
        case PAGE_SYSTEM:  return "系统管理";
        case PAGE_ABOUT:   return "关于本机";
        default:           return "";
    }
}

// 构建面包屑标题: "< 上级页面名" (主页显示 "HomeFront")
static const char *breadcrumb(int page)
{
    static char buf[18];
    if (page == PAGE_MAIN) return "HomeFront";
    snprintf(buf, sizeof(buf), "< %s", page_cn(get_parent_page(page)));
    return buf;
}

// 标题栏右侧: 时间 + WiFi 信号条
static void title_bar(char *time_buf, int sz, bool &wifi_ok)
{
    snprintf(time_buf, sz, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    wifi_ok = (WiFi.status() == WL_CONNECTED);
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
        case PAGE_MODE:    return 6;
        case PAGE_PARAMS:  return 7 + valve_count;           // 阀门时长 ×N + 菜地号 + 定时 + 湿度 + 洗车
        case PAGE_SYSTEM:  return 6;  // 保存/WiFi重连/关于/固件升级/重启/恢复出厂
        case PAGE_ABOUT:   return 6;  // 版本/编译时间/WiFi/IP/运行时长/内存
        case PAGE_CONFIRM: return (confirm_action == 2) ? 2 : 1;
        case PAGE_MSG:     return 1;
        default:           return 0;
    }
}

// ============================================================
// 状态页 (只读)
// ============================================================
static void draw_status()
{
    char b1[22], b2[22], b3[22], b4[64];
    snprintf(b1, sizeof(b1), "土壤湿度: %.1f%%", soil_moisture);
    snprintf(b2, sizeof(b2), "阈值: %.0f%%", soil_moisture_need);

    // 模式文本
    const char *mode = "空闲";
    if (carwash_flag)                           mode = "洗车";
    else if (hand_watering_flag)                mode = "手动浇花";
    else if (vegetable_flag_net || vegetable_flag_hand) mode = "定湿度";
    else if (soil2wat)                          mode = "浇水中";
    snprintf(b3, sizeof(b3), "模式: %s", mode);

    // 各区域浇花时长 (格式: "阀: 30V、30、30V")
    {
        char dur[64] = "阀: ";
        int pos = strlen(dur);
        for (int i = 0; i < valve_count && pos < (int)sizeof(dur) - 7; i++)
        {
            const char *fmt = working_solenoid_valve[i] ?
                (i == 0 ? "%dV" : "、%dV") :
                (i == 0 ? "%d"  : "、%d");
            pos += snprintf(dur + pos, sizeof(dur) - pos, fmt, pin_watering_time[i]);
        }
        snprintf(b4, sizeof(b4), "%s", dur);
    }
    const char *lines[] = {b1, b2, b3, b4};
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_STATUS), lines, 4, cursor, true, nullptr, tb, wk);
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
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_CONTROL), lines, valve_count + pump_count, cursor, true, nullptr, tb, wk);
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
            if (!any) shut_all_soft();  // 慢关防锤, 走状态机关泵→等5s→关阀序列
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
        if (pump_working_flag) { shut_all_soft(); }  // 慢关防锤
        else
        {
            pump_working_flag = 1;
            start_work_time = timeinfo;
            soil2wat = 1;
            // 检查是否有阀门已开, 未开时提示用户 (洗车模式等场景只需水泵)
            bool any = false;
            for (int i = 0; i < valve_count; i++)
                if (working_solenoid_valve[i]) { any = true; break; }
            if (!any)
                show_msg("请注意手动打开洗车阀门", PAGE_CONTROL, cursor);
        }
    }
}

// ============================================================
// 模式选择页
// ============================================================
static void draw_mode()
{
    char b1[22], b2[22], b3[22], b4[22], b5[22], b6[22];
    snprintf(b1, sizeof(b1), "定时浇水: %s%s", YN(auto_timing_watering_flag),
             auto_timing_watering_flag ? "V" : "");
    snprintf(b2, sizeof(b2), "湿度浇水: %s%s", YN(auto_soil_watering_flag),
             auto_soil_watering_flag ? "V" : "");
    snprintf(b3, sizeof(b3), "▸ 手动浇水%s", hand_watering_flag ? "V" : "");
    snprintf(b4, sizeof(b4), "▸ 洗车模式%s", carwash_flag ? "V" : "");
    snprintf(b5, sizeof(b5), "▸ 菜地浇水%s",
             (vegetable_flag_hand || vegetable_flag_net) ? "V" : "");
    snprintf(b6, sizeof(b6), "▸ 关闭全部");
    const char *lines[] = {b1, b2, b3, b4, b5, b6};
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_MODE), lines, 6, cursor, true, nullptr, tb, wk);
}

static void handle_mode_select()
{
    switch (cursor)
    {
        case 0: // 定时浇水开关
            auto_timing_watering_flag = !auto_timing_watering_flag;
            if (!auto_timing_watering_flag) shut_all();  // 关闭时强制停止, 防止灌溉卡住
            break;
        case 1: // 湿度浇水开关
            auto_soil_watering_flag = !auto_soil_watering_flag;
            if (!auto_soil_watering_flag) shut_all();
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
            vegetable_flag_hand = 1;
            pump_working_flag = 1;
            working_solenoid_valve[field_valve_num - 1] = 1;
            start_work_time = timeinfo;
            soil2wat = 1;
            break;
        case 5: // 关闭全部 — 停止所有正在进行的浇水, 恢复普通模式
            shut_all_soft();
            carwash_flag = 0;
            vegetable_flag_hand = 0;
            vegetable_flag_net = 0;
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
    char bufs[PARAM_BUF_COUNT][28];  // 最长标签"阀门N时长(分)"(18) + ": "(2) + 值(3) + hint" +/->"(2) + NUL(1) = 26, 取28
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

        const char *hint = "";
        if (editing && cursor == i)
        {
            if (edit_dir > 0)      hint = " +";
            else if (edit_dir < 0) hint = " -";
            else                   hint = " >";  // ASCII, WQY12 GB2312 不含 U+25C0
        }
        snprintf(bufs[i], sizeof(bufs[i]), "%s: %d%s",
                 p.label,
                 (editing && cursor == i) ? edit_val : *p.value,
                 hint);
        lines[i] = bufs[i];
    }
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_PARAMS), lines, n, cursor, true, nullptr, tb, wk);
}

static void enter_edit()
{
    ParamInfo p = get_param_info(cursor);
    if (p.value)
    {
        editing = true;
        edit_val = *p.value;
        edit_dir = 0;  // 刚进入, 显示 ◀
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
    edit_dir = 0;
    saveAllConfig();
}

static void adj_val(int dir)
{
    if (!editing) return;
    ParamInfo p = get_param_info(cursor);
    if (p.value)
    {
        edit_val = constrain(edit_val + dir, p.min_val, p.max_val);
        edit_dir = (dir > 0) ? 1 : -1;
    }
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
        "▸ 固件升级",
        "▸ 重启设备",
        "▸ 恢复出厂"
    };
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_SYSTEM), items, 6, cursor, true, nullptr, tb, wk);
}

static void handle_system_select()
{
    switch (cursor)
    {
        case 0: // 保存设置
            saveAllConfig();
            show_msg("设置已保存", PAGE_SYSTEM, cursor);
            break;
        case 1: // WiFi 重连
            WiFi.reconnect();
            show_msg("WiFi重连中...", PAGE_SYSTEM, cursor);
            break;
        case 2: // 关于
            current_page = PAGE_ABOUT; cursor = 0;
            break;
        case 3: // 固件升级
            if (upUrl.length() > 0)
            {
                updateBin();
                show_msg("固件升级中...", PAGE_SYSTEM, cursor);
            }
            else
            {
                show_msg("未配置升级URL", PAGE_SYSTEM, cursor);
            }
            break;
        case 4: // 重启 → 确认
            confirm_action = 0;
            current_page = PAGE_CONFIRM; cursor = 0;
            break;
        case 5: // 恢复出厂 → 确认
            confirm_action = 1;
            current_page = PAGE_CONFIRM; cursor = 0;
            break;
    }
}

// ============================================================
// 关于本机
// ============================================================
static void draw_about()
{
    char b1[22], b2[22], b3[22], b4[22], b5[22], b6[22];
    snprintf(b1, sizeof(b1), "版本: v" FW_VERSION);
    snprintf(b2, sizeof(b2), "构建: " FW_BUILD_DATE);
    const char *wifi_state = (WiFi.status() == WL_CONNECTED) ? "已连接" : "未连接";
    snprintf(b3, sizeof(b3), "WiFi: %s", wifi_state);
    snprintf(b4, sizeof(b4), "IP: %s", WiFi.localIP().toString().c_str());
    // 运行时长
    unsigned long uptime_s = millis() / 1000;
    int up_d = uptime_s / 86400;
    int up_h = (uptime_s % 86400) / 3600;
    int up_m = (uptime_s % 3600) / 60;
    snprintf(b5, sizeof(b5), "运行: %dd %02d:%02d", up_d, up_h, up_m);
    snprintf(b6, sizeof(b6), "内存: %uB", (unsigned)ESP.getFreeHeap());
    const char *lines[] = {b1, b2, b3, b4, b5, b6};
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(breadcrumb(PAGE_ABOUT), lines, 6, cursor, true, nullptr, tb, wk);
}

// ============================================================
// 主菜单
// ============================================================
static void draw_main()
{
    const char *items[] = {"状态信息", "阀门控制", "模式选择", "参数设置", "系统管理"};

    // 右上角: 时间 + 信号条 (像素绘制, 从矮到高)
    char status_right[8];
    snprintf(status_right, sizeof(status_right), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
    bool wifi_ok = (WiFi.status() == WL_CONNECTED);
    oled_draw_page("HomeFront", items, 5, cursor, true, nullptr, status_right, wifi_ok);
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
// 确认页 (用于破坏性操作)
// ============================================================
static void draw_confirm()
{
    const char *ctitle;
    const char *lines[2];
    int n;
    int cur;
    if (confirm_action == 2)
    {
        ctitle = "确认紧急停止?";
        lines[0] = "▸ 确认";
        lines[1] = "  取消";
        n = 2;
        cur = cursor;
    }
    else
    {
        ctitle = (confirm_action == 0) ? "确认重启?" : "确认恢复出厂?";
        lines[0] = "按下确认  KEY1取消";
        n = 1;
        cur = 0;
    }
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page(ctitle, lines, n, cur, (confirm_action == 2), nullptr, tb, wk);
}

// ============================================================
// 临时消息页
// ============================================================
static void draw_msg()
{
    const char *lines[] = {msg_text ? msg_text : ""};
    char tb[8]; bool wk;
    title_bar(tb, sizeof(tb), wk);
    oled_draw_page("提示", lines, 1, -1, false, nullptr, tb, wk);
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
        case PAGE_CONFIRM: draw_confirm(); break;
        case PAGE_MSG:     draw_msg();     break;
    }
}

// ============================================================
// 公开接口
// ============================================================

// 紧急停止 — 关断硬件 + 关闭所有模式 + 持久化
// resume_page/resume_cursor 仅在从确认页取消时使用, 但不影响 show_msg 的参数
static void emergency_stop()
{
    shut_all();
    auto_timing_watering_flag = 0;
    auto_soil_watering_flag = 0;
    carwash_flag = 0;
    vegetable_flag_hand = 0;
    vegetable_flag_net = 0;
    hand_watering_flag = 0;
    saveAllConfig();
}

void menu_init()
{
    oled_init();
    encoder_init();
    buttons_init();
    draw_current();
}

void menu_tick()
{
    // ---- 定期自动刷新: 时间/分钟变化 或 WiFi 状态变化时重绘标题栏 ----
    {
        static unsigned long last_refresh_check = 0;
        if (millis() - last_refresh_check >= 500)
        {
            last_refresh_check = millis();
            static int  last_min    = -1;
            static bool last_wifi_ok = false;
            int  cur_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            bool cur_wifi = (WiFi.status() == WL_CONNECTED);
            if (cur_min != last_min || cur_wifi != last_wifi_ok)
            {
                last_min    = cur_min;
                last_wifi_ok = cur_wifi;
                if (current_page != PAGE_MSG)
                    draw_current();
            }
        }
    }

    // ---- 临时消息页: 2 秒后自动消失, KEY1 可提前返回 ----
    if (current_page == PAGE_MSG)
    {
        if (millis() - msg_start_time >= 2000)
        {
            current_page = msg_prev_page;
            cursor = msg_prev_cursor;
            msg_text = nullptr;
            draw_current();
        }
        else if (button_get_event(0) == BTN_PRESS)
        {
            current_page = msg_prev_page;
            cursor = msg_prev_cursor;
            msg_text = nullptr;
            draw_current();
        }
        return;
    }

    EncoderEvent enc = encoder_get_event();
    int maxItems = page_count(current_page);

    // ---- 编辑模式 ----
    if (editing)
    {
        if (enc == ENC_UP)        { adj_val(1);  draw_current(); }
        else if (enc == ENC_DOWN) { adj_val(-1); draw_current(); }
        else if (enc == ENC_CLICK) { commit_edit(); draw_current(); }

        // KEY1 取消编辑
        if (button_get_event(0) == BTN_PRESS) { editing = false; edit_dir = 0; cursor = 0; draw_current(); }
        return; // 编辑中忽略按键2/3
    }

    // ---- 确认页 ----
    if (current_page == PAGE_CONFIRM)
    {
        if (confirm_action == 2)
        {
            // 紧急停止确认: 2 项可选, 滚轮上下切换
            if (enc == ENC_UP)        { cursor = 0;      draw_current(); }
            else if (enc == ENC_DOWN) { cursor = 1;      draw_current(); }
            else if (enc == ENC_CLICK)
            {
                if (cursor == 0)
                {
                    emergency_stop();
                    show_msg("已紧急停止!", PAGE_MAIN, 0);
                }
                else
                {
                    current_page = confirm_return_page;
                    cursor = confirm_return_cursor;
                }
                draw_current();
            }
        }
        else if (enc == ENC_CLICK)
        {
            if (confirm_action == 0)  // 重启
            {
                saveAllConfig();
                delay(200);
                ESP.restart();
            }
            else  // 恢复出厂
            {
                shut_all();                             // 先关断所有硬件
                Preferences p; p.begin("homefront", false);
                p.clear(); p.end();
                // 复位所有全局变量为默认值, 防止重启前瞬间的异常状态
                valve_count = 3; pump_count = 1; field_valve_num = 3;
                soil_moisture_need = 32;
                auto_timing_watering_flag = 0; auto_soil_watering_flag = 1;
                carwash_duration_min = 30;
                wat_begin_hour = 4; wat_begin_min = 40;
                for (int i = 0; i < MAX_VALVES; i++) pin_watering_time[i] = (i < 3) ? 30 : 0;
                for (int i = 0; i < MAX_VALVES; i++) working_solenoid_valve[i] = 0;
                ssid = ""; password = ""; device_id = ""; upUrl = "";
                // 显示确认消息后重启
                {
                    const char *lines[] = {"设置已清除", "即将重启..."};
                    char tb[8]; bool wk;
                    title_bar(tb, sizeof(tb), wk);
                    oled_draw_page("恢复出厂", lines, 2, -1, false, nullptr, tb, wk);
                }
                delay(1500);
                ESP.restart();
            }
        }
        if (button_get_event(0) == BTN_PRESS)
        {
            if (confirm_action == 2)
            {
                current_page = confirm_return_page;
                cursor = confirm_return_cursor;
            }
            else
            {
                current_page = PAGE_SYSTEM;
                cursor = (confirm_action == 0) ? 5 : 6;
            }
            draw_current();
        }
        return;
    }

    // ---- 正常导航 (边界循环) ----
    if (enc == ENC_UP)
    {
        if (cursor > 0) cursor--;
        else cursor = maxItems - 1;
        draw_current();
    }
    else if (enc == ENC_DOWN)
    {
        if (cursor < maxItems - 1) cursor++;
        else cursor = 0;
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

    // KEY1 → 返回上级
    if (button_get_event(0) == BTN_PRESS)
    {
        if (current_page != PAGE_MAIN)
        {
            current_page = get_parent_page(current_page);
            cursor = 0;
        }
        draw_current();
    }

    // KEY2 → 快捷控制
    if (button_get_event(1) == BTN_PRESS)
    {
        current_page = PAGE_CONTROL; cursor = 0;
        draw_current();
    }

    // KEY3 长按 → 紧急停止确认 (防误触)
    if (button_get_event(2) == BTN_LONG_PRESS)
    {
        confirm_action = 2;
        confirm_return_page = current_page;
        confirm_return_cursor = cursor;
        current_page = PAGE_CONFIRM;
        cursor = 0;  // 默认选中"确认"
        draw_current();
    }
}
