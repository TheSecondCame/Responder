// responder_request.h : Responder 代码输出和页面注册基类的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_REQUEST_H
#define RESPONDER_REQUEST_H

#include "httplib.h"

std::string generate_random_token(size_t length, std::string charset);

class responder_request
	// 所有请求处理程序的基类，包含一些通用的成员变量和函数
{
protected:
	std::string address;               // 请求地址（不包含查询字符串）
	std::map<std::string, std::string> params;
	                                   // 请求参数（从查询字符串中解析，或从表单数据中解析）
	std::string body;                  // 请求体数据（仅在处理post请求时使用）
	httplib::MultipartFormData req_form;
	                                   // multipart 表单数据（含上传文件），仅当请求为 multipart 时有内容
	bool is_request_login;             // 是否登录才能访问

	virtual void init() = 0;           // 需在子类自行实现初始化地址和其他参数的函数
	virtual std::string generate_response() = 0;

public:
	virtual void set_handler(httplib::Server &svr) = 0;
};

class responder_page : public responder_request
	// 可供浏览器访问的主页和菜单页的基类，包含一些通用的页面元素和生成响应的函数
{
protected:
	std::string browser_title;         // 浏览器标签页标题
	std::string page_title;            // 页面内标题（浅蓝色顶栏）
	std::string breadcrumb;            // 面包屑导航（主页 > ... > 当前页）

	std::string content;               // 页面内容（HTML代码），需在generate_page()中生成
	bool is_index_page = true;         // 是否包含浅蓝色顶栏
	bool is_full_custom = false;       // 是否完全自定义输出（不使用模板）

	std::string generate_response() override;
	virtual void generate_page() = 0;  // 需在子类自行实现生成页面内容的函数
public:
	void set_handler(httplib::Server& svr) override;
};

class responder_post_request : public responder_request
	// 表单提交、post请求处理程序的基类，包含一些通用的生成响应的函数
{
public:
	void set_handler(httplib::Server& svr) override;
	// virtual void generate_response() 没有实现，需在子类自行实现生成响应内容的函数
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