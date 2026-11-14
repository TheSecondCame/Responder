// server_core.h : 服务端控制接口（供 GUI 与命令行入口共用）。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RES_SERVER_CORE_H
#define RES_SERVER_CORE_H

#include <string>

namespace server_core
{
	// 服务端口
	const int PORT = 8087;

	// 初始化：切换工作目录、创建数据目录、加载题库。
	// 必须在 start() 之前调用一次。
	void initialize();

	// 在后台线程中运行 HTTP 服务。函数阻塞直到 stop() 被调用。
	// 两个线程不应同时调用 start()。
	void start();

	// 请求停止服务（线程安全，可从任意线程调用）。
	void stop();

	// 服务是否正在监听（线程安全）。
	bool is_running();

	// 最近一次 start() 期间的错误信息；为空表示无错误。
	std::string last_error();
}

// 当前登录用的 6 位 PIN（登录成功后会自动轮换，GUI 需要实时读取）
extern std::string authentic_pin;

// 当前会话 token
extern std::string authentic_token;

// 取本机首个可用 IPv4 地址（局域网访问地址），失败返回空串
std::string get_first_ip();

#endif // RES_SERVER_CORE_H
