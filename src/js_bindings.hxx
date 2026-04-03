#ifndef NANOLIVELENS_JS_BINDINGS_HXX
#define NANOLIVELENS_JS_BINDINGS_HXX

#include "danmaku_monitor_win.hxx"
#include "rmlui_sys.hxx"

class JSBindings
{
public:
	JSBindings() = delete;
	~JSBindings() = delete;
	JSBindings(const JSBindings &) = delete;
	JSBindings(JSBindings &&) = delete;
	JSBindings &operator=(const JSBindings &) = delete;
	JSBindings &operator=(JSBindings &&) = delete;

	static void init(RmlUISystem &rmluiSys, DanmakuMonitorWin &danmakuMonitorWin);
	static void shutdown();
	static void evalString(const std::string &code);
	static void evalFile(const std::filesystem::path &filePath);
};

#endif
