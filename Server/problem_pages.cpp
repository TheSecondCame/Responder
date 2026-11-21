// problem_pages.cpp : 题库管理页面与处理器的实现。
// 本文件把原 Problems Generator 命令行工具的功能（转换 / 合并 / 查重 / 备份）
// 合并进 Server，以网页形式提供操作界面。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_pages.h"
#include "responder_problem.h"
#include "functions.h"
#include <sstream>
#include <fstream>

extern problem_list pl;
extern problem_list pl_unused;

namespace
{
	const char* PROBLEM_DIR = "data/problems/";
	const char* PROBLEM_LIST_FILE = "data/problems/problem_list.json";
	const char* BACKUP_DIR = "data/problems_backup/";
	const char* UPLOAD_DIR = "data/upload_tmp/";
	const char* UPLOAD_FILE = "data/upload_tmp/upload.txt";

	struct ProblemStat
	{
		const char* name;
		int total;
		int unused;
	};

	std::vector<ProblemStat> collect_stats()
	{
		std::vector<ProblemStat> stats;
		stats.push_back({ "选择题", (int)pl.choice_problems.size(),     problem_list::count_unused(pl.choice_problems) });
		stats.push_back({ "填空题", (int)pl.completion_problems.size(), problem_list::count_unused(pl.completion_problems) });
		stats.push_back({ "信息题", (int)pl.info_problems.size(),       problem_list::count_unused(pl.info_problems) });
		return stats;
	}

	// 把题库三类的存量拼成表格行
	std::string problem_stats_rows()
	{
		std::string html;
		for (const auto& r : collect_stats())
		{
			int used = r.total - r.unused;
			int percent = (r.total > 0) ? (int)(100.0 * r.unused / r.total) : 0;

			html += "<tr><td>" + std::string(r.name) + "</td>";
			html += "<td>" + std::to_string(r.unused) + " / " + std::to_string(r.total) + "</td>";
			html += "<td>" + std::to_string(used) + "</td>";
			html += R"(<td><div class="progress" role="progressbar" style="height:8px;"><div class="progress-bar" style="width:)";
			html += std::to_string(percent);
			html += R"(%"></div></div></td></tr>)";
		}
		return html;
	}

	std::string problem_stats_table()
	{
		return R"(
          <table class="table table-hover">
            <thead>
              <tr>
                <th>题型</th>
                <th>剩余 / 总数</th>
                <th>已使用</th>
                <th>比例</th>
              </tr>
            </thead>
            <tbody>
              )" + problem_stats_rows() + R"(
            </tbody>
          </table>)";
	}

	// 重新载入题库（操作完成后刷新内存中的存量，避免必须重启服务端）
	void reload_problem_list()
	{
		pl.choice_problems.clear();
		pl.completion_problems.clear();
		pl.info_problems.clear();
		pl.load(PROBLEM_LIST_FILE);
		pl_unused = pl;
		pl_unused.remove_used();
	}

	// 把题库目录备份到 data/problems_backup/（覆盖旧备份）
	void backup_problems()
	{
		RemoveDir(BACKUP_DIR);
		CreateDirectoryA("data", NULL);
		CreateDirectoryA(BACKUP_DIR, NULL);
		CopyDir(PROBLEM_DIR, BACKUP_DIR);
	}
}

// ==================== 题库管理页面 ====================

void problem_manage_page::init()
{
	address = "/main/problems";
	browser_title = "题库管理 - Responder";
	breadcrumb = R"(<li><a href="/main">主页</a></li><li>题库管理</li>)";
	page_title = "题库管理";

	is_request_login = true;
	is_index_page = true;
}

