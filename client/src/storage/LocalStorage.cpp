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
    plugins_path_     = app_data_path + "/plugins";
    ext_path_         = app_data_path + "/extensions";
    ai_path_          = app_data_path + "/ai";
    devtools_path_    = app_data_path + "/devtools";
    user_custom_path_ = app_data_path + "/user_custom";

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
    if (ensure_dir(plugins_path_) != 0) {
        LOG_WARN("无法创建插件目录: %s", plugins_path_.c_str());
    }
    if (ensure_dir(ext_path_) != 0) {
        LOG_WARN("无法创建扩展目录: %s", ext_path_.c_str());
    }
    if (ensure_dir(ai_path_) != 0) {
        LOG_WARN("无法创建AI目录: %s", ai_path_.c_str());
    }
    if (ensure_dir(devtools_path_) != 0) {
        LOG_WARN("无法创建开发者工具目录: %s", devtools_path_.c_str());
    }
    if (ensure_dir(user_custom_path_) != 0) {
        LOG_WARN("无法创建用户自定义目录: %s", user_custom_path_.c_str());
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

    std::string file_path = config_path_ + "/" + key + ".json";
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, file_path.c_str(), "w");
#else
    fp = fopen(file_path.c_str(), "w");
#endif
    if (!fp) {
        LOG_ERROR("无法打开配置文件: %s", file_path.c_str());
        return -1;
    }

    fwrite(value.data(), 1, value.size(), fp);
    fclose(fp);

    LOG_DEBUG("保存配置: %s=%s (%zu bytes)",
              key.c_str(), value.c_str(), value.size());
    return 0;
}

int LocalStorage::load_config(const std::string& key, std::string& value)
{
    if (!initialized_) return -1;

    std::string file_path = config_path_ + "/" + key + ".json";
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, file_path.c_str(), "rb");
#else
    fp = fopen(file_path.c_str(), "rb");
#endif
    if (!fp) {
        LOG_DEBUG("配置文件不存在: %s", file_path.c_str());
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size <= 0) {
        fclose(fp);
        return -1;
    }

    rewind(fp);
    value.resize(static_cast<size_t>(file_size));
    size_t bytes_read = fread(&value[0], 1, static_cast<size_t>(file_size), fp);
    fclose(fp);

    if (bytes_read != static_cast<size_t>(file_size)) {
        LOG_ERROR("读取配置不完整: %s", file_path.c_str());
        return -1;
    }

    LOG_DEBUG("加载配置: %s (%zu bytes)", key.c_str(), value.size());
    return 0;
}

int LocalStorage::save_file(const std::string& relative_path,
                            const uint8_t* data, size_t length)
{
    if (!initialized_ || !data) return -1;

    std::string file_path = full_path(relative_path);
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, file_path.c_str(), "wb");
#else
    fp = fopen(file_path.c_str(), "wb");
#endif
    if (!fp) {
        LOG_ERROR("无法打开文件: %s", file_path.c_str());
        return -1;
    }

    size_t written = fwrite(data, 1, length, fp);
    fclose(fp);

    if (written != length) {
        LOG_ERROR("写入文件不完整: %s", file_path.c_str());
        return -1;
    }

    LOG_DEBUG("保存文件: %s (%zu bytes)", relative_path.c_str(), length);
    return 0;
}

int LocalStorage::load_file(const std::string& relative_path,
                            std::vector<uint8_t>& data)
{
    if (!initialized_) return -1;

    std::string file_path = full_path(relative_path);
    FILE* fp = nullptr;
#ifdef _WIN32
    fopen_s(&fp, file_path.c_str(), "rb");
#else
    fp = fopen(file_path.c_str(), "rb");
#endif
    if (!fp) {
        LOG_DEBUG("文件不存在: %s", file_path.c_str());
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size <= 0) {
        fclose(fp);
        return -1;
    }

    rewind(fp);
    data.resize(static_cast<size_t>(file_size));
    size_t bytes_read = fread(data.data(), 1, static_cast<size_t>(file_size), fp);
    fclose(fp);

    if (bytes_read != static_cast<size_t>(file_size)) {
        LOG_ERROR("读取文件不完整: %s", file_path.c_str());
        return -1;
    }

    LOG_DEBUG("加载文件: %s (%zu bytes)", relative_path.c_str(), data.size());
    return 0;
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
