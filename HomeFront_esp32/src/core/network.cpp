#include "network.h"
#include "globals.h"

// 非阻塞 WiFi 重连
void wifi_reconnect_cx()
{
    static unsigned long lastAttemptTime = 0;

    if (WiFi.status() == WL_CONNECTED)
    {
        wifi_retry_times = 0;
        NET_LOSTING_FLAG = false;
        return;
    }

    if (millis() - lastAttemptTime >= wifiRetryInterval)
    {
        lastAttemptTime = millis();
        wifi_retry_times++;

        // 每 15 次重试 (~4.5s) 触发一次 WiFi 重连
        if (wifi_retry_times % 15 == 0)
        {
            WiFi.reconnect();
        }

        if (wifi_retry_times < 150)
        {
            Serial.print(".");
        }
        else
        {
            if (soil2wat == 1)
            {
                Serial.println("使用millis()进行猜测时间");
                NET_LOSTING_FLAG = true;
                NET_LOSTING_time = timeinfo;
                BEGIN_TIMESTAMP = millis();
            }
            else
            {
                wifi_to_reboot_times++;
                if (wifi_to_reboot_times > 500)
                {
                    Serial.println("准备重启");
                    ESP.restart();
                }
            }
        }
    }
}

// 非阻塞检测客户端（tlink服务器）连接
void check_client_connected()
{
    static unsigned long lastAttemptTime = 0;
    static bool ticker_attached = false;

    if (client.connected())
    {
        wifi_to_reboot_times = 0;
        if (!ticker_attached)
        {
            tk.attach(40, time_fun);
            ticker_attached = true;
            Serial.println("send device_id");
            client.print(device_id);
        }
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (millis() - lastAttemptTime >= clientRetryInterval)
    {
        lastAttemptTime = millis();

        Serial.println("尝试重新连接服务器...");
        client.connect(host, httpPort);

        if (client.connected())
        {
            Serial.println("TCP connected, sending device_id");
            client.print(device_id);
            if (!ticker_attached)
            {
                tk.attach(40, time_fun);
                ticker_attached = true;
            }
        }
    }

    if (!client.connected())
    {
        wifi_to_reboot_times++;
        if (wifi_to_reboot_times > 500)
        {
            Serial.println("准备重启");
            ESP.restart();
        }
    }
}