void problem_manage_page::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
	content = R"(
    <div class="row g-4 align-items-stretch">

      <div class="col-lg-5 d-flex">
        <div class="console-card w-100">
          <h3 class="console-card-header">题库存量</h3>
          <p class="text-secondary mb-3" style="font-size:14px;">「剩余」指尚未在任何比赛中使用过的题目数量。比赛抽题只会从未使用的题目中选取。</p>
)" + problem_stats_table() + R"(
        </div>
      </div>

      <div class="col-lg-7 d-flex">
        <div class="console-card w-100">
          <h3 class="console-card-header">题库操作</h3>
          <p class="text-secondary mb-3" style="font-size:14px;">导入用的 txt 格式：第一行为「复杂度 类型」（复杂度 1–3；类型 0=选择题、1=填空题、2=信息题），其后依次为题干、选项或线索、答案。</p>

          <form method="post" action="/main/problems" enctype="multipart/form-data" id="problemForm">
            <div class="table-responsive">
              <table class="table table-hover align-middle">
                <tr>
                  <th style="width:34%">操作</th>
                  <th style="width:20%">文件</th>
                  <th>说明</th>
                </tr>
                <tr>
                  <td>
                    <div class="form-check">
                      <input class="form-check-input" type="radio" name="action" id="actConvert" value="convert" checked>
                      <label class="form-check-label" for="actConvert"><strong>转换题库</strong></label>
                    </div>
                  </td>
                  <td><span class="badge bg-danger">需要 txt</span></td>
                  <td>用 txt 内容<strong>完整替换</strong>现有题库，题目 ID 从 1 重新编号，并自动去除重复题。</td>
                </tr>
                <tr>
                  <td>
                    <div class="form-check">
                      <input class="form-check-input" type="radio" name="action" id="actMerge" value="merge">
                      <label class="form-check-label" for="actMerge"><strong>合并题库</strong></label>
                    </div>
                  </td>
                  <td><span class="badge bg-danger">需要 txt</span></td>
                  <td>保留现有题目，把 txt 中的新题按新 ID 追加；与现有题目重复的会被跳过。</td>
                </tr>
                <tr>
                  <td>
                    <div class="form-check">
                      <input class="form-check-input" type="radio" name="action" id="actDedup" value="dedup">
                      <label class="form-check-label" for="actDedup"><strong>去除重复题</strong></label>
                    </div>
                  </td>
                  <td><span class="badge bg-success">不需要</span></td>
                  <td>对当前题库去重并重新连续编号，修正题目 ID。</td>
                </tr>
              </table>
            </div>

            <div class="mb-3">
              <label for="problemFile" class="form-label">题库文本文件（.txt）</label>
              <input class="form-control" type="file" name="file" id="problemFile" accept=".txt">
              <div class="form-text">选择「去除重复题」时可以不上传文件。</div>
            </div>

            <div class="alert alert-warning py-2" role="alert">
              <i class="bi bi-exclamation-triangle"></i>
              所有操作执行前都会把当前题库自动备份到 <code>data/problems_backup/</code>（覆盖上一次备份）。
            </div>

            <button type="submit" class="btn btn-success">
              <i class="bi bi-play-fill"></i> 执行操作
            </button>
          </form>

          <hr class="my-4">

          <h3 class="console-card-header mt-4">备份与恢复</h3>
          <p class="text-secondary mb-3" style="font-size:14px;">备份目录：<code>data/problems_backup/</code>。恢复方式：停止服务端，把备份内容复制回 <code>data/problems/</code>，再重新启动。</p>
          <form method="post" action="/main/problems">
            <input type="hidden" name="action" value="backup">
            <button type="submit" class="btn btn-outline-primary btn-sm">
              <i class="bi bi-archive"></i> 立即备份当前题库
            </button>
          </form>

        </div>
      </div>

    </div>

  <script>
    function syncFileRequirement() {
        var action = document.querySelector('input[name="action"]:checked');
        var fileInput = document.getElementById('problemFile');
        var needFile = action && action.value !== 'dedup';
        fileInput.disabled = !needFile;
        fileInput.required = needFile;
    }
    var radios = document.querySelectorAll('input[name="action"]');
    for (var i = 0; i < radios.length; i++) radios[i].addEventListener('change', syncFileRequirement);
    syncFileRequirement();
  </script>
)";
}

// ==================== 题库操作处理器 ====================

void problem_manage_handler::init()
{
	address = "/main/problems";
	browser_title = "题库管理 - Responder";
	breadcrumb = R"(<li><a href="/main">主页</a></li><li>题库管理</li>)";
	page_title = "题库管理";

	is_request_login = true;
	is_index_page = true;
}

