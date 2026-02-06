/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "RmlUi_Backend.h"
#include "RmlUi_Platform_GLFW.h"
#include "RmlUi_Renderer_GL3.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Profiling.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <ranges>

static void SetupCallbacks(GLFWwindow* window);

static void LogErrorFromGLFW(int error, const char* description)
{
	Rml::Log::Message(Rml::Log::LT_ERROR, "GLFW error (0x%x): %s", error, description);
}

/**
    Global data used by this backend.
 
    Lifetime governed by the calls to Backend::Initialize() and Backend::Shutdown().
 */

// 每個視窗專屬的渲染器與狀態
struct WindowData {
	GLFWwindow* glfw_win = nullptr;
	RenderInterface_GL3 renderer;
	Rml::Context* context = nullptr;
	KeyDownCallback key_down_callback = nullptr;
	int glfw_active_modifiers = 0;
	bool context_dimensions_dirty = true;
	bool shouldClose = false;
};

// 一個「多窗口」RenderInterface，根據當前活動視窗轉發所有調用
class MultiWindowRenderInterface_GL3 : public Rml::RenderInterface {
public:
	void SetActive(RenderInterface_GL3* renderer) { active_renderer = renderer; }

	// -- Rml::RenderInterface overrides，全部轉發到當前活動渲染器 --

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->CompileGeometry(vertices, indices);
	}

	void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->RenderGeometry(handle, translation, texture);
	}

	void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override
	{
		if (!active_renderer) return;
		active_renderer->ReleaseGeometry(handle);
	}

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->LoadTexture(texture_dimensions, source);
	}

	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data, Rml::Vector2i source_dimensions) override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->GenerateTexture(source_data, source_dimensions);
	}

	void ReleaseTexture(Rml::TextureHandle texture_handle) override
	{
		if (!active_renderer) return;
		active_renderer->ReleaseTexture(texture_handle);
	}

	void EnableScissorRegion(bool enable) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->EnableScissorRegion(enable);
	}

	void SetScissorRegion(Rml::Rectanglei region) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->SetScissorRegion(region);
	}

	void EnableClipMask(bool enable) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->EnableClipMask(enable);
	}

	void RenderToClipMask(Rml::ClipMaskOperation mask_operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->RenderToClipMask(mask_operation, geometry, translation);
	}

	void SetTransform(const Rml::Matrix4f* transform) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->SetTransform(transform);
	}

	Rml::LayerHandle PushLayer() override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->PushLayer();
	}

	void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode,
		Rml::Span<const Rml::CompiledFilterHandle> filters) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->CompositeLayers(source, destination, blend_mode, filters);
	}

	void PopLayer() override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->PopLayer();
	}

	Rml::TextureHandle SaveLayerAsTexture() override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->SaveLayerAsTexture();
	}

	Rml::CompiledFilterHandle SaveLayerAsMaskImage() override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->SaveLayerAsMaskImage();
	}

	Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->CompileFilter(name, parameters);
	}

	void ReleaseFilter(Rml::CompiledFilterHandle filter) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->ReleaseFilter(filter);
	}

	Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) override
	{
		RMLUI_ASSERT(active_renderer);
		return active_renderer->CompileShader(name, parameters);
	}

	void RenderShader(Rml::CompiledShaderHandle shader_handle, Rml::CompiledGeometryHandle geometry_handle, Rml::Vector2f translation,
		Rml::TextureHandle texture) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->RenderShader(shader_handle, geometry_handle, translation, texture);
	}

	void ReleaseShader(Rml::CompiledShaderHandle effect_handle) override
	{
		RMLUI_ASSERT(active_renderer);
		active_renderer->ReleaseShader(effect_handle);
	}

private:
	RenderInterface_GL3* active_renderer = nullptr;
};

struct BackendData {
	SystemInterface_GLFW system_interface;
	MultiWindowRenderInterface_GL3 render_interface;
	Rml::Vector<std::unique_ptr<WindowData>> windows;
};

static Rml::UniquePtr<BackendData> data;

// 依據 GLFWwindow 指標取得對應的 WindowData
static WindowData* GetWindowData(GLFWwindow* window)
{
	RMLUI_ASSERT(data);
	auto it = std::ranges::find_if(data->windows,
		[window](const std::unique_ptr<WindowData>& w) { return w->glfw_win == window; }
	);
	return it != data->windows.end() ? &**it : nullptr;
}

