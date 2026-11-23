// responder_request.cpp : Responder 代码输出和页面注册基类的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_request.h"
#include "responder_template.h"
#include "encoding.h"
#include <algorithm>
#include <cctype>

extern std::string authentic_token;

namespace
{
	bool contains_case_insensitive(const std::string& haystack, const std::string& needle)
	{
		auto it = std::search(
			haystack.begin(), haystack.end(),
			needle.begin(), needle.end(),
			[](unsigned char a, unsigned char b) {
				return std::tolower(a) == std::tolower(b);
			});
		return it != haystack.end();
	}

	// 解析 application/x-www-form-urlencoded 请求体，填充到 params。
	// 原实现只从查询字符串取参数，导致浏览器提交的表单（POST body）取不到值，
	// 这里补齐表单体解析，使 params 同时覆盖查询串与表单体两种来源。
	void parse_form_urlencoded(const httplib::Request& req, std::map<std::string, std::string>& params)
	{
		auto content_type = req.get_header_value("Content-Type");
		if (!contains_case_insensitive(content_type, "application/x-www-form-urlencoded"))
			return;

		const std::string& data = req.body;
		std::string::size_type pos = 0;
		while (pos < data.size())
		{
			std::string::size_type amp = data.find('&', pos);
			if (amp == std::string::npos)
				amp = data.size();

			std::string pair = data.substr(pos, amp - pos);
			if (!pair.empty())
			{
				std::string::size_type eq = pair.find('=');
				std::string key, value;
				if (eq == std::string::npos)
				{
					key = pair;
				}
				else
				{
					key = pair.substr(0, eq);
					value = pair.substr(eq + 1);
				}
				key = decode_uri(key, true);
				value = decode_uri(value, true);
				params[key] = value; // 覆盖同名查询串参数，表单体优先
			}
			pos = amp + 1;
		}
	}
}

request_context build_request_context(const httplib::Request& req, bool parse_urlencoded_body)
{
	request_context ctx;
	ctx.body = req.body;
	for (const auto& kv : req.params)
		ctx.params.insert(kv);
	ctx.req_form = req.form; // multipart 表单（含上传文件）；非 multipart 请求时为空
	ctx.headers = req.headers;
	if (parse_urlencoded_body)
		parse_form_urlencoded(req, ctx.params);
	return ctx;
}

std::string generate_random_token(size_t length, std::string charset) // by ChatGPT
{
    // 用于生成随机数的随机数引擎
    std::random_device rd;
    std::mt19937 gen(rd());  // 随机数生成器
    std::uniform_int_distribution<> dis(0, charset.size() - 1);  // 均匀分布

    std::string token;
    token.reserve(length);  // 为生成的字符串预分配空间

    // 生成随机字符并拼接到 token 字符串
    for (size_t i = 0; i < length; ++i)
        token += charset[dis(gen)];

    return token;
}

std::string get_cookie_value(const std::string& cookie_header, const std::string& cookie_name) // by ChatGPT
{
    size_t start_pos = cookie_header.find(cookie_name + "=");
    if (start_pos == std::string::npos) {
        return "";  // Cookie not found
    }

    start_pos += cookie_name.length() + 1;  // Move past the name and '='
    size_t end_pos = cookie_header.find(";", start_pos);  // Find the end of the cookie value

    if (end_pos == std::string::npos) {
        return cookie_header.substr(start_pos);  // No more cookies, return till the end
    }
    else {
        return cookie_header.substr(start_pos, end_pos - start_pos);  // Extract the cookie value
    }
}
std::string get_token(const httplib::Request & req) // by ChatGPT
{
    // 获取请求头中的 Cookie 字段
    auto it = req.headers.find("Cookie");
    if (it != req.headers.end()) {
        std::string cookie_header = it->second;

        std::string cookie_name = "token";
        std::string cookie_value = get_cookie_value(cookie_header, cookie_name);

        if (!cookie_value.empty()) {
			return cookie_value;  // 输出 Cookie 的值
        }
    }
    return "";
}

std::string responder_page::generate_response(const request_context& ctx)
{
	page_context page;
	page.browser_title = browser_title;
	page.page_title = page_title;
	page.breadcrumb = breadcrumb;
	page.is_index_page = is_index_page;
	page.is_full_custom = is_full_custom;

	generate_page(ctx, page);

	if (page.is_full_custom)
		return page.content;

	return output_html_code(page.browser_title, page.breadcrumb, page.page_title, page.content,
		page.is_index_page, is_request_login);
}

void responder_page::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << std::endl;
	svr.Get(address, [this](const httplib::Request& req, httplib::Response& res) {
        // 在服务器端打印请求
        std::cout << "Received GET request : " << address << std::endl;

		if (is_request_login)
        {
			std::string token = get_token(req);
			if (token == "" || token != authentic_token) // 没有权限访问
            {
                res.set_content(R"(
<script>
    document.cookie="token=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
    window.location.href = "/";
</script>
)", "text/html;charset=utf-8");
                return;
            }
		}

		request_context ctx = build_request_context(req, false);
		res.set_content(generate_response(ctx), "text/html;charset=utf-8");
	});

    if (address[address.size() - 1] != '/')
    {
        std::string address_slash = address + "/";
        svr.Get(address_slash, [target = address](const httplib::Request&, httplib::Response& res) {
            res.set_redirect(target);
        });
    }
}

void responder_post_request::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << std::endl;
    svr.Post(address, [this](const httplib::Request& req, httplib::Response& res) {
        // 在服务器端打印请求
        std::cout << "Received POST request : " << address << std::endl;

        if (is_request_login)
        {
            std::string token = get_token(req);
            if (token == "" || token != authentic_token) // 没有权限访问
            {
                res.set_content("fail : unauthorized", "text/html;charset=utf-8");
                return;
            }
        }

        request_context ctx = build_request_context(req, true);
        res.set_content(generate_response(ctx), "text/html;charset=utf-8");
    });
}

void responder_post_page::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << " (POST)" << std::endl;
    svr.Post(address, [this](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Received POST request : " << address << std::endl;

        if (is_request_login)
        {
            std::string token = get_token(req);
            if (token == "" || token != authentic_token) // 没有权限访问
            {
                res.set_content("fail : unauthorized", "text/html;charset=utf-8");
                return;
            }
        }

        request_context ctx = build_request_context(req, true);
        res.set_content(generate_response(ctx), "text/html;charset=utf-8");
    });
}
