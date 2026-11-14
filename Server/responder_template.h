// responder_template.h : Responder 代码模板的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_TEMPLATE_H
#define RESPONDER_TEMPLATE_H

#include "responder_request.h"

#define VERSION "2.0 Beta 5"
#define BRANCH  "res_refresh"

std::string get_compile_info();

std::string output_html_code(std::string _browser_title, std::string _breadcrumb, std::string _page_title,
    std::string _content, int _is_index_page, bool is_login_required);

#endif // RESPONDER_TEMPLATE_H
