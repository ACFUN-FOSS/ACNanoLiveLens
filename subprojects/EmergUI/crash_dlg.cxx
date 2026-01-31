#include "EmergUI/crash_dlg.hxx"
#include "sound.hxx"
#include <cassert>
#include <glfw/glfw3.h>
#include <ui.h>


std::string_view crashReason2Str(const CrashReason reason) {
	switch (reason) {
		case CrashReason::UNHANDLED_EXCP:
		return "应用程序#遇到#错误，#不能#继续#运行。";
		break;
		case CrashReason::ILLEGAL_OP:
		return "应用程序#执行了#非法操作，#不能#继续#运行。";
		break;
		case CrashReason::ASSERT_FAIL:
		return
			"应用程序#陷入#意外状态，#继续#执行#将#导致#不可控#的#行为#——#已#自我#了结。";
		default:
		assert(false && "???");
	}
}

std::tuple<float, float> calcCrashDlgSize() {
	float xScale = 1, yScale = 1;
	if (auto primaryMonitor = glfwGetPrimaryMonitor()) {
		glfwGetMonitorContentScale(primaryMonitor, &xScale, &yScale);
	}

	return { xScale * 500, yScale * 700 };
}

void wrapTextToLinesAndAddToVbox(uiBox &uiBox, std::string_view text) {
	// 斷行。#是唯一的斷行點。
	constexpr size_t maxLineBytes = 110;
	constexpr char breakableChar = '#';
	std::string line;
	bool needNewLine = false;
	for (const char c : text) {
		if (c == breakableChar && needNewLine) {
			// 換行
			uiBoxAppend(&uiBox, uiControl(uiNewLabel(line.c_str())), 0);
			line.clear();
			needNewLine = false;
		} else if (c == '\n') {
			// 强制換行
			uiBoxAppend(&uiBox, uiControl(uiNewLabel(line.c_str())), 0);
			line.clear();
			needNewLine = false;
		} else {
			if (c != breakableChar)
				line += c;
			if (line.length() >= maxLineBytes) {
				needNewLine = true;
			}
		}
	}

	if (!line.empty()) {
		uiBoxAppend(&uiBox, uiControl(uiNewLabel(line.c_str())), 0);
	}
}

void crashDlg(const CrashDlgData data) {

	// 创建窗口（模态对话框风格）
	auto [winW, winH] = calcCrashDlgSize();
	uiWindow *win = uiNewWindow("死了", winW, winH, 1);
	uiWindowSetMargined(win, 1);

	// 主垂直布局
	uiBox *vbox = uiNewVerticalBox();
	uiBoxSetPadded(vbox, 1);
	uiWindowSetChild(win, uiControl(vbox));

	uiBox *textvbox = uiNewVerticalBox();
	uiBoxSetPadded(textvbox, 0);
	uiBoxAppend(vbox, uiControl(textvbox), 0);
	wrapTextToLinesAndAddToVbox(*textvbox, crashReason2Str(data.crashReason));
	wrapTextToLinesAndAddToVbox(*textvbox, "請 將 錯誤信息 報告于 developers。");
	wrapTextToLinesAndAddToVbox(*textvbox, "附加#调试器#到#该#进程，#然后#点击# “OK” #按钮#以#查看#错误#处。#（这#取决于#平台#是否#支持#调试）");
#ifdef WIN32
	//wrapTextToLinesAndAddToVbox(*textvbox, "如果#你#想要#继续#运行#程序，#请#重新#启动#应用程序。");
#endif

	uiMultilineEntry *msgBox = uiNewMultilineEntry();
	uiMultilineEntrySetReadOnly(msgBox, 1);
	uiMultilineEntrySetText(msgBox, std::string(data.msg).c_str());
	uiBoxAppend(vbox, uiControl(msgBox), 1);

	// “确定”按钮
	uiButton *okBtn = uiNewButton("善");
	uiBoxAppend(vbox, uiControl(okBtn), 0);

	uiAreaHandler handler{ };
	uiArea *img = uiNewArea(&handler);
	uiAreaSetSize(img, 200, 200);
	uiBoxAppend(vbox, uiControl(img), 0);


	uiButtonOnClicked(okBtn, [](uiButton *, void *data) {
		//std::exit(-1);
		uiQuit();
	}, nullptr);

	uiWindowOnClosing(win, [](uiWindow *, void *data) {
		//std::exit(-1);
		uiQuit();
		return 0;
	}, nullptr);

	uiControlShow(uiControl(win));

	crashSound();
	uiMain();

}
