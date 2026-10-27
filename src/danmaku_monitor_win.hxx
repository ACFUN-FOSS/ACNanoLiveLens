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
        // 直接暴露为属性即可：脚本读到的是 JS Date（时间点的转换由
        // ElementQuickJsBinding 统一提供，见 metapp_chrono_metatypes.hxx），
        // 不需要任何辅助访问器。
        std::chrono::system_clock::time_point timestamp;
    };

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
