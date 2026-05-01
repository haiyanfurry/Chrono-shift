/**
 * Chrono-shift 本地存储管理
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_LOCAL_STORAGE_H
#define CHRONO_CLIENT_LOCAL_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>

namespace chrono {
namespace client {
namespace storage {

/**
 * 本地存储管理器
 * 管理配置文件、缓存、主题文件
 */
class LocalStorage {
public:
    LocalStorage();
    ~LocalStorage();

    LocalStorage(const LocalStorage&) = delete;
    LocalStorage& operator=(const LocalStorage&) = delete;
    LocalStorage(LocalStorage&&) = default;
    LocalStorage& operator=(LocalStorage&&) = default;

    /**
     * 初始化存储
     * @param app_data_path 应用数据根路径
     * @return 0 成功, -1 失败
     */
    int init(const std::string& app_data_path);

    /** 获取各路径 */
    const std::string& base_path()   const { return base_path_; }
    const std::string& config_path()  const { return config_path_; }
    const std::string& cache_path()   const { return cache_path_; }
    const std::string& themes_path()  const { return themes_path_; }

    // ---- 配置管理 ----

    /**
     * 保存配置项
     * @param key   键
     * @param value 值
     * @return 0 成功, -1 失败
     */
    int save_config(const std::string& key, const std::string& value);

    /**
     * 加载配置项
     * @param key        键
     * @param value[out] 值
     * @return 0 成功, -1 失败 (键不存在返回 -1)
     */
    int load_config(const std::string& key, std::string& value);

    // ---- 文件管理 ----

    /**
     * 保存文件到存储目录
     * @param relative_path 相对路径 (如 "cache/avatar.png")
     * @param data          数据
     * @param length        数据长度
     * @return 0 成功, -1 失败
     */
    int save_file(const std::string& relative_path, const uint8_t* data, size_t length);

    /**
     * 加载文件
     * @param relative_path 相对路径
     * @param data[out]     数据
     * @return 0 成功, -1 失败
     */
    int load_file(const std::string& relative_path, std::vector<uint8_t>& data);

    /**
     * 删除文件
     * @param relative_path 相对路径
     * @return 0 成功, -1 失败
     */
    int delete_file(const std::string& relative_path);

    /**
     * 检查文件是否存在
     * @param relative_path 相对路径
     * @return true 存在, false 不存在
     */
    bool file_exists(const std::string& relative_path);

private:
    /**
     * 创建目录 (如果不存在)
     * @param path 目录路径
     * @return 0 成功, -1 失败
     */
    int ensure_dir(const std::string& path);

    /**
     * 获取完整路径
     * @param relative_path 相对路径
     * @return 完整路径字符串
     */
    std::string full_path(const std::string& relative_path) const;

    std::string base_path_;
    std::string config_path_;
    std::string cache_path_;
    std::string themes_path_;
    bool initialized_ = false;
};

} // namespace storage
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_LOCAL_STORAGE_H
