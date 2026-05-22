#pragma once
#include <Arduino.h>

// 字符串分割函数: 按 fen 分隔 str, 取第 index 段
String fenge(String str, String fen, int index);

// 拼接遥测数据并发送到 TCP 服务器
void send2clinet();

// 处理收到的 TCP 消息, 分发到各命令
void handle_incoming_message(const String &ch);
