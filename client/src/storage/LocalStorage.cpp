/**
 * Chrono-shift 本地存储管理
 * C++17 重构版
 */
#include "LocalStorage.h"
#include "../util/Logger.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define ACCESS _access
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/types.h>
#define ACCESS access
#define MKDIR(p) mkdir(p, 0755)
#endif

namespace chrono {
namespace client {
namespace storage {

LocalStorage::LocalStorage() = default;
LocalStorage::~LocalStorage() = default;

int LocalStorage::init(const std::string& app_data_path)
{
    base_path_   = app_data_path;
    config_path_ = app_data_path + "/config";
    cache_path_  = app_data_path + "/cache";
    themes_path_ = app_data_path + "/themes";

    /* 创建必要目录 */
    if (ensure_dir(config_path_) != 0) {
        LOG_ERROR("无法创建配置目录: %s", config_path_.c_str());
        return -1;
    }
    if (ensure_dir(cache_path_) != 0) {
        LOG_ERROR("无法创建缓存目录: %s", cache_path_.c_str());
        return -1;
    }
    if (ensure_dir(themes_path_) != 0) {
        LOG_ERROR("无法创建主题目录: %s", themes_path_.c_str());
        return -1;
    }

    initialized_ = true;
    LOG_INFO("本地存储初始化完成: %s", app_data_path.c_str());
    return 0;
}

int LocalStorage::ensure_dir(const std::string& path)
{
#ifdef _WIN32
    if (ACCESS(path.c_str(), 0) != 0) {
        return MKDIR(path.c_str());
    }
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return MKDIR(path.c_str());
    }
#endif
    return 0;
}

std::string LocalStorage::full_path(const std::string& relative_path) const
{
    return base_path_ + "/" + relative_path;
}

int LocalStorage::save_config(const std::string& key, const std::string& value)
{
    if (!initialized_) return -1;
    (void)key;
    (void)value;
    /* TODO: Phase 3 实现 */
    LOG_DEBUG("保存配置: %s=%s", key.c_str(), value.c_str());
    return 0;
}

int LocalStorage::load_config(const std::string& key, std::string& value)
{
    if (!initialized_) return -1;
    (void)key;
    (void)value;
    /* TODO: Phase 3 实现 */
    LOG_DEBUG("加载配置: %s", key.c_str());
    return -1;
}

int LocalStorage::save_file(const std::string& relative_path,
                            const uint8_t* data, size_t length)
{
    if (!initialized_ || !data) return -1;
    (void)relative_path;
    (void)data;
    (void)length;
    /* TODO: Phase 5 实现 */
    LOG_DEBUG("保存文件: %s (%zu bytes)", relative_path.c_str(), length);
    return 0;
}

int LocalStorage::load_file(const std::string& relative_path,
                            std::vector<uint8_t>& data)
{
    if (!initialized_) return -1;
    (void)relative_path;
    (void)data;
    /* TODO: Phase 5 实现 */
    LOG_DEBUG("加载文件: %s", relative_path.c_str());
    return -1;
}

int LocalStorage::delete_file(const std::string& relative_path)
{
    if (!initialized_) return -1;
    std::string fp = full_path(relative_path);
    if (std::remove(fp.c_str()) != 0) {
        LOG_DEBUG("删除文件失败: %s", fp.c_str());
        return -1;
    }
    LOG_DEBUG("删除文件: %s", fp.c_str());
    return 0;
}

bool LocalStorage::file_exists(const std::string& relative_path)
{
    if (!initialized_) return false;
    std::string fp = full_path(relative_path);
#ifdef _WIN32
    return ACCESS(fp.c_str(), 0) == 0;
#else
    struct stat st;
    return stat(fp.c_str(), &st) == 0;
#endif
}

} // namespace storage
} // namespace client
} // namespace chrono
