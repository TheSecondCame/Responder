// main.cpp : 程序入口。默认启动桌面管理窗口；带 --console 参数时按原命令行模式运行。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include <Windows.h>
#include <cstdio>
#include <iostream>
#include <string>
#include "server_core.h"
#include "logging.h"

// 定义在 gui.cpp
int RunGui();

// 把工作目录切到 exe 所在目录的上一级，使 assets\ 与 data\ 相对于 exe 定位。
static void SetupWorkingDirectory()
{
	char strModule[512] = { 0 };
	GetModuleFileNameA(NULL, strModule, sizeof(strModule));
	std::string dir(strModule);
	dir.append("\\..\\");
	SetCurrentDirectoryA(dir.c_str());
}

// 声明为「系统 DPI 感知」并在窗口内自行按比例缩放。
// 不用 per-monitor：那种模式下 GetSystemMetrics 返回逻辑值，需要额外做
// 物理/逻辑坐标换算；这里统一用系统 DPI 比例缩放布局，简单且行为可预期。
static void EnableDpiAwareness()
{
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	if (!user32)
		return;
	typedef BOOL(WINAPI* SetCtxFn)(void*);
	auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
	if (setCtx)
		setCtx((void*)-2); // DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type)
{
	switch (ctrl_type)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		if (!server_core::is_running())
			return FALSE;
		server_core::stop();
		return TRUE;
	case CTRL_CLOSE_EVENT:
		server_core::stop();
		return FALSE;
	default:
		return FALSE;
	}
}

// Windows 子系统进程在 --console 模式下优先附着父控制台，失败时再新建控制台并绑定未重定向的标准流。
static void SetupConsole()
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD outType = (hOut && hOut != INVALID_HANDLE_VALUE) ? GetFileType(hOut) : FILE_TYPE_UNKNOWN;
	bool outRedirected = (outType == FILE_TYPE_DISK || outType == FILE_TYPE_PIPE);

	HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
	DWORD errType = (hErr && hErr != INVALID_HANDLE_VALUE) ? GetFileType(hErr) : FILE_TYPE_UNKNOWN;
	bool errRedirected = (errType == FILE_TYPE_DISK || errType == FILE_TYPE_PIPE);

	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD inType = (hIn && hIn != INVALID_HANDLE_VALUE) ? GetFileType(hIn) : FILE_TYPE_UNKNOWN;
	bool inRedirected = (inType == FILE_TYPE_DISK || inType == FILE_TYPE_PIPE);

	if ((!outRedirected || !errRedirected || !inRedirected) && !GetConsoleWindow())
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS))
			AllocConsole();
	}

	if (outRedirected)
		SetStdHandle(STD_OUTPUT_HANDLE, hOut);
	if (errRedirected)
		SetStdHandle(STD_ERROR_HANDLE, hErr);
	if (inRedirected)
		SetStdHandle(STD_INPUT_HANDLE, hIn);

	FILE* fp = nullptr;
	if (!outRedirected)
		freopen_s(&fp, "CONOUT$", "w", stdout);
	if (!errRedirected)
		freopen_s(&fp, "CONOUT$", "w", stderr);
	if (!inRedirected)
		freopen_s(&fp, "CONIN$", "r", stdin);

	if (GetConsoleWindow())
	{
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
		SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
	}
}

int main(int argc, char** argv)
{
	SetupWorkingDirectory();
	EnableDpiAwareness();

	bool console_mode = false;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--console" || a == "-c")
			console_mode = true;
	}

	if (console_mode)
		SetupConsole();

	// 接管 std::cout / std::cerr：写入 server.log，并供窗口或控制台显示。
	logging::setup(console_mode ? 0 : 1);

	server_core::initialize();

	if (console_mode)
	{
		// 原命令行模式：前台运行，Ctrl+C 退出
		server_core::start();
		return 0;
	}

	// 默认：桌面管理窗口
	return RunGui();
}
