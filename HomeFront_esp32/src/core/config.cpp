#include "config.h"
#include "globals.h"
#include "../hal/oled.h"
#include <Preferences.h>

void loadConfig()
{
    Preferences prefs;
    if (!prefs.begin("homefront", true))
    {
        Serial.println("loadConfig: NVS open FAILED (read-only)");
        return;
    }

    // WiFi / 网络
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("password", "");
    device_id = prefs.getString("device_id", "");
    upUrl = prefs.getString("upUrl", "");

    // 硬件布局
    valve_count = constrain(prefs.getInt("valve_cnt", 3), 1, MAX_VALVES);
    pump_count  = constrain(prefs.getInt("pump_cnt", 1), 0, MAX_PUMPS);
    field_valve_num = constrain(prefs.getInt("field_valve", valve_count), 1, valve_count);

    // 浇水参数
    for (int i = 0; i < MAX_VALVES; i++)
    {
        char key[12];
        snprintf(key, sizeof(key), "wat_t%d", i);
        pin_watering_time[i] = constrain(prefs.getInt(key, (i < 3) ? 30 : 0), 0, 120);
    }
    wat_begin_hour = constrain(prefs.getInt("wat_hour", 4), 0, 23);
    wat_begin_min  = constrain(prefs.getInt("wat_min", 40), 0, 59);

    // 传感器
    soil_moisture_need = constrain(prefs.getFloat("soil_need", 32), 0, 100);

    // 自动开关
    auto_timing_watering_flag = constrain(prefs.getInt("auto_time", 0), 0, 1);
    auto_soil_watering_flag   = constrain(prefs.getInt("auto_soil", 1), 0, 1);

    // 特殊模式
    carwash_duration_min = constrain(prefs.getInt("carwash_dur", 30), 1, 120);

    prefs.end();

    Serial.println("loadConfig done");
    if (ssid.length() > 0)
    {
        Serial.printf("ssid: %s  valves:%d  pumps:%d  field_v:%d\n",
                       ssid.c_str(), valve_count, pump_count, field_valve_num);
    }
    else
    {
        Serial.println("no saved ssid, need config portal");
    }
}

// 将当前所有参数写入 NVS
void saveAllConfig()
{
    Preferences prefs;
    if (!prefs.begin("homefront", false))
    {
        Serial.println("saveAllConfig: NVS open FAILED (read-write)");
        return;
    }

    prefs.putInt("valve_cnt", valve_count);
    prefs.putInt("pump_cnt", pump_count);
    prefs.putInt("field_valve", field_valve_num);

    for (int i = 0; i < MAX_VALVES; i++)
    {
        char key[12];
        snprintf(key, sizeof(key), "wat_t%d", i);
        prefs.putInt(key, pin_watering_time[i]);
    }
    prefs.putInt("wat_hour", wat_begin_hour);
    prefs.putInt("wat_min", wat_begin_min);
    prefs.putFloat("soil_need", soil_moisture_need);
    prefs.putInt("auto_time", auto_timing_watering_flag);
    prefs.putInt("auto_soil", auto_soil_watering_flag);
    prefs.putInt("carwash_dur", carwash_duration_min);

    prefs.end();
    Serial.println("saveAllConfig done");
}

// ============================================================
// AP 配置门户 (Web 服务器)
// ============================================================

// URL 解码
static String urlDecode(const String &s)
{
    String r = s;
    r.replace("+", " ");
    r.replace("%25", "%");  r.replace("%21", "!");  r.replace("%23", "#");
    r.replace("%24", "$");  r.replace("%26", "&");  r.replace("%27", "'");
    r.replace("%28", "(");  r.replace("%29", ")");  r.replace("%2A", "*");
    r.replace("%2B", "+");  r.replace("%2C", ",");  r.replace("%2F", "/");
    r.replace("%3A", ":");  r.replace("%3B", ";");  r.replace("%3D", "=");
    r.replace("%3F", "?");  r.replace("%40", "@");
    return r;
}

