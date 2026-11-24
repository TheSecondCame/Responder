// responder_request.h : Responder 代码输出和页面注册基类的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_REQUEST_H
#define RESPONDER_REQUEST_H

#include "httplib.h"
#include <map>
#include <string>

std::string generate_random_token(size_t length, std::string charset);

// 单次 HTTP 请求的局部上下文（仅在当次请求栈上构造，避免多线程处理器共享竞态）
struct request_context
{
	std::map<std::string, std::string> params;
	std::string body;
	httplib::MultipartFormData req_form;
	httplib::Headers headers;

	bool has_param(const std::string& key) const
	{
		return params.find(key) != params.end();
	}

	std::string get_param(const std::string& key, const std::string& default_value = "") const
	{
		auto it = params.find(key);
		return (it != params.end()) ? it->second : default_value;
	}
};

// 单次页面渲染的局部输出上下文（由处理器默认元数据初始化，避免多线程覆盖 title/content）
struct page_context
{
	std::string browser_title;
	std::string page_title;
	std::string breadcrumb;
	std::string content;
	bool is_index_page = true;
	bool is_full_custom = false;
	int status_code = 200;
};

request_context build_request_context(const httplib::Request& req, bool parse_urlencoded_body);

class responder_request
	// 所有请求处理程序的基类。除 init() 初始化的不可变路由配置外，不持有单次请求的可变状态
{
protected:
	std::string address;               // 请求地址（不包含查询字符串）
	bool is_request_login = false;     // 是否登录才能访问

	virtual void init() = 0;           // 需在子类自行实现初始化地址和其他配置的函数
	virtual std::string generate_response(const request_context& ctx) = 0;

public:
	virtual ~responder_request() = default;
	virtual void set_handler(httplib::Server& svr) = 0;
};

class responder_page : public responder_request
	// 可供浏览器访问的主页和菜单页的基类，包含一些通用的页面元素和生成响应的函数
{
protected:
	std::string browser_title;         // 默认浏览器标签页标题（init 中设置）
	std::string page_title;            // 默认页面内标题（init 中设置）
	std::string breadcrumb;            // 默认面包屑导航（init 中设置）

	bool is_index_page = true;         // 是否包含浅蓝色顶栏
	bool is_full_custom = false;       // 是否完全自定义输出（不使用模板）

	std::string render_page_with_status(const request_context& ctx, int& out_status);
	std::string generate_response(const request_context& ctx) override;
	virtual void generate_page(const request_context& ctx, page_context& page) = 0;
public:
	void set_handler(httplib::Server& svr) override;
};

class responder_post_request : public responder_request
	// 表单提交、post请求处理程序的基类，包含一些通用的生成响应的函数
{
public:
	void set_handler(httplib::Server& svr) override;
	// virtual void generate_response(const request_context& ctx) 需在子类实现
};

class responder_post_page : public responder_page
	// 既作为 POST 表单处理器、又复用页面模板输出结果的基类。
	// 适用于「提交表单 -> 渲染结果页」这类处理器（如题库管理）。
	// 注意：responder_page::set_handler 只注册 GET，本类改为注册 POST。
{
public:
	void set_handler(httplib::Server& svr) override;
};

#endif // RESPONDER_REQUEST_H