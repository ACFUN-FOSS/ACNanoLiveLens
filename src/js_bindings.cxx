#include "js_bindings.hxx"
#include "rmluipp.hxx"
#include "appstate.hxx"

using namespace RmlUIWin;

struct JSContextData
{
	duk_context *ctx = nullptr;
	DanmakuMonitorWin *danmakuMonitorWin = nullptr;
};

static JSContextData g_jsContextData;

static duk_ret_t js_addDanmaku(duk_context *ctx)
{
	if (duk_get_top(ctx) < 2)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "addDanmaku requires at least 2 arguments");
		return DUK_RET_TYPE_ERROR;
	}

	const char *sender = duk_get_string(ctx, 0);
	const char *content = duk_get_string(ctx, 1);

	if (!sender || !content)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid arguments");
		return DUK_RET_TYPE_ERROR;
	}

	int64_t timestampMs = 0;
	if (duk_get_top(ctx) >= 3)
	{
		timestampMs = duk_get_number(ctx, 2);
	}
	else
	{
		timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count();
	}

	if (g_jsContextData.danmakuMonitorWin)
	{
		g_jsContextData.danmakuMonitorWin->addDanmaku({
			sender,
			content,
			std::chrono::system_clock::time_point(std::chrono::milliseconds(timestampMs))
		});
	}

	return 0;
}

static duk_ret_t js_clearDanmaku(duk_context *ctx)
{
	if (g_jsContextData.danmakuMonitorWin)
	{
		g_jsContextData.danmakuMonitorWin->clearDanmaku();
	}
	return 0;
}

static duk_ret_t js_addDanmakuFromJson(duk_context *ctx)
{
	if (duk_get_top(ctx) < 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "addDanmakuFromJson requires 1 argument");
		return DUK_RET_TYPE_ERROR;
	}

	const char *jsonStr = duk_get_string(ctx, 0);
	if (!jsonStr)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid JSON string");
		return DUK_RET_TYPE_ERROR;
	}

	try
	{
		auto json = nlohmann::json::parse(jsonStr);
		if (json.is_array())
		{
			for (const auto &item : json)
			{
				std::string sender = item.value("sender", "");
				std::string content = item.value("content", "");
				int64_t timestampMs = item.value("timestamp", 
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch()
					).count()
				);

				if (g_jsContextData.danmakuMonitorWin)
				{
					g_jsContextData.danmakuMonitorWin->addDanmaku({
						sender,
						content,
						std::chrono::system_clock::time_point(std::chrono::milliseconds(timestampMs))
					});
				}
			}
		}
		else if (json.is_object())
		{
			std::string sender = json.value("sender", "");
			std::string content = json.value("content", "");
			int64_t timestampMs = json.value("timestamp", 
				std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()
				).count()
			);

			if (g_jsContextData.danmakuMonitorWin)
			{
				g_jsContextData.danmakuMonitorWin->addDanmaku({
					sender,
					content,
					std::chrono::system_clock::time_point(std::chrono::milliseconds(timestampMs))
				});
			}
		}
	}
	catch (const std::exception &e)
	{
		duk_error(ctx, DUK_ERR_ERROR, "Failed to parse JSON: %s", e.what());
		return DUK_RET_ERROR;
	}

	return 0;
}

static duk_ret_t js_addRandomDanmaku(duk_context *ctx)
{
	int count = 1;
	if (duk_get_top(ctx) >= 1)
	{
		count = duk_get_int(ctx, 0);
	}

	static const std::vector<std::string> sampleSenders = {
		"User1", "User2", "User3", "Guest", "Admin", "Moderator"
	};
	static const std::vector<std::string> sampleContents = {
		"Hello!", "Nice stream!", "Great content!", "LOL", "Amazing!",
		"Thanks for sharing!", "Keep it up!", "Awesome!", "Cool!", "Nice!"
	};

	if (g_jsContextData.danmakuMonitorWin)
	{
		for (int i = 0; i < count; ++i)
		{
			g_jsContextData.danmakuMonitorWin->addDanmaku({
				sampleSenders[rand() % sampleSenders.size()],
				sampleContents[rand() % sampleContents.size()],
				std::chrono::system_clock::now()
			});
		}
	}

	return 0;
}

static duk_ret_t js_log(duk_context *ctx)
{
	int nargs = duk_get_top(ctx);
	for (int i = 0; i < nargs; i++)
	{
		if (duk_is_string(ctx, i))
		{
			const char *str = duk_get_string(ctx, i);
			std::print("{} ", str);
		}
		else if (duk_is_number(ctx, i))
		{
			double num = duk_get_number(ctx, i);
			std::print("{} ", num);
		}
		else if (duk_is_boolean(ctx, i))
		{
			bool val = duk_get_boolean(ctx, i);
			std::print("{} ", val ? "true" : "false");
		}
		else if (duk_is_null_or_undefined(ctx, i))
		{
			std::print("null ");
		}
		else
		{
			std::print("[object] ");
		}
	}
	std::println("");
	return 0;
}

