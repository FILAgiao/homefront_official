#include "config.h"
#include "globals.h"
#include <Preferences.h>

void loadConfig()
{
    Preferences prefs;
    prefs.begin("homefront", true);

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

    Serial2.println("loadConfig done");
    if (ssid.length() > 0)
    {
        Serial2.printf("ssid: %s  valves:%d  pumps:%d  field_v:%d\n",
                       ssid.c_str(), valve_count, pump_count, field_valve_num);
    }
    else
    {
        Serial2.println("no saved ssid, need config portal");
    }
}

// 将当前所有参数写入 NVS
void saveAllConfig()
{
    Preferences prefs;
    prefs.begin("homefront", false);

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
    Serial2.println("saveAllConfig done");
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
    h += "<style>body{font-family:Arial;max-width:420px;margin:10px auto;padding:12px;}";
    h += "h2{font-size:18px;margin:8px 0} h3{font-size:14px;margin:10px 0 4px;color:#555}";
    h += "label{display:block;font-size:13px;margin:4px 0 1px}";
    h += "input,select{width:100%;padding:6px;margin:2px 0 8px;box-sizing:border-box;font-size:14px;}";
    h += "input[type=submit]{background:#4CAF50;color:#fff;border:none;padding:10px;font-size:16px;margin-top:10px;}";
    h += "input.short{width:70px}</style></head><body>";
    h += "<h2>Homefront Config Portal</h2>";

    // WiFi
    h += "<h3>WiFi 设置</h3>";
    h += "<label>SSID:</label><input name='ssid' value='" + ssid + "' required>";
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

    h += "<input type='submit' value='Save & Restart'>";
    h += "</form></body></html>";
    return h;
}

void startConfigPortal()
{
    Serial2.println("startConfigPortal: starting AP...");
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
        if (!client) { delay(10); continue; }

        String request = "";
        unsigned long timeout = millis() + 5000;
        while (client.connected() && millis() < timeout)
        {
            if (client.available())
            {
                char c = client.read();
                request += c;
                if (request.endsWith("\r\n\r\n")) break;
            }
        }

        if (request.indexOf("POST /save") >= 0)
        {
            // 解析 POST body
            String body = "";
            int bodyStart = request.indexOf("\r\n\r\n");
            if (bodyStart >= 0) body = request.substring(bodyStart + 4);

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

                    if (key == "ssid")       nSsid = val;
                    else if (key == "password") nPass = val;
                    else if (key == "devid")      nDevid = val;
                    else if (key == "upurl")      nUpurl = val;
                    else if (key == "v_cnt")      nValveCnt = val.toInt();
                    else if (key == "p_cnt")      nPumpCnt = val.toInt();
                    else if (key == "fld_v")      nFieldV = val.toInt();
                    else if (key == "wh")         nWHour = val.toInt();
                    else if (key == "wm")         nWMin = val.toInt();
                    else if (key == "s_need")     nSoilNeed = val.toInt();
                    else if (key == "a_time")     nAutoTime = val.toInt();
                    else if (key == "a_soil")     nAutoSoil = val.toInt();
                    else if (key == "cw_dur")     nCWdur = val.toInt();
                    else if (key.startsWith("wt_"))
                    {
                        int idx = key.substring(3).toInt();
                        if (idx >= 0 && idx < MAX_VALVES) nWatT[idx] = val.toInt();
                    }
                }
                pos = ampPos + 1;
            }

            // 约束值并写入 NVS
            nValveCnt = constrain(nValveCnt, 1, MAX_VALVES);
            nPumpCnt  = constrain(nPumpCnt, 0, MAX_PUMPS);
            nFieldV   = constrain(nFieldV, 1, nValveCnt);
            nWHour    = constrain(nWHour, 0, 23);
            nWMin     = constrain(nWMin, 0, 59);
            nSoilNeed = constrain(nSoilNeed, 0, 100);
            nCWdur    = constrain(nCWdur, 1, 120);

            Preferences prefs;
            prefs.begin("homefront", false);
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
            html += buildConfigHtml();
            client.print(html);
            client.stop();
        }
    }
}
