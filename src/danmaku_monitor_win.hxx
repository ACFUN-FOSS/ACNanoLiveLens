#ifndef NANOLIVELENS_DANMAKU_MONITOR_WIN_HXX
#define NANOLIVELENS_DANMAKU_MONITOR_WIN_HXX

#include "uiwin_bizlogic_obj_async_op_scope.hxx"
#include "Core/aclive_backend_client.hxx"

class DanmakuMonitorWin
{
public:
    struct DanmakuInfo
    {
        std::string sender;
        std::string content;
        std::chrono::system_clock::time_point timestamp;

        // 以下两个辅助访问器只为脚本侧使用（见 src/js_metatypes.hxx）：
        // 时间点本身无法直接映射到 JS 值，故以毫秒数与格式化文本两种形式暴露。
        std::int64_t timestampMs() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
        }

        std::string timestampText() const {
            return std::format("{:%H:%M:%S}", timestamp);
        }
    };

	METAPP_REFLECT
	struct DanmakuGuiInfo
	{
		gsl::not_null<Rml::Element *const> danmakuItemAppearAnimContainerEle;
		gsl::not_null<Rml::Element *const> danmakuEle;
		void test() {
			std::cout << "test: " << std::endl;
		}
	};

	struct DanmakuInGui
	{
		DanmakuGuiInfo guiInfo;
		DanmakuInfo danmakuInfo;
	};

    DanmakuMonitorWin(UiWinBizLogicObjContext<DanmakuMonitorWin> ctx, AcliveBackendClient *client);
    ~DanmakuMonitorWin();

    DanmakuMonitorWin(const DanmakuMonitorWin &) = delete;
    DanmakuMonitorWin(DanmakuMonitorWin &&) = delete;
    DanmakuMonitorWin &operator=(const DanmakuMonitorWin &) = delete;
    DanmakuMonitorWin &operator=(DanmakuMonitorWin &&) = delete;

    void addDanmaku(const DanmakuInfo &danmaku);
    void clearDanmaku();

	UiWinBizLogicObjContext<DanmakuMonitorWin>& getLogicObjCtx();
	RmlUIWin::UiWin &getUiWin();

	//static void setupJsBinding(qjs::Context &ctx);

private:
    class Impl;
    stdx::pimpl::unique_ptr<Impl> pImpl;
	
};

#endif