static GLFWwindow* CreateWindowInternal(const char* name, int width, int height, bool allow_resize, GLFWwindow* shared)
{
	// Set window hints for OpenGL 3.3 Core context creation.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

	// Apply window properties and create it.
	glfwWindowHint(GLFW_RESIZABLE, allow_resize ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(width, height, name, nullptr, shared);
	if (!window)
		return nullptr;

	return window;
}

bool Backend::Initialize(const char* name, int width, int height, bool allow_resize)
{
	RMLUI_ASSERT(!data);

	glfwSetErrorCallback(LogErrorFromGLFW);

	if (!glfwInit())
		return false;

	GLFWwindow* window = CreateWindowInternal(name, width, height, allow_resize, nullptr);
	if (!window)
		return false;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	// Load the OpenGL functions.
	Rml::String renderer_message;
	if (!RmlGL3::Initialize(&renderer_message))
		return false;

	// Construct global backend data.
	data = Rml::MakeUnique<BackendData>();
	if (!data)
		return false;

	// 對主視窗建立專屬的 RenderInterface_GL3（需在該視窗的 GL context 下構建）
	auto wdata = std::make_unique<WindowData>();
	wdata->glfw_win = window;
	if (!wdata->renderer)
		return false;
	data->windows.push_back(std::move(wdata));

	data->system_interface.SetWindow(window);
	data->system_interface.LogMessage(Rml::Log::LT_INFO, renderer_message);

	// The window size may have been scaled by DPI settings, get the actual pixel size.
	glfwGetFramebufferSize(window, &width, &height);
	// 使用主視窗對應的渲染器設定 viewport
	if (!data->windows.empty() && data->windows.front())
		data->windows.front()->renderer.SetViewport(width, height);

	// Receive num lock and caps lock modifiers for proper handling of numpad inputs in text fields.
	glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

	// Setup the input and window event callback functions.
	SetupCallbacks(window);

	return true;
}

void Backend::OnRmlShutdown()
{
	RMLUI_ASSERT(data);
	for (auto& w : data->windows) {
		glfwSetWindowContentScaleCallback(w->glfw_win, nullptr);
		glfwSetFramebufferSizeCallback(w->glfw_win, nullptr);
		glfwSetScrollCallback(w->glfw_win, nullptr);
		glfwSetMouseButtonCallback(w->glfw_win, nullptr);
		glfwSetCursorPosCallback(w->glfw_win, nullptr);
		glfwSetCursorEnterCallback(w->glfw_win, nullptr);
		glfwSetCharCallback(w->glfw_win, nullptr);
		glfwSetKeyCallback(w->glfw_win, nullptr);
	}
}

void Backend::Shutdown()
{
	RMLUI_ASSERT(data);
	for (auto& w : data->windows)
		glfwDestroyWindow(w->glfw_win);
	data.reset();
	RmlGL3::Shutdown();
	glfwTerminate();
}

Rml::SystemInterface* Backend::GetSystemInterface()
{
	RMLUI_ASSERT(data);
	return &data->system_interface;
}

Rml::RenderInterface* Backend::GetRenderInterface()
{
	RMLUI_ASSERT(data);
	return &data->render_interface;
}

GLFWwindow* Backend::GetMainWindow()
{
	RMLUI_ASSERT(data);
	// 主視窗必定是第一個加入的視窗
	return data->windows.front()->glfw_win;
}

bool Backend::ShouldWindowClose(GLFWwindow* glfw_win)
{
	RMLUI_ASSERT(data);
	auto winIt = std::ranges::find_if(
		data->windows,
		[glfw_win](const std::unique_ptr<WindowData>& w) { return w->glfw_win == glfw_win; }
	);
	RMLUI_ASSERTMSG(winIt != data->windows.end(), "Window not managed by backend.");
	return winIt->get()->shouldClose;
}

void Backend::DestroyWindow(GLFWwindow* glfwWin)
{
	RMLUI_ASSERT(data);
	auto winIt = std::ranges::find_if(
		data->windows,
		[glfwWin](const std::unique_ptr<WindowData>& w) { return w->glfw_win == glfwWin; }
	);
	RMLUI_ASSERTMSG(winIt != data->windows.end(), "Window not managed by backend.");
	
	// To let the renderer destory its resources properly.
	glfwMakeContextCurrent(glfwWin);
	data->render_interface.SetActive(nullptr);
	data->windows.erase(winIt);
	glfwDestroyWindow(glfwWin);
	
}

GLFWwindow* Backend::CreateWindow(const char* name, int width, int height, bool allow_resize)
{
	RMLUI_ASSERT(data);
	GLFWwindow* shared = data->windows.empty() ? nullptr : data->windows.front()->glfw_win;

	GLFWwindow* window = CreateWindowInternal(name, width, height, allow_resize, shared);
	if (!window)
		return nullptr;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	// 在新視窗的 GL context 下構建該視窗專屬渲染器
	auto wdata = std::make_unique<WindowData>();
	wdata->glfw_win = window;
	if (!wdata->renderer)
		return nullptr;
	// 初始化 viewport，避免後續 BeginFrame 時為 0
	glfwGetFramebufferSize(window, &width, &height);
	wdata->renderer.SetViewport(width, height);
	data->windows.push_back(std::move(wdata));

	// Receive num lock and caps lock modifiers for proper handling of numpad inputs in text fields.
	glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
	SetupCallbacks(window);

	return window;
}

void Backend::AttachContext(GLFWwindow* window, Rml::Context* context, KeyDownCallback key_down_callback)
{
	RMLUI_ASSERT(data && window);
	for (auto& w : data->windows)
	{
		if (w->glfw_win == window)
		{
			w->context = context;
			w->key_down_callback = key_down_callback;
			return;
		}
	}
	RMLUI_ASSERTMSG(false, "Window not managed by backend.");
}

Rml::Vector2i Backend::GetWindowSize(GLFWwindow* window)
{
	RMLUI_ASSERT(data && window);
	auto winIt = std::ranges::find_if(
		data->windows,
		[window](const std::unique_ptr<WindowData>& w) { return w->glfw_win == window; }
	);
	RMLUI_ASSERTMSG(winIt != data->windows.end(), "Window not managed by backend.");
	Rml::Vector2i size;
	glfwGetFramebufferSize(window, &size.x, &size.y);
	return size;
}

Rml::Vector2i Backend::GetWindowPos(GLFWwindow *window) {
	RMLUI_ASSERT(data && window);
	auto winIt = std::ranges::find_if(
		data->windows,
		[window](const std::unique_ptr<WindowData> &w) { return w->glfw_win == window; }
	);
	RMLUI_ASSERTMSG(winIt != data->windows.end(), "Window not managed by backend.");
	Rml::Vector2i pos;
	glfwGetWindowPos(window, &pos.x, &pos.y);
	return pos;
}

void Backend::SetWindowPos(GLFWwindow *window, Rml::Vector2i pos) {
	RMLUI_ASSERT(data && window);
	auto winIt = std::ranges::find_if(
		data->windows,
		[window](const std::unique_ptr<WindowData> &w) { return w->glfw_win == window; }
	);
	RMLUI_ASSERTMSG(winIt != data->windows.end(), "Window not managed by backend.");
	glfwSetWindowPos(window, pos.x, pos.y);
}

bool Backend::ProcessEvents(bool power_save)
{
	RMLUI_ASSERT(data);
	const double wait_timeout = 10.0;

	// 1. Update DPI and context dimensions for each window
	for (auto& win : data->windows)
	{
		if (win->shouldClose)
			continue;

		if (win->context_dimensions_dirty)
		{
			Rml::Vector2i window_size;
			float dp_ratio = 1.f;
			glfwGetFramebufferSize(win->glfw_win, &window_size.x, &window_size.y);
			glfwGetWindowContentScale(win->glfw_win, &dp_ratio, nullptr);

			win->context->SetDimensions(window_size);
			win->context->SetDensityIndependentPixelRatio(dp_ratio);

			win->context_dimensions_dirty = false;
		}
	}

	// 2. Wait for events
	auto min_next_update_delay_among_all_contexts = [&]() {
		auto alive_wins = data->windows | std::views::filter([](const auto &w) { return !w->shouldClose; });
		auto min_next_update_delay_it = std::ranges::min_element(
			alive_wins,
			[](const auto &w1, const auto &w2) {
				return
					(w1->context->GetNextUpdateDelay() < w2->context->GetNextUpdateDelay());
			}
		);
		return
			min_next_update_delay_it != alive_wins.end() ? 
			min_next_update_delay_it->get()->context->GetNextUpdateDelay() : 
			wait_timeout;
	}();

	
	if (power_save)
		glfwWaitEventsTimeout(min_next_update_delay_among_all_contexts);
	else
		glfwPollEvents();

	// 3. Handling window close
	for (auto& win : data->windows)
	{
		if (glfwWindowShouldClose(win->glfw_win))
			win->shouldClose = true;
	}

	
	// 4. Handling application exit
	// false = applicaction should exit
	// No main window, the application should exit
	if (data->windows.empty())
		return false;

	auto &mainWin = *data->windows.front();

	// If main window is closed, the application should exit
	bool shouldExit = mainWin.shouldClose;

	// If the application should exit, we postpone the destruction of the last window,
	// in order to let rmgui to do something (this is required)
	glfwSetWindowShouldClose(mainWin.glfw_win, GLFW_FALSE);
	return !shouldExit;
}



void Backend::RequestExit()
{
	RMLUI_ASSERT(data);
	for (auto& w : data->windows)
		glfwSetWindowShouldClose(w->glfw_win, GLFW_TRUE);
}

void Backend::BeginFrame(GLFWwindow* window)
{
	RMLUI_ASSERT(data);
	glfwMakeContextCurrent(window);
	data->system_interface.SetWindow(window);

	WindowData* w = ::GetWindowData(window);
	RMLUI_ASSERT(w);

	// 若 viewport 尚未初始化（可能為 0），在此補齊
	if (w->renderer.GetViewportWidth() == 0 || w->renderer.GetViewportHeight() == 0)
	{
		int fbw = 0, fbh = 0;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		w->renderer.SetViewport(fbw, fbh);
	}

	// 將當前活動渲染器設為此視窗的 renderer
	data->render_interface.SetActive(&w->renderer);

	w->renderer.Clear();
	w->renderer.BeginFrame();
}

void Backend::PresentFrame(GLFWwindow* window)
{
	RMLUI_ASSERT(data);
	glfwMakeContextCurrent(window);
	WindowData* w = ::GetWindowData(window);
	RMLUI_ASSERT(w);

	w->renderer.EndFrame();
	glfwSwapBuffers(window);

	// Optional, used to mark frames during performance profiling.
	RMLUI_FrameMark;
}

static void SetupCallbacks(GLFWwindow* window)
{
	RMLUI_ASSERT(data);
	glfwSetWindowUserPointer(window, ::GetWindowData(window));

	// Key input
	glfwSetKeyCallback(window, [](GLFWwindow* win, int glfw_key, int /*scancode*/, int glfw_action, int glfw_mods) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(win));
		if (!w || !w->context)
			return;

		// Store the active modifiers for later because GLFW doesn't provide them in the callbacks to the mouse input events.
		w->glfw_active_modifiers = glfw_mods;

		// Override the default key event callback to add global shortcuts for the samples.
		Rml::Context* context = w->context;
		KeyDownCallback key_down_callback = w->key_down_callback;

		switch (glfw_action)
		{
		case GLFW_PRESS:
		case GLFW_REPEAT:
		{
			const Rml::Input::KeyIdentifier key = RmlGLFW::ConvertKey(glfw_key);
			const int key_modifier = RmlGLFW::ConvertKeyModifiers(glfw_mods);
			float dp_ratio = 1.f;
			glfwGetWindowContentScale(win, &dp_ratio, nullptr);

			// See if we have any global shortcuts that take priority over the context.
			if (key_down_callback && !key_down_callback(context, key, key_modifier, dp_ratio, true))
				break;
			// Otherwise, hand the event over to the context by calling the input handler as normal.
			if (!RmlGLFW::ProcessKeyCallback(context, glfw_key, glfw_action, glfw_mods))
				break;
			// The key was not consumed by the context either, try keyboard shortcuts of lower priority.
			if (key_down_callback && !key_down_callback(context, key, key_modifier, dp_ratio, false))
				break;
		}
		break;
		case GLFW_RELEASE: RmlGLFW::ProcessKeyCallback(context, glfw_key, glfw_action, glfw_mods); break;
		}
	});

	glfwSetCharCallback(window, [](GLFWwindow* window, unsigned int codepoint) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (w)
			RmlGLFW::ProcessCharCallback(w->context, codepoint);
	});

	glfwSetCursorEnterCallback(window, [](GLFWwindow* window, int entered) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (w)
			RmlGLFW::ProcessCursorEnterCallback(w->context, entered);
	});

	// Mouse input
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (w)
			RmlGLFW::ProcessCursorPosCallback(w->context, window, xpos, ypos, w->glfw_active_modifiers);
	});

	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (!w)
			return;
		w->glfw_active_modifiers = mods;
		RmlGLFW::ProcessMouseButtonCallback(w->context, button, action, mods);
	});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double /*xoffset*/, double yoffset) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (w)
			RmlGLFW::ProcessScrollCallback(w->context, yoffset, w->glfw_active_modifiers);
	});

	// Window events
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		if (!w)
			return;
		w->renderer.SetViewport(width, height);
		RmlGLFW::ProcessFramebufferSizeCallback(w->context, width, height);
	});

	glfwSetWindowContentScaleCallback(window,
		[](GLFWwindow* window, float xscale, float /*yscale*/) {
			auto* w = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
			if (w)
				RmlGLFW::ProcessContentScaleCallback(w->context, xscale);
		});
}
