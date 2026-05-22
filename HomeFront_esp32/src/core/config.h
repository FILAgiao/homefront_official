#pragma once

// NVS 读取已保存的所有配置
void loadConfig();

// 将当前所有参数写入 NVS
void saveAllConfig();

// 启动 AP 模式 + HTTP 配置门户 (阻塞式, 配置完成后重启)
void startConfigPortal();
