#include "sensor.h"
#include "globals.h"

// ---- 内部辅助: 从逗号分隔的16进制字符串中提取湿度值 ----
static float getTemp(String temp)
{
    int commaPosition = -1;
    String info[9];
    for (int i = 0; i < 9; i++)
    {
        commaPosition = temp.indexOf(',');
        if (commaPosition != -1)
        {
            info[i] = temp.substring(0, commaPosition);
            temp = temp.substring(commaPosition + 1, temp.length());
        }
        else
        {
            if (temp.length() > 0)
            {
                info[i] = temp.substring(0, commaPosition);
            }
        }
    }
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

// 检测土壤湿度: 通过 RS485 (UART0) 发送 Modbus RTU 测温命令并解析返回
void check_soil()
{
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

    if (data_soil.length() > 0)
    {
        soil_moisture_into_list(getTemp(data_soil));
        Serial2.print(soil_moisture);
        Serial2.println("%water");
    }
}
