#ifndef NANOLIVELENS_DANMAKU_MONITOR_WIN_HXX
#define NANOLIVELENS_DANMAKU_MONITOR_WIN_HXX

#include <chrono>
#include <string>
#include <vector>

class DanmakuMonitorWin
{
public:
    struct DanmakuInfo
    {
        std::string sender;
        std::string content;
        std::chrono::system_clock::time_point timestamp;
    };

    DanmakuMonitorWin();
    ~DanmakuMonitorWin();

    DanmakuMonitorWin(const DanmakuMonitorWin &) = delete;
    DanmakuMonitorWin(DanmakuMonitorWin &&) = delete;
    DanmakuMonitorWin &operator=(const DanmakuMonitorWin &) = delete;
    DanmakuMonitorWin &operator=(DanmakuMonitorWin &&) = delete;

    void addDanmaku(const DanmakuInfo &danmaku);
    void clearDanmaku();

private:
    class Impl;
    stdx::pimpl::unique_ptr<Impl> pImpl;
};

#endif
