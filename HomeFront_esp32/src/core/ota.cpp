#include "ota.h"
#include "globals.h"
#include <HTTPUpdate.h>

// 当升级开始时，打印日志
void update_started()
{
    Serial2.println("CALLBACK:  HTTP update process started");
}

// 当升级结束时，打印日志
void update_finished()
{
    Serial2.println("CALLBACK:  HTTP update process finished");
}

// 当升级中，打印日志
void update_progress(int cur, int total)
{
    Serial2.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

// 当升级失败时，打印日志
void update_error(int err)
{
    Serial2.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

/**
 * 固件升级函数
 * 在需要升级的地方，加上这个函数即可，例如setup中加的updateBin();
 * 原理：通过http请求获取远程固件，实现升级
 */
void updateBin()
{
    Serial2.println("start update");
    WiFiClient UpdateClient;

    // 如果是旧版esp32 SDK，需要删除下面四行，旧版不支持，不然会报错
    // httpUpdate.onStart(update_started);
    // httpUpdate.onEnd(update_finished);
    // httpUpdate.onProgress(update_progress);
    // httpUpdate.onError(update_error);

    t_httpUpdate_return ret = httpUpdate.update(UpdateClient, upUrl);
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        Serial2.println("[update] Update failed.");
        ota_feedback = "[update] Update failed.";
        break;
    case HTTP_UPDATE_NO_UPDATES:
        Serial2.println("[update] Update no Update.");
        ota_feedback = "[update] Update no Update.";
        break;
    case HTTP_UPDATE_OK:
        Serial2.println("[update] Update ok.");
        ota_feedback = "[update] Update ok.";
        break;
    }
}
