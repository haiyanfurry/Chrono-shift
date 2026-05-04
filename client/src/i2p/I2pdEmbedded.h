#pragma once
/**
 * I2pdEmbedded.h — i2pd 内嵌管理 (源码级嵌入)
 *
 * 编译 libi2pd + libi2pd_client 静态库并链接进 chrono-client。
 * i2pd 源码位于 C:\Users\haiyan\i2pd
 * 在后台线程启动 I2P 路由器，内部提供 SAM API。
 */
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

namespace chrono { namespace client { namespace i2p {

enum class I2pdState {
    Stopped,
    Starting,
    Reseeding,       // 正在从 reseed 服务器获取网络数据库
    Integrating,     // 正在集成到 I2P 网络
    Running,         // 就绪
    Failed
};

class I2pdEmbedded {
public:
    static I2pdEmbedded& instance();

    /** 启动内嵌 I2P 路由器 (后台线程)
     *  @param data_dir  数据目录 (存储 netDb, 密钥等)
     *  @param sam_port  SAM API 端口 (默认 7656)
     */
    bool start(const std::string& data_dir = "./i2p_data",
               uint16_t sam_port = 7656);

    /** 停止路由器 */
    void stop();

    /** 获取当前状态 */
    I2pdState state() const { return state_; }

    /** 获取启动日志 (最近 N 行) */
    std::string get_log() const;

    /** 获取集成进度信息 */
    std::string get_integration_status() const;

    /** 隧道数量 */
    int tunnel_count() const { return tunnels_; }

    /** 已知节点数 */
    int known_nodes() const { return nodes_; }

    /** 正常运行时间 (秒) */
    int uptime_seconds() const;

    /** 是否已就绪 */
    bool is_ready() const { return state_ == I2pdState::Running; }

    /** 获取我们的 .b32.i2p 地址 */
    const std::string& our_address() const { return our_addr_; }

    /** 状态变更回调 */
    void on_state_change(std::function<void(I2pdState)> cb) { state_cb_ = cb; }

private:
    I2pdEmbedded() = default;
    ~I2pdEmbedded();
    void run_router();

    std::thread worker_;
    std::atomic<I2pdState> state_{I2pdState::Stopped};
    std::atomic<int> tunnels_{0};
    std::atomic<int> nodes_{0};
    std::string our_addr_;
    std::string log_buf_;
    mutable std::mutex log_mutex_;
    std::chrono::steady_clock::time_point start_time_;
    std::function<void(I2pdState)> state_cb_;
    uint16_t sam_port_ = 7656;
    std::string data_dir_;
};

} } } // namespace