// 构建配置网页 HTML
static String buildConfigHtml()
{
    String h;
    h.reserve(3000);
    h += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Homefront Setup</title>";
    h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    h += "<style>body{font-family:Arial;max-width:420px;margin:10px auto;padding:12px;background:#f5f5f5;}";
    h += ".card{background:#fff;border-radius:8px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.1);}";
    h += "h2{font-size:18px;margin:8px 0;text-align:center;color:#2e7d32}";
    h += "h3{font-size:14px;margin:0 0 8px;color:#555;border-bottom:1px solid #eee;padding-bottom:4px}";
    h += "label{display:block;font-size:13px;margin:4px 0 1px;color:#333}";
    h += "input,select{width:100%;padding:6px;margin:2px 0 8px;box-sizing:border-box;font-size:14px;border:1px solid #ccc;border-radius:4px}";
    h += "input[type=submit]{background:#4CAF50;color:#fff;border:none;padding:12px;font-size:16px;margin-top:10px;border-radius:6px}";
    h += ".brand{text-align:center;color:#999;font-size:11px;margin-top:16px}";
    h += "</style></head><body>";
    h += "<h2>HomeFront 配网设置</h2>";
    h += "<p style='text-align:center;color:#666;font-size:13px;margin:-4px 0 12px'>浙江水龙农业科技</p>";
    h += "<form method='POST' action='/save'>";

    // WiFi
    h += "<h3>WiFi 设置</h3>";
    h += "<label>WiFi名字:</label><input name='ssid' value='" + ssid + "' required>";
    h += "<label>Password:</label><input name='password' type='password' value='" + password + "'>";
    h += "<label>Device ID:</label><input name='devid' value='" + device_id + "'>";
    h += "<label>OTA URL:</label><input name='upurl' value='" + upUrl + "'>";

    // 硬件布局
    h += "<h3>硬件布局</h3>";
    h += "<label>阀门数量 (1-6):</label><input name='v_cnt' type='number' min='1' max='6' value='" + String(valve_count) + "'>";
    h += "<label>水泵数量 (0-2):</label><input name='p_cnt' type='number' min='0' max='2' value='" + String(pump_count) + "'>";
    h += "<label>菜地阀门号 (1-" + String(valve_count) + "):</label><input name='fld_v' type='number' min='1' max='" + String(valve_count) + "' value='" + String(field_valve_num) + "'>";

    // 浇水时长
    h += "<h3>阀门浇水时长 (分钟)</h3>";
    for (int i = 0; i < valve_count; i++)
    {
        h += "<label>阀门" + String(i + 1) + ":</label>";
        h += "<input name='wt_" + String(i) + "' type='number' min='0' max='120' value='" + String(pin_watering_time[i]) + "'>";
    }

    // 浇水参数
    h += "<h3>定时浇水</h3>";
    h += "<label>开始时间:</label>";
    h += "<input name='wh' type='number' min='0' max='23' value='" + String(wat_begin_hour) + "' style='width:60px;display:inline'> 时 ";
    h += "<input name='wm' type='number' min='0' max='59' value='" + String(wat_begin_min) + "' style='width:60px;display:inline'> 分";
    h += "<br>";

    // 传感器
    h += "<h3>土壤湿度</h3>";
    h += "<label>最低湿度阈值 (%):</label><input name='s_need' type='number' min='0' max='100' value='" + String((int)soil_moisture_need) + "'>";

    // 自动开关
    h += "<h3>自动浇水开关</h3>";
    h += "<label>定时自动:</label><select name='a_time'>";
    h += "<option value='1'" + String(auto_timing_watering_flag ? " selected" : "") + ">开</option>";
    h += "<option value='0'" + String(auto_timing_watering_flag ? "" : " selected") + ">关</option>";
    h += "</select>";
    h += "<label>湿度自动:</label><select name='a_soil'>";
    h += "<option value='1'" + String(auto_soil_watering_flag ? " selected" : "") + ">开</option>";
    h += "<option value='0'" + String(auto_soil_watering_flag ? "" : " selected") + ">关</option>";
    h += "</select>";

    // 洗车
    h += "<h3>洗车模式</h3>";
    h += "<label>持续时长 (分钟):</label><input name='cw_dur' type='number' min='1' max='120' value='" + String(carwash_duration_min) + "'>";

    h += "<input type='submit' value='保存并启动'>";
    h += "<p class='brand'>浙江水龙农业科技 &copy; 2026</p>";
    h += "</form></body></html>";
    return h;
}

