/**
 * Chrono-shift 自动更新模块
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_UPDATER_H
#define CHRONO_CLIENT_UPDATER_H

#include <string>
#include <functional>

namespace chrono {
namespace client {
namespace app {

/**
 * 更新信息
 */
struct UpdateInfo {
    std::string current_version;   ///< 当前版本
    std::string latest_version;    ///< 最新版本
    std::string download_url;      ///< 下载地址
    bool        update_available = false; ///< 是否有可用更新
};

/**
 * 自动更新器
 */
class Updater {
public:
    using ProgressCallback = std::function<void(int percent, const std::string& status)>;

    Updater();
    ~Updater();

    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;
    Updater(Updater&&) = default;
    Updater& operator=(Updater&&) = default;

    /**
     * 初始化更新模块
     * @param current_version 当前版本号
     * @param update_url      更新检查地址
     * @return 0 成功, -1 失败
     */
    int init(const std::string& current_version, const std::string& update_url);

    /**
     * 检查更新
     * @param[out] info 更新信息
     * @return 0 成功, -1 失败
     */
    int check(UpdateInfo& info);

    /**
     * 下载更新
     * @param info        更新信息
     * @param output_path 下载输出路径
     * @param callback    进度回调 (可选)
     * @return 0 成功, -1 失败
     */
    int download(const UpdateInfo& info, const std::string& output_path,
                 ProgressCallback callback = nullptr);

    /**
     * 安装更新
     * @param installer_path 安装包路径
     * @return 0 成功, -1 失败
     */
    int install(const std::string& installer_path);

    /** 获取当前版本 */
    const std::string& current_version() const { return current_version_; }

    /** 获取更新检查地址 */
    const std::string& update_url() const { return update_url_; }

private:
    std::string current_version_;
    std::string update_url_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_UPDATER_H
