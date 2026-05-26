#include "sensor.h"
#include "globals.h"

// ---- 内部辅助: 从逗号分隔的16进制字符串中提取湿度值 ----
// 返回 -1 表示解析失败 (字段不足等异常)
static float getTemp(String temp)
{
    int commaPosition = -1;
    String info[9];
    int i = 0;
    do
    {
        commaPosition = temp.indexOf(',');
        if (commaPosition != -1)
        {
            if (i >= 9) break;
            info[i] = temp.substring(0, commaPosition);
            temp = temp.substring(commaPosition + 1);
            i++;
        }
        else
        {
            if (temp.length() > 0 && i < 9)
                info[i] = temp;
            i++;
        }
    } while (commaPosition >= 0);

    if (i < 5) return -1.0f;  // 需要至少 5 个字段
    return (info[3].toInt() * 256 + info[4].toInt()) / 10.00;
}

// ---- 内部辅助: 将单次湿度读数平滑写入全局变量 ----
static void soil_moisture_into_list(float soil_m)
{
    if (0 != soil_m)
    {
        if (soil_moisture_list_size < soil_moisture_test_maxsize)
        {
            soil_moisture_list_size += 1;
        }
        soil_moisture = (soil_moisture * (soil_moisture_list_size - 1)) / soil_moisture_list_size
                      + soil_m / soil_moisture_list_size;
    }
}

// 检测土壤湿度: 通过 RS485 (UART0, GPIO1/3) 发送 Modbus RTU 测温命令并解析返回
// UART0 与 USB 调试共用, RS485 通信期间短暂切换到 4800 baud
void check_soil()
{
    // 确保调试输出发送完毕, 切换到 RS485 波特率
    Serial.flush();
    Serial.updateBaudRate(4800);

    for (int i = 0; i < 8; i++)
    {
        Serial.write(item[i]);
    }
    delay(100);
    data_soil = "";
    while (Serial.available())
    {
        unsigned char in = (unsigned char)Serial.read();
        data_soil += in;
        data_soil += ',';
    }

    // 恢复调试波特率
    Serial.updateBaudRate(115200);

    if (data_soil.length() > 0)
    {
        float val = getTemp(data_soil);
        if (val >= 0)
        {
            soil_moisture_into_list(val);
            Serial.print(soil_moisture);
            Serial.println("%water");
        }
    }
}
