/**
 * Chrono-shift 自动更新模块 (骨架实现)
 * C++17 重构版
 */
#include "Updater.h"
#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace app {

Updater::Updater()
    : current_version_("0.0.0")
    , update_url_()
{
}

Updater::~Updater() = default;

int Updater::init(const std::string& current_version, const std::string& update_url)
{
    current_version_ = current_version;
    update_url_ = update_url;

    LOG_INFO("更新模块初始化: 当前版本=%s, 更新地址=%s",
             current_version_.c_str(), update_url_.c_str());
    return 0;
}

int Updater::check(UpdateInfo& info)
{
    info = UpdateInfo{};
    info.current_version = current_version_;
    LOG_DEBUG("检查更新");
    /* TODO: Phase 8 实现 */
    return 0;
}

int Updater::download(const UpdateInfo& info, const std::string& output_path,
                      ProgressCallback callback)
{
    (void)info;
    (void)output_path;
    (void)callback;
    LOG_DEBUG("下载更新");
    /* TODO: Phase 8 实现 */
    return 0;
}

int Updater::install(const std::string& installer_path)
{
    (void)installer_path;
    LOG_DEBUG("安装更新");
    /* TODO: Phase 8 实现 */
    return 0;
}

} // namespace app
} // namespace client
} // namespace chrono
