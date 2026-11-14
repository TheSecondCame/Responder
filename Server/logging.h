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
	// 追加一行日志（线程安全）。会自动写入 server.log 并保留给 GUI 读取。
	void append(const std::string& line);

	// 取走自上次调用以来新增的日志行（线程安全）。
	std::vector<std::string> drain();

	// 全部日志行的快照（线程安全）。
	std::vector<std::string> snapshot();

	// 清空日志缓存（不删除日志文件）。
	void clear();

	// 把标准输出/标准错误重定向到日志，并把控制台窗口隐藏。
	// 这样原有的 std::cout 输出会同时进入 GUI 日志与 server.log，
	// 无需改动已有的 66 处输出语句。
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
		char c = (char)ch;
		if (c == '\n')
		{
			logging::append(buffer_);
			buffer_.clear();
		}
		else if (c != '\r')
		{
			buffer_ += c;
		}
		return ch;
	}

	std::streamsize xsputn(const char* s, std::streamsize n) override
	{
		for (std::streamsize i = 0; i < n; ++i)
			overflow((unsigned char)s[i]);
		return n;
	}

	int sync() override
	{
		if (!buffer_.empty())
		{
			logging::append(buffer_);
			buffer_.clear();
		}
		return 0;
	}

private:
	std::string buffer_;
	bool to_stderr_;
};

#endif // RES_LOGGING_H