static duk_ret_t js_evalFile(duk_context *ctx)
{
	if (duk_get_top(ctx) < 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "evalFile requires 1 argument");
		return DUK_RET_TYPE_ERROR;
	}

	const char *filename = duk_get_string(ctx, 0);
	if (!filename)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid filename");
		return DUK_RET_TYPE_ERROR;
	}

	std::filesystem::path filePath(filename);

	if (!std::filesystem::exists(filePath))
	{
		duk_error(ctx, DUK_ERR_ERROR, "File not found: %s", filePath.string().c_str());
		return DUK_RET_ERROR;
	}

	std::ifstream file(filePath);
	if (!file.is_open())
	{
		duk_error(ctx, DUK_ERR_ERROR, "Failed to open file: %s", filePath.string().c_str());
		return DUK_RET_ERROR;
	}

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	if (duk_peval_string(ctx, content.c_str()) != DUK_EXEC_SUCCESS)
	{
		duk_error(ctx, DUK_ERR_ERROR, "JavaScript error: %s", duk_safe_to_string(ctx, -1));
		return DUK_RET_ERROR;
	}

	return 1;
}

void JSBindings::init(RmlUISystem &rmluiSys, DanmakuMonitorWin &danmakuMonitorWin)
{
	g_jsContextData.ctx = duk_create_heap_default();
	if (!g_jsContextData.ctx)
	{
		std::println("Failed to create Duktape heap");
		return;
	}

	g_jsContextData.danmakuMonitorWin = &danmakuMonitorWin;

	duk_push_global_object(g_jsContextData.ctx);

	duk_push_object(g_jsContextData.ctx);
	duk_push_c_function(g_jsContextData.ctx, js_addDanmaku, DUK_VARARGS);
	duk_put_prop_string(g_jsContextData.ctx, -2, "add");
	duk_push_c_function(g_jsContextData.ctx, js_clearDanmaku, 0);
	duk_put_prop_string(g_jsContextData.ctx, -2, "clear");
	duk_push_c_function(g_jsContextData.ctx, js_addDanmakuFromJson, 1);
	duk_put_prop_string(g_jsContextData.ctx, -2, "addFromJson");
	duk_push_c_function(g_jsContextData.ctx, js_addRandomDanmaku, 1);
	duk_put_prop_string(g_jsContextData.ctx, -2, "addRandom");
	duk_put_prop_string(g_jsContextData.ctx, -2, "danmaku");

	duk_push_c_function(g_jsContextData.ctx, js_log, DUK_VARARGS);
	duk_put_prop_string(g_jsContextData.ctx, -2, "log");

	duk_push_c_function(g_jsContextData.ctx, js_evalFile, 1);
	duk_put_prop_string(g_jsContextData.ctx, -2, "evalFile");

	duk_pop(g_jsContextData.ctx);

	std::println("JavaScript bindings initialized successfully");
}

void JSBindings::shutdown()
{
	if (g_jsContextData.ctx)
	{
		duk_destroy_heap(g_jsContextData.ctx);
		g_jsContextData.ctx = nullptr;
	}

	g_jsContextData.danmakuMonitorWin = nullptr;
}

void JSBindings::evalString(const std::string &code)
{
	if (!g_jsContextData.ctx)
	{
		std::println("JavaScript context not initialized");
		return;
	}

	if (duk_peval_string(g_jsContextData.ctx, code.c_str()) != DUK_EXEC_SUCCESS)
	{
		std::println("JavaScript error: {}", duk_safe_to_string(g_jsContextData.ctx, -1));
	}
	duk_pop(g_jsContextData.ctx);
}

void JSBindings::evalFile(const std::filesystem::path &filePath)
{
	if (!g_jsContextData.ctx)
	{
		std::println("JavaScript context not initialized");
		return;
	}

	if (!std::filesystem::exists(filePath))
	{
		std::println("File not found: {}", filePath.string());
		return;
	}

	std::ifstream file(filePath);
	if (!file.is_open())
	{
		std::println("Failed to open file: {}", filePath.string());
		return;
	}

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	if (duk_peval_string(g_jsContextData.ctx, content.c_str()) != DUK_EXEC_SUCCESS)
	{
		std::println("JavaScript error: {}", duk_safe_to_string(g_jsContextData.ctx, -1));
	}
	duk_pop(g_jsContextData.ctx);
}
