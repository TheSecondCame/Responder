// logging.cpp : 全局日志的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "logging.h"
#include <Windows.h>
#include <cstdio>
#include <iostream>
#include <fstream>

namespace
{
	std::mutex g_mutex;
	std::vector<std::string> g_lines;
	size_t g_read_pos = 0;
	std::ofstream g_file;
	bool g_console_echo = false;

	// 接管 std::cout / std::cerr 的静态缓冲实例
	LogStreamBuf g_cout_buf(false);
	LogStreamBuf g_cerr_buf(true);
}

namespace logging
{

void append(const std::string& line, bool to_stderr)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_lines.push_back(line);
	if (g_file.is_open())
		g_file << line << std::endl;
	if (g_console_echo)
	{
		FILE* out = to_stderr ? stderr : stdout;
		std::fwrite(line.data(), 1, line.size(), out);
		std::fputc('\n', out);
		std::fflush(out);
	}
}

std::vector<std::string> drain()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	std::vector<std::string> out;
	for (; g_read_pos < g_lines.size(); ++g_read_pos)
		out.push_back(g_lines[g_read_pos]);
	return out;
}

std::vector<std::string> snapshot()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_lines;
}

void clear()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_lines.clear();
	g_read_pos = 0;
}

void setup(int hide_console)
{
	g_console_echo = (hide_console == 0);

	if (hide_console)
	{
		HWND console = GetConsoleWindow();
		if (console)
			ShowWindow(console, SW_HIDE);
	}

	if (!g_file.is_open())
		g_file.open("server.log", std::ios::out | std::ios::app);

	std::cout.rdbuf(&g_cout_buf);
	std::cerr.rdbuf(&g_cerr_buf);
	std::cerr.unsetf(std::ios::unitbuf);
}

} // namespace logging
