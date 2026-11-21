// responder_pages.h : Responder 主页和菜单页派生类定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_MAIN_PAGES_H
#define RESPONDER_MAIN_PAGES_H

#include "responder_request.h"

class welcome_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class start_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class login_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class main_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class logout_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class new_competition_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class new_competition_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_edit_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class competition_edit_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_delete_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_detail_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class competition_display_page : public responder_page
{
protected:
	void init() override;
	void generate_page() override;
};

class competition_display_scores_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_display_problem_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_display_session_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

class competition_enddisplay_handler : public responder_post_request
{
protected:
	void init() override;
	std::string generate_response() override;
};

// ===== 题库管理（原 Problems Generator 的 Web 化）=====

class problem_manage_page : public responder_page
	// 题库管理主页：显示题库存量，提供转换 / 合并 / 查重 / 备份等操作入口
{
protected:
	void init() override;
	void generate_page() override;
};

class problem_manage_handler : public responder_post_page
	// 处理题库操作表单：接收上传的 txt，执行 转换 / 合并 / 查重 / 备份，并渲染结果页
{
protected:
	void init() override;
	void generate_page() override;

	std::string alert_class;           // 结果提示框的样式类（alert-success / alert-danger）
	std::string alert_icon;            // 结果提示框的图标类
	std::string result_title;          // 结果标题
	std::string result_summary;        // 结果摘要
	std::string exec_log;              // 题库操作的执行日志
};

#endif // RESPONDER_MAIN_PAGES_H
