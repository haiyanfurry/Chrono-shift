#ifndef CHRONO_UPDATER_H
#define CHRONO_UPDATER_H

#include <stdbool.h>

/* ============================================================
 * 自动更新模块
 * ============================================================ */

typedef struct {
    char current_version[32];
    char latest_version[32];
    char download_url[1024];
    bool update_available;
} UpdateInfo;

/* --- API --- */
int  updater_init(const char* current_version, const char* update_url);
int  updater_check(UpdateInfo* info);
int  updater_download(const UpdateInfo* info, const char* output_path);
int  updater_install(const char* installer_path);

#endif /* CHRONO_UPDATER_H */