void problem_manage_handler::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
	std::string action;
	std::string uploaded_filename;

	// action 可能来自 multipart 表单字段，也可能来自普通表单体（params 已解析两者）
	if (req_form.has_field("action"))
		action = req_form.get_field("action");
	else if (params.find("action") != params.end())
		action = params["action"];

	if (req_form.has_file("file"))
	{
		auto file = req_form.get_file("file");
		if (!file.filename.empty())
		{
			// 只取文件名，忽略客户端可能带上的路径
			size_t slash = file.filename.find_last_of("/\\");
			uploaded_filename = (slash == std::string::npos) ? file.filename : file.filename.substr(slash + 1);

			CreateDirectoryA("data", NULL);
			CreateDirectoryA("data/upload_tmp", NULL);
			std::ofstream fout(UPLOAD_FILE, std::ios::binary);
			fout.write(file.content.data(), (std::streamsize)file.content.size());
			fout.close();
		}
	}

	// 捕获题库操作过程中 std::cout 的输出，作为执行日志返回页面
	std::ostringstream log;
	std::streambuf* old_buf = std::cout.rdbuf(log.rdbuf());

	bool ok = true;

	try
	{
		if (action == "convert" || action == "merge")
		{
			if (uploaded_filename.empty())
			{
				ok = false;
				result_title = "执行失败";
				result_summary = "没有收到上传的题库文件，请重新选择后再试。";
			}
			else
			{
				std::cout << "=== 操作："
					<< (action == "convert" ? "转换题库（覆盖现有题库）" : "合并题库（保留现有题目）")
					<< " ===" << std::endl;
				std::cout << "上传文件：" << uploaded_filename << std::endl;

				backup_problems();
				std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;

				if (action == "convert")
					problem_list::convert(UPLOAD_FILE, PROBLEM_DIR);
				else
					problem_list::merge(UPLOAD_FILE, PROBLEM_DIR);

				reload_problem_list();
				result_title = "执行成功";
				result_summary = (action == "convert")
					? "题库已按上传文件完整替换。"
					: "上传文件中的新题已合并进题库。";
			}
		}
		else if (action == "dedup")
		{
			std::cout << "=== 操作：去除重复题并重排 ID ===" << std::endl;

			backup_problems();
			std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;

			responder_problem_output_handler handler;
			handler.load(PROBLEM_LIST_FILE);

			int before = (int)(handler.choice_problems.size() + handler.completion_problems.size() + handler.info_problems.size());
			handler.delete_duplicates();
			int after = (int)(handler.choice_problems.size() + handler.completion_problems.size() + handler.info_problems.size());

			RemoveDir(PROBLEM_DIR);
			CreateDirectoryA(PROBLEM_DIR, NULL);
			handler.output_to_folder(PROBLEM_DIR);

			reload_problem_list();
			result_title = "执行成功";
			result_summary = "共删除 " + std::to_string(before - after) + " 道重复题目（操作前 "
				+ std::to_string(before) + " 道，操作后 " + std::to_string(after) + " 道）。";
		}
		else if (action == "backup")
		{
			std::cout << "=== 操作：备份当前题库 ===" << std::endl;
			backup_problems();
			std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;
			result_title = "备份完成";
			result_summary = std::string("当前题库已备份到 ") + BACKUP_DIR + "。";
		}
		else
		{
			ok = false;
			result_title = "执行失败";
			result_summary = "无法识别的操作类型。";
		}
	}
	catch (const std::exception& e)
	{
		ok = false;
		result_title = "执行出错";
		result_summary = std::string("发生异常：") + e.what();
		std::cout << "发生异常：" << e.what() << std::endl;
	}
	catch (...)
	{
		ok = false;
		result_title = "执行出错";
		result_summary = "发生未知异常。";
	}

	std::cout.rdbuf(old_buf);

	// 清理上传的临时文件
	remove(UPLOAD_FILE);

	alert_class = ok ? "alert-success" : "alert-danger";
	alert_icon = ok ? "bi-check-circle" : "bi-x-circle";
	exec_log = log.str();

	content = R"(
    <div class="alert )" + alert_class + R"( d-flex align-items-center mb-4" role="alert">
      <i class="bi )" + alert_icon + R"( me-2"></i>
      <div><strong>)" + result_title + R"(</strong> —— )" + result_summary + R"(</div>
    </div>

    <div class="console-card mb-4">
      <h3 class="console-card-header">操作后的题库存量</h3>
)" + problem_stats_table() + R"(
    </div>

    <div class="console-card">
      <h3 class="console-card-header">执行日志</h3>
      <pre class="console-log-pre mb-3">)" + exec_log + R"(</pre>
      <div class="d-flex flex-wrap gap-2">
        <a href="/main/problems" class="btn btn-primary"><i class="bi bi-arrow-left"></i> 返回题库管理</a>
        <a href="/main" class="btn btn-outline-primary">回到主页</a>
      </div>
    </div>
)";
}
