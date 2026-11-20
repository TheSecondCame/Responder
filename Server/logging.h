// logging.h : 全局日志（供 GUI 显示与文件记录）。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RES_LOGGING_H
#define RES_LOGGING_H

#include <string>
#include <vector>
#include <mutex>
#include <streambuf>

namespace logging
{
	// 追加一行日志（线程安全）。会自动写入 server.log 并保留给 GUI 读取；
	// 在控制台模式下同步回写 stdout/stderr。
	void append(const std::string& line, bool to_stderr = false);

	// 取走自上次调用以来新增的日志行（线程安全）。
	std::vector<std::string> drain();

	// 全部日志行的快照（线程安全）。
	std::vector<std::string> snapshot();

	// 清空日志缓存（不删除日志文件）。
	void clear();

	// 把标准输出/标准错误重定向到日志；
	// hide_console 非 0 时隐藏控制台窗口（Windows 子系统下自然无操作），
	// 为 0 时（--console 模式）将日志同步回写控制台。
	void setup(int hide_console);
}

// 一个把写入内容按行转发给 logging::append 的流缓冲。
// 用于接管 std::cout / std::cerr 的 rdbuf。
class LogStreamBuf : public std::streambuf
{
public:
	explicit LogStreamBuf(bool also_stderr) : to_stderr_(also_stderr) {}

protected:
	int overflow(int ch) override
	{
		if (ch == traits_type::eof())
			return traits_type::not_eof(ch);
		std::lock_guard<std::mutex> lock(mutex_);
		push_char((char)ch);
		return ch;
	}

	std::streamsize xsputn(const char* s, std::streamsize n) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (std::streamsize i = 0; i < n; ++i)
			push_char(s[i]);
		return n;
	}

	int sync() override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!buffer_.empty())
		{
			logging::append(buffer_, to_stderr_);
			buffer_.clear();
		}
		return 0;
	}

private:
	void push_char(char c)
	{
		if (c == '\n')
		{
			logging::append(buffer_, to_stderr_);
			buffer_.clear();
		}
		else if (c != '\r')
		{
			buffer_ += c;
		}
	}

	std::mutex mutex_;
	std::string buffer_;
	bool to_stderr_;
};

#endif // RES_LOGGING_H
