// responder_request.cpp : Responder 代码输出和页面注册基类的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_request.h"
#include "responder_template.h"
#include "encoding.h"

extern std::string authentic_token;

// 解析 application/x-www-form-urlencoded 请求体，填充到 params。
// 原实现只从查询字符串取参数，导致浏览器提交的表单（POST body）取不到值，
// 这里补齐表单体解析，使 params 同时覆盖查询串与表单体两种来源。
static void parse_form_urlencoded(const httplib::Request& req, std::map<std::string, std::string>& params)
{
	auto content_type = req.get_header_value("Content-Type");
	if (content_type.find("application/x-www-form-urlencoded") == std::string::npos)
		return;

	std::string data = req.body;
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
			key = decodeURI(key);
			value = decodeURI(value);
			// 表单中 + 代表空格
			for (auto& c : value)
				if (c == '+') c = ' ';
			for (auto& c : key)
				if (c == '+') c = ' ';
			params[key] = value; // 覆盖同名查询串参数，表单体优先
		}
		pos = amp + 1;
	}
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

std::string responder_page::generate_response()
{
    content = "";
	generate_page();

    if (is_full_custom)
		return content;

    return output_html_code(browser_title, breadcrumb, page_title, content,
        is_index_page, is_request_login);
}

void responder_page::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << std::endl;
	svr.Get(address, [this](const httplib::Request& req, httplib::Response& res) {
        // 获取请求体数据
        body = req.body;

        // 在服务器端打印请求
        std::cout << "Received GET request : " << address << std::endl;

		params.clear();
        auto _param = req.params;
        for (const auto& key : _param) {
            params.insert(std::make_pair(key.first, key.second));
        }

		req_form = req.form; // multipart 表单（含上传文件）；非 multipart 请求时为空

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

		res.set_content(generate_response(), "text/html;charset=utf-8");
	});

    if (address[address.size() - 1] != '/')
    {
        std::string address_slash = address + "/";
        svr.Get(address_slash, [this](const httplib::Request& req, httplib::Response& res) {
            res.set_redirect(address);
        });
    }
}

void responder_post_request::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << std::endl;
    svr.Post(address, [this](const httplib::Request& req, httplib::Response& res) {
        // 获取请求体数据
        body = req.body;

        // 在服务器端打印请求
        std::cout << "Received POST request : " << address << std::endl;

        params.clear();
        auto _param = req.params;
        for (const auto& key : _param) {
            params.insert(std::make_pair(key.first, key.second));
        }

        req_form = req.form; // multipart 表单（含上传文件）；非 multipart 请求时为空
        parse_form_urlencoded(req, params); // 解析普通表单体

        if (is_request_login)
        {
            std::string token = get_token(req);
            if (token == "" || token != authentic_token) // 没有权限访问
            {
                res.set_content("fail : unauthorized", "text/html;charset=utf-8");
                return;
            }
        }

        res.set_content(generate_response(), "text/html;charset=utf-8");
    });
}

void responder_post_page::set_handler(httplib::Server& svr)
{
    init();
    std::cout << "Set Handler: " << address << " (POST)" << std::endl;
    svr.Post(address, [this](const httplib::Request& req, httplib::Response& res) {
        body = req.body;

        std::cout << "Received POST request : " << address << std::endl;

        params.clear();
        auto _param = req.params;
        for (const auto& key : _param) {
            params.insert(std::make_pair(key.first, key.second));
        }

        req_form = req.form;                 // multipart 表单（含上传文件）
        parse_form_urlencoded(req, params);  // 普通表单体

        if (is_request_login)
        {
            std::string token = get_token(req);
            if (token == "" || token != authentic_token) // 没有权限访问
            {
                res.set_content("fail : unauthorized", "text/html;charset=utf-8");
                return;
            }
        }

        res.set_content(generate_response(), "text/html;charset=utf-8");
    });
}