void startConfigPortal()
{
    // 初始化 OLED 显示配网提示
    oled_init();
    {
        const char *lines[] = {
            "AP配网模式",
            "请连接WiFi:",
            "Homefront-Setup",
            "浏览器打开",
            "192.168.4.1"
        };
        oled_draw_page("首次配置", lines, 5, -1, false);
    }

    Serial.println("startConfigPortal: starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Homefront-Setup");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    WiFiServer server(80);
    server.begin();
    Serial.println("HTTP config server started on port 80");

    while (true)
    {
        WiFiClient client = server.available();
        if (!client) { delay(10); continue; }

        String request = "";
        unsigned long timeout = millis() + 5000;

        // 第一步: 读取 HTTP 头 (直到 \r\n\r\n)
        while (client.connected() && millis() < timeout)
        {
            if (client.available())
            {
                char c = client.read();
                request += c;
                if (request.endsWith("\r\n\r\n")) break;
            }
        }

        // 如果是 POST, 按 Content-Length 读取 body
        String body = "";
        if (request.indexOf("POST /save") >= 0)
        {
            int clen = 0;
            int clPos = request.indexOf("Content-Length:");
            if (clPos >= 0)
            {
                int clEnd = request.indexOf("\r\n", clPos);
                String clStr = request.substring(clPos + 15, clEnd);
                clStr.trim();
                clen = clStr.toInt();
                Serial.printf("Content-Length: %d\n", clen);
            }

            // 先提取 header 后已读入的 body 部分
            int bodyStart = request.indexOf("\r\n\r\n");
            if (bodyStart >= 0)
            {
                body = request.substring(bodyStart + 4);
            }

            // 继续读取剩余 body 字节
            unsigned long bodyTimeout = millis() + 3000;
            while (body.length() < (unsigned)clen && client.connected() && millis() < bodyTimeout)
            {
                if (client.available())
                {
                    body += (char)client.read();
                }
            }
            Serial.printf("body read: %d / %d bytes\n", body.length(), clen);

            // ---- 解析 POST body ----
            String nSsid = "", nPass = "", nDevid = "", nUpurl = "";
            int nValveCnt = valve_count, nPumpCnt = pump_count, nFieldV = field_valve_num;
            int nWatT[MAX_VALVES];
            for (int i = 0; i < MAX_VALVES; i++) nWatT[i] = pin_watering_time[i];
            int nWHour = wat_begin_hour, nWMin = wat_begin_min;
            int nSoilNeed = (int)soil_moisture_need;
            int nAutoTime = auto_timing_watering_flag, nAutoSoil = auto_soil_watering_flag;
            int nCWdur = carwash_duration_min;

            int pos = 0;
            while (pos < body.length())
            {
                int eqPos = body.indexOf('=', pos);
                int ampPos = body.indexOf('&', pos);
                if (ampPos == -1) ampPos = body.length();
                if (eqPos >= 0 && eqPos < ampPos)
                {
                    String key = body.substring(pos, eqPos);
                    String val = urlDecode(body.substring(eqPos + 1, ampPos));

                    if (key == "ssid")             nSsid = val;
                    else if (key == "password")    nPass = val;
                    else if (key == "devid")       nDevid = val;
                    else if (key == "upurl")       nUpurl = val;
                    else if (key == "v_cnt")       nValveCnt = val.toInt();
                    else if (key == "p_cnt")       nPumpCnt = val.toInt();
                    else if (key == "fld_v")       nFieldV = val.toInt();
                    else if (key == "wh")          nWHour = val.toInt();
                    else if (key == "wm")          nWMin = val.toInt();
                    else if (key == "s_need")      nSoilNeed = val.toInt();
                    else if (key == "a_time")      nAutoTime = val.toInt();
                    else if (key == "a_soil")      nAutoSoil = val.toInt();
                    else if (key == "cw_dur")      nCWdur = val.toInt();
                    else if (key.startsWith("wt_"))
                    {
                        int idx = key.substring(3).toInt();
                        if (idx >= 0 && idx < MAX_VALVES) nWatT[idx] = val.toInt();
                    }
                }
                pos = ampPos + 1;
            }

            // 约束值
            nValveCnt = constrain(nValveCnt, 1, MAX_VALVES);
            nPumpCnt  = constrain(nPumpCnt, 0, MAX_PUMPS);
            nFieldV   = constrain(nFieldV, 1, nValveCnt);
            nWHour    = constrain(nWHour, 0, 23);
            nWMin     = constrain(nWMin, 0, 59);
            nSoilNeed = constrain(nSoilNeed, 0, 100);
            nCWdur    = constrain(nCWdur, 1, 120);

            // 写入 NVS
            {
                Preferences prefs;
                if (!prefs.begin("homefront", false))
                {
                    Serial.println("NVS open FAILED in startConfigPortal save");
                }
                else
                {
                    if (nSsid.length() > 0)   prefs.putString("ssid", nSsid);
                    if (nPass.length() > 0)   prefs.putString("password", nPass);
                    if (nDevid.length() > 0)  prefs.putString("device_id", nDevid);
                    if (nUpurl.length() > 0)  prefs.putString("upUrl", nUpurl);
                    prefs.putInt("valve_cnt", nValveCnt);
                    prefs.putInt("pump_cnt", nPumpCnt);
                    prefs.putInt("field_valve", nFieldV);
                    for (int i = 0; i < MAX_VALVES; i++)
                    {
                        char k[12];
                        snprintf(k, sizeof(k), "wat_t%d", i);
                        prefs.putInt(k, constrain(nWatT[i], 0, 120));
                    }
                    prefs.putInt("wat_hour", nWHour);
                    prefs.putInt("wat_min", nWMin);
                    prefs.putFloat("soil_need", nSoilNeed);
                    prefs.putInt("auto_time", nAutoTime);
                    prefs.putInt("auto_soil", nAutoSoil);
                    prefs.putInt("carwash_dur", nCWdur);
                    prefs.end();

                    // 验证写入
                    Preferences verify;
                    if (verify.begin("homefront", true))
                    {
                        String checkSsid = verify.getString("ssid", "");
                        verify.end();
                        if (checkSsid.length() > 0)
                            Serial.printf("NVS verify OK: ssid=%s\n", checkSsid.c_str());
                        else
                            Serial.println("NVS verify FAILED: ssid empty after save!");
                    }
                }
            }

            // 同步更新全局变量
            ssid = nSsid;
            password = nPass;
            device_id = nDevid;
            upUrl = nUpurl;
            valve_count = nValveCnt;
            pump_count = nPumpCnt;
            field_valve_num = nFieldV;
            for (int i = 0; i < MAX_VALVES; i++) pin_watering_time[i] = nWatT[i];
            wat_begin_hour = nWHour;
            wat_begin_min = nWMin;
            soil_moisture_need = nSoilNeed;
            auto_timing_watering_flag = nAutoTime;
            auto_soil_watering_flag = nAutoSoil;
            carwash_duration_min = nCWdur;

            // 返回确认页面
            {
                String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
                resp += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>OK</title>";
                resp += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
                resp += "<style>body{font-family:Arial;text-align:center;padding:40px 16px;background:#f5f5f5;}";
                resp += ".card{background:#fff;border-radius:8px;padding:24px;max-width:360px;margin:0 auto;box-shadow:0 2px 8px rgba(0,0,0,.1);}";
                resp += "h2{color:#2e7d32;margin:0 0 8px} p{color:#666;font-size:14px;margin:6px 0}";
                resp += ".brand{color:#999;font-size:11px;margin-top:20px}</style></head><body>";
                resp += "<div class='card'>";
                resp += "<h2>配置已保存</h2>";
                resp += "<p>设备正在启动...</p>";
                resp += "<p>请观察 OLED 屏幕, 稍后将显示 IP 地址</p>";
                resp += "<p style='font-size:12px;color:#999;margin-top:12px'>您现在可以关闭此页面,<br>连接回原来的 WiFi</p>";
                resp += "</div>";
                resp += "<p class='brand'>浙江水龙农业科技</p>";
                resp += "</body></html>";
                client.print(resp);
            }
            client.stop();

            // 退出 AP 模式
            server.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(500);
            Serial.println("startConfigPortal: config saved, exiting AP mode");
            return;
        }
        else
        {
            String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
            html += buildConfigHtml();
            client.print(html);
            client.stop();
        }
    }
}
