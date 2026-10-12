#include "script_style.hxx"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StreamMemory.h>
#include <RmlUi/Core/StyleSheetContainer.h>

#include <algorithm>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

namespace ScriptStyle {
namespace {

struct Sheet
{
	std::string name;   // 报错时标识来源
	std::string source; // RCSS 全文
};

// 注册过的脚本样式表，按注册顺序排列：后注册者与先注册者同等特异性时胜出
//（组合时把脚本样式放在文档自身样式表之后）。
std::vector<Sheet> &sheets() {
	static std::vector<Sheet> value;
	return value;
}

// sheets() 编译出的样式容器缓存。为 true 时表示需要重新编译。
bool &containerDirty() {
	static bool value = true;
	return value;
}

// 由 sheets() 编译出的容器。没有任何可用样式表时为空。
Rml::SharedPtr<Rml::StyleSheetContainer> &scriptContainer() {
	static Rml::SharedPtr<Rml::StyleSheetContainer> value;
	return value;
}

// 与某个文档相关的样式状态。
struct DocumentStyle
{
	// 文档**自身**的样式表，即脚本样式叠加的基座。
	// StyleSheetContainer 不可拷贝，因此保存的是它的克隆。
	Rml::SharedPtr<Rml::StyleSheetContainer> base;
	// 我们最后让文档持有的容器指针。文档换成别的指针，就说明容器被外部替换过。
	const Rml::StyleSheetContainer *expected = nullptr;
	// 组合出 expected 时所用的脚本样式容器，用于判断文档是否已是最新状态。
	const Rml::StyleSheetContainer *scriptUsed = nullptr;
};

std::unordered_map<const Rml::ElementDocument *, DocumentStyle> &documentStyles() {
	static std::unordered_map<const Rml::ElementDocument *, DocumentStyle> value;
	return value;
}

// 克隆一份样式容器：StyleSheetContainer 不可拷贝，
// 但「和空容器组合」会新建一个包含全部内容的新容器，效果等同克隆。
Rml::SharedPtr<Rml::StyleSheetContainer> cloneOf(const Rml::StyleSheetContainer *container) {
	if (!container)
		return { };
	return container->CombineStyleSheetContainer(Rml::StyleSheetContainer{ });
}

// 把注册的样式表编译成一个容器。逐份 Load 即可：LoadStyleSheetContainer 会向
// 已有容器**追加**媒体块，而不是替换。
Rml::SharedPtr<Rml::StyleSheetContainer> buildScriptContainer() {
	auto container = Rml::MakeShared<Rml::StyleSheetContainer>();
	bool hasAnySheet = false;

	for (const Sheet &sheet : sheets()) {
		Rml::StreamMemory stream{
			reinterpret_cast<const Rml::byte *>(sheet.source.data()), sheet.source.size()
		};
		stream.SetSourceURL(sheet.name);
		if (container->LoadStyleSheetContainer(&stream))
			hasAnySheet = true;
		else
			std::println(stderr, "脚本样式表 {} 解析失败，已忽略。", sheet.name);
	}

	if (!hasAnySheet)
		return { };
	return container;
}

// 让一个文档持有「文档自身样式表 + 当前脚本样式」。
void reconcile(Rml::ElementDocument &document) {
	DocumentStyle &style = documentStyles()[&document];
	const Rml::StyleSheetContainer *current = document.GetStyleSheetContainer();

	if (current != style.expected) {
		// 文档现在的容器不是我们装上去的那一个，说明它被外部替换过
		//（首次加载、F6 的 ReloadStyleSheet()、Ctrl+R 的 LoadDocument）。
		// 此刻容器里只有文档自身的样式表，正好作为基座重新记下来。
		style.base = cloneOf(current);
		style.expected = current;
		style.scriptUsed = nullptr;
	}

	if (style.scriptUsed == scriptContainer().get())
		return; // 脚本样式没变，文档已是最新状态。

	Rml::SharedPtr<Rml::StyleSheetContainer> next;
	if (!scriptContainer()) {
		// 脚本样式已被清空（例如热重载后新脚本不再注册样式）：退回文档自身的样式表。
		next = style.base;
	} else {
		// 脚本样式放在文档自身样式表之后：同等特异性下脚本样式胜出。
		next = style.base
			? style.base->CombineStyleSheetContainer(*scriptContainer())
			: cloneOf(scriptContainer().get());
	}

	document.SetStyleSheetContainer(next);
	std::println("[临时] 文档 {} 已挂上脚本样式（脚本样式表 {} 份，基座 {}）",
		document.GetSourceURL(), sheets().size(), style.base ? "有" : "无");
	style.expected = next ? next.get() : current;
	style.scriptUsed = scriptContainer().get();
}

}

bool add(const std::string_view name, const std::string_view rcss) {
	// 先试解析一次，让脚本能在注册失败时立刻拿到 false（而不是等到下次对账才发现）。
	{
		Rml::StyleSheetContainer probe;
		Rml::StreamMemory stream{
			reinterpret_cast<const Rml::byte *>(rcss.data()), rcss.size()
		};
		stream.SetSourceURL(std::string{ name });
		if (!probe.LoadStyleSheetContainer(&stream)) {
			std::println(stderr, "addScriptStyle(\"{}\") 失败：RCSS 解析出错。", name);
			return false;
		}
	}

	sheets().push_back(Sheet{ std::string{ name }, std::string{ rcss } });
	containerDirty() = true;
	return true;
}

void clear() {
	if (sheets().empty() && !scriptContainer())
		return;

	sheets().clear();
	containerDirty() = true;
	// 立即对账一次：脚本样式的失效不必等到下一帧。
	syncAll();
}

void syncAll() {
	if (containerDirty()) {
		scriptContainer() = buildScriptContainer();
		containerDirty() = false;
	}

	// 当前活着的文档（所有上下文的所有文档）。
	std::vector<Rml::ElementDocument *> liveDocuments;
	for (int i = 0, numContexts = Rml::GetNumContexts(); i < numContexts; ++i) {
		Rml::Context *context = Rml::GetContext(i);
		if (!context)
			continue;
		for (int j = 0, numDocuments = context->GetNumDocuments(); j < numDocuments; ++j) {
			if (Rml::ElementDocument *document = context->GetDocument(j))
				liveDocuments.push_back(document);
		}
	}

	// 先清理已销毁文档的记录：键是裸指针，留着会一直持有它最后一份样式容器。
	std::erase_if(documentStyles(), [&](const auto &entry) {
		return std::ranges::find(liveDocuments, entry.first) == liveDocuments.end();
	});

	for (Rml::ElementDocument *document : liveDocuments)
		reconcile(*document);

	// TEMP-VERIFY-BEGIN
	static int tempFrame = 0;
	if (++tempFrame == 200 || tempFrame == 400) {
		for (Rml::ElementDocument *document : liveDocuments) {
			Rml::Element *list = document->GetElementById("danmaku-list");
			if (!list)
				continue;
			std::println("[临时] 第 {} 帧：danmaku-list 有 {} 个子元素", tempFrame, list->GetNumChildren());
			for (int i = 0, n = list->GetNumChildren(); i < n && i < 6; ++i) {
				Rml::Element *wrapper = list->GetChild(i);
				std::println("[临时]   包装[{}] 高={}", i, wrapper->GetOffsetHeight());
				for (int j = 0, m = wrapper->GetNumChildren(); j < m; ++j) {
					Rml::Element *child = wrapper->GetChild(j);
					std::println("[临时]     子[{}] <{}> 类=\"{}\" 高={}", j,
						child->GetTagName(), child->GetClassNames(), child->GetOffsetHeight());
				}
			}
		}
	}
	// TEMP-VERIFY-END
}

}
