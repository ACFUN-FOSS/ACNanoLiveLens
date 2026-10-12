#include "js_hook.hxx"

#include <algorithm>
#include <exception>
#include <print>
#include <string_view>
#include <utility>
#include <vector>

namespace JsHook {

namespace {

// 一个钩子的回调表：id 与回调成对保存，以便按 id 注销。
template <typename Callback>
using HookList = std::vector<std::pair<HookId, Callback>>;

// 全部钩子的回调表。一个应用只有一份脚本引擎，故用文件内的单例即可。
struct Registry
{
	HookList<SysStartUpCallback> sysStartUp;
	HookList<PostLoginCallback> postLogin;
	HookList<ReceivedDanmakuCallback> onReceivedDanmaku;
	HookList<ShowedDanmakuCallback> postShowedDanmaku;
	HookList<DanmakuDropCallback> onDanmakuDrop;
	HookList<HotreloadStyleCallback> postHotreloadStyle;
	HookList<HotreloadFullCallback> postHotreloadFull;

	HookId nextId = 1;
};

Registry &registry() {
	static Registry instance;
	return instance;
}

template <typename Callback>
HookId addHook(HookList<Callback> &hooks, Callback callback) {
	const HookId id = registry().nextId++;
	hooks.emplace_back(id, std::move(callback));
	return id;
}

template <typename Callback>
bool removeHookFrom(HookList<Callback> &hooks, const HookId id) {
	return std::erase_if(hooks, [id](const auto &entry) {
		return entry.first == id;
	}) != 0;
}

// 按注册顺序调用一个钩子的全部回调。
//
// 回调在执行过程中可能注册或注销钩子，故先对回调表取一份副本：遍历的是副本，
// 因此容器变化不会影响本次遍历；调用前再按 id 复查一次，已经注销的回调不再调用。
template <typename Callback, typename Invoke>
void triggerHooks(const std::string_view hookName, const HookList<Callback> &hooks, Invoke &&invoke) {
	const HookList<Callback> snapshot{ hooks };
	for (const auto &[id, callback] : snapshot) {
		const bool stillRegistered = std::ranges::any_of(hooks, [id](const auto &entry) {
			return entry.first == id;
		});
		if (!stillRegistered)
			continue;

		try {
			invoke(callback);
		} catch (const std::exception &ex) {
			// 单个回调失败既不能影响其余回调，也不能影响应用本身。
			std::println(stderr, "钩子 {} 的回调 #{} 抛出异常：{}", hookName, id, ex.what());
		} catch (...) {
			std::println(stderr, "钩子 {} 的回调 #{} 抛出未知异常。", hookName, id);
		}
	}
}

}

HookId addSysStartUpHook(SysStartUpCallback callback) {
	return addHook(registry().sysStartUp, std::move(callback));
}

HookId addPostLoginHook(PostLoginCallback callback) {
	return addHook(registry().postLogin, std::move(callback));
}

HookId addOnReceivedDanmakuHook(ReceivedDanmakuCallback callback) {
	return addHook(registry().onReceivedDanmaku, std::move(callback));
}

HookId addPostShowedDanmakuHook(ShowedDanmakuCallback callback) {
	return addHook(registry().postShowedDanmaku, std::move(callback));
}

HookId addOnDanmakuDropHook(DanmakuDropCallback callback) {
	return addHook(registry().onDanmakuDrop, std::move(callback));
}

HookId addPostHotreloadStyleHook(HotreloadStyleCallback callback) {
	return addHook(registry().postHotreloadStyle, std::move(callback));
}

HookId addPostHotreloadFullHook(HotreloadFullCallback callback) {
	return addHook(registry().postHotreloadFull, std::move(callback));
}

bool removeHook(const HookId id) {
	auto &reg = registry();

	bool removed = false;
	// 直接调用而不是用 || 串联：短路求值会跳过后面钩子的移除。
	const auto tryRemove = [&](auto &hooks) {
		if (removeHookFrom(hooks, id))
			removed = true;
	};

	tryRemove(reg.sysStartUp);
	tryRemove(reg.postLogin);
	tryRemove(reg.onReceivedDanmaku);
	tryRemove(reg.postShowedDanmaku);
	tryRemove(reg.onDanmakuDrop);
	tryRemove(reg.postHotreloadStyle);
	tryRemove(reg.postHotreloadFull);
	return removed;
}

void clear() {
	auto &reg = registry();
	reg.sysStartUp.clear();
	reg.postLogin.clear();
	reg.onReceivedDanmaku.clear();
	reg.postShowedDanmaku.clear();
	reg.onDanmakuDrop.clear();
	reg.postHotreloadStyle.clear();
	reg.postHotreloadFull.clear();
	reg.nextId = 1;
}

void triggerSysStartUp(const TimePoint time) {
	triggerHooks("SysStartUp", registry().sysStartUp, [time](const SysStartUpCallback &callback) {
		callback(time);
	});
}

void triggerPostLogin() {
	triggerHooks("PostLogin", registry().postLogin, [](const PostLoginCallback &callback) {
		callback();
	});
}

void triggerOnReceivedDanmaku(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, const TimePoint time) {
	triggerHooks("OnReceivedDanmaku", registry().onReceivedDanmaku, [&](const ReceivedDanmakuCallback &callback) {
		callback(danmakuInfo, time);
	});
}

void triggerPostShowedDanmaku(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, Rml::Element &danmakuEle) {
	triggerHooks("PostShowedDanmaku", registry().postShowedDanmaku, [&](const ShowedDanmakuCallback &callback) {
		callback(danmakuInfo, danmakuEle);
	});
}

void triggerOnDanmakuDrop(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, Rml::Element &danmakuEle) {
	triggerHooks("OnDanmakuDrop", registry().onDanmakuDrop, [&](const DanmakuDropCallback &callback) {
		callback(danmakuInfo, danmakuEle);
	});
}

void triggerPostHotreloadStyle() {
	triggerHooks("PostHotreloadStyle", registry().postHotreloadStyle, [](const HotreloadStyleCallback &callback) {
		callback();
	});
}

void triggerPostHotreloadFull() {
	triggerHooks("PostHotreloadFull", registry().postHotreloadFull, [](const HotreloadFullCallback &callback) {
		callback();
	});
}

std::size_t hookCount() {
	const auto &reg = registry();
	return reg.sysStartUp.size() + reg.postLogin.size() + reg.onReceivedDanmaku.size()
		+ reg.postShowedDanmaku.size() + reg.onDanmakuDrop.size()
		+ reg.postHotreloadStyle.size() + reg.postHotreloadFull.size();
}

}
