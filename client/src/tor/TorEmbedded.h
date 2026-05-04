#pragma once
/**
 * TorEmbedded.h — Tor 内嵌管理 (子进程模式)
 *
 * chrono-client 启动/管理 tor.exe 子进程，通过 SOCKS5:9050 + Control:9051 通信。
 * Tor 源码位于 C:\Users\haiyan\tor，通过其 autotools 构建为 tor.exe。
 */
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace chrono { namespace client { namespace tor {

enum class TorState {
    Stopped,
    Starting,
    Bootstrapping,   // 正在建立电路
    Running,         // 就绪
    Failed
};

class TorEmbedded {
public:
    static TorEmbedded& instance();

    /** 启动内嵌 Tor (后台线程) */
    bool start(const std::string& tor_exe_path = "./tor.exe",
               const std::string& data_dir = "./tor_data");

    /** 停止 Tor */
    void stop();

    /** 获取当前状态 */
    TorState state() const { return state_; }

    /** 获取启动日志 (最近 N 行) */
    std::string get_log() const;

    /** 获取 Bootstrap 进度 (0-100) */
    int bootstrap_progress() const { return bootstrap_pct_; }

    /** 获取电路数量 */
    int circuit_count() const { return circuits_; }

    /** 是否已就绪 */
    bool is_ready() const { return state_ == TorState::Running; }

    /** 获取 onion 地址 */
    const std::string& onion_addr() const { return onion_addr_; }

    /** 状态变更回调 */
    void on_state_change(std::function<void(TorState)> cb) { state_cb_ = cb; }

private:
    TorEmbedded() = default;
    ~TorEmbedded();
    void run_event_loop();

    std::thread worker_;
    std::atomic<TorState> state_{TorState::Stopped};
    std::atomic<int> bootstrap_pct_{0};
    std::atomic<int> circuits_{0};
    std::string onion_addr_;
    std::string log_buf_;
    mutable std::mutex log_mutex_;
    std::function<void(TorState)> state_cb_;
};

} } } // namespace
