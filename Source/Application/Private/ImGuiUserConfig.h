#pragma once
// Uniform configuration for the four core objects and all private adapters.
#define IMGUI_USE_WCHAR32
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_FONT
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#ifdef IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS
#error Platform clipboard services must remain owned by Application
#endif
