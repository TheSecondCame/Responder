// Server.cpp : 服务端核心（HTTP 服务、路由注册、题库加载）。
// 本文件不再包含 main()，入口见 main.cpp。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include <iostream>
#include <atomic>
#include <mutex>
#include "httplib.h"
#include "responder_request.h"
#include "responder_problem.h"
#include "responder_pages.h"
#include "responder_template.h"
#include "functions.h"
#include "server_core.h"

problem_list pl;
problem_list pl_unused;

std::string authentic_token;
std::string authentic_pin;

namespace
{
	// 全局唯一的 HTTP 服务对象；GUI 需要持有它以便随时 stop()
	httplib::Server g_server;
	bool g_initialized = false;

	std::mutex g_state_mutex;
	std::atomic<bool> g_listening{ false };
	std::string g_last_error;

	// 页面与请求处理器对象。
	// 重要：httplib 保存的 lambda 以 this 捕获这些对象，它们的生命周期必须覆盖
	// 整个服务运行期。因此在这里以文件级静态对象持有，绝不能放进局部作用域，
	// 否则注册函数返回后对象析构，收到请求时会访问已释放内存（表现为连接被重置）。
	welcome_page wp;
	main_page mp;

	start_page sp;
	login_handler lh;
	logout_page lop;

	new_competition_page ncp;
	new_competition_handler nch;

	competition_detail_page cdp;

	competition_edit_page cep;
	competition_edit_handler ceh;
	competition_delete_handler cdh;

	competition_display_page cdp2;
	competition_display_scores_handler cdsh;
	competition_display_problem_handler cdph;
	competition_display_session_handler cdssh;
	competition_enddisplay_handler cdedh;

	// 题库管理（原 Problems Generator 的功能，现已并入服务端）
	problem_manage_page pmp;
	problem_manage_handler pmh;

	// 注册全部路由
	void register_handlers(httplib::Server& svr)
	{
		wp.set_handler(svr);
		mp.set_handler(svr);

		sp.set_handler(svr);
		lh.set_handler(svr);
		lop.set_handler(svr);

		ncp.set_handler(svr);
		nch.set_handler(svr);

		cdp.set_handler(svr);

		cep.set_handler(svr);
		ceh.set_handler(svr);
		cdh.set_handler(svr);

		cdp2.set_handler(svr);
		cdsh.set_handler(svr);
		cdph.set_handler(svr);
		cdssh.set_handler(svr);
		cdedh.set_handler(svr);

		pmp.set_handler(svr);
		pmh.set_handler(svr);
	}
}

std::string get_first_ip()
{
	std::string list = get_ip_list();
	size_t pos = list.find('\n');
	std::string first = (pos == std::string::npos) ? list : list.substr(0, pos);
	while (!first.empty() && (first.back() == '\r' || first.back() == ' '))
		first.pop_back();
	return first;
}

namespace server_core
{

void initialize()
{
	if (g_initialized)
		return;
	g_initialized = true;

	// 首次运行时自动创建数据目录，避免题库路径不存在
	CreateDirectoryA("data", NULL);
	CreateDirectoryA("data/problems", NULL);
	CreateDirectoryA("data/competitions", NULL);

	// 加载题库
	pl.load("data/problems/problem_list.json");
	pl_unused = pl;
	pl_unused.remove_used();

	httplib::Server& svr = g_server;
	svr.set_mount_point("/assets", "assets");

	// 独占绑定端口。httplib 默认会设置 SO_REUSEADDR，在 Windows 上这允许
	// 两个进程同时绑定同一端口、请求被随机分流，容易出现"开了两份实例却不知道
	// 请求进了哪个"的问题。这里用空实现覆盖默认选项，使第二个实例绑定失败并明确报错。
	svr.set_socket_options([](SOCKET) {});

	register_handlers(svr);

	authentic_token = generate_random_token(50, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	authentic_pin = generate_random_token(6, "0123456789");

	std::cout << "=================================================" << std::endl;
	std::cout << "Responder by STA " << VERSION << std::endl;
	std::cout << get_compile_info() << std::endl;
	std::cout << "=================================================" << std::endl;
	std::cout << "PIN code for login: " << authentic_pin << std::endl;
	std::cout << "=================================================" << std::endl;
}

void start()
{
	{
		std::lock_guard<std::mutex> lock(g_state_mutex);
		g_last_error.clear();
	}
	g_listening = false;

	std::cout << "Starting server on port " << PORT << " ..." << std::endl;

	// listen 在绑定失败（如端口已被占用）时返回 false。
	bool ok = g_server.listen("0.0.0.0", PORT);

	if (ok)
	{
		std::cout << "Server stopped." << std::endl;
	}
	else
	{
		std::string msg = "Failed to start server on port " + std::to_string(PORT) +
			". The port may already be in use by another program.";
		{
			std::lock_guard<std::mutex> lock(g_state_mutex);
			g_last_error = msg;
		}
		std::cout << "ERROR: " << msg << std::endl;
	}

	g_listening = false;
}

void stop()
{
	if (g_server.is_running())
		g_server.stop();
}

bool is_running()
{
	return g_server.is_running();
}

std::string last_error()
{
	std::lock_guard<std::mutex> lock(g_state_mutex);
	return g_last_error;
}

} // namespace server_core
