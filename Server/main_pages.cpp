// main_pages.cpp : Responder 主页和菜单页派生类实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_pages.h"
#include "responder_problem.h"
#include "responder_competition.h"

extern problem_list pl;
extern std::string authentic_pin;
extern std::string authentic_token;

void welcome_page::init()
{
    is_index_page = false;
    address = "/";
    browser_title = "欢迎 - Responder";

	is_request_login = false;
}
void welcome_page::generate_page() // '/'
{
    content = R"(
  <section class="console-welcome">
    <div class="container">
      <div class="console-welcome-hero">
        <div class="console-welcome-badge">
          <i class="bi bi-lightning-charge-fill"></i>
          <span>成都七中科学技术协会 · 知识竞赛组织系统</span>
        </div>
        <h1 class="console-welcome-title">欢迎使用 <span>Responder</span></h1>
        <p class="console-welcome-desc">面向校园知识竞赛与活动现场的轻量局域网控制台，集成题库管理、多队抢答轮答与自适应投屏大屏。</p>
        <div class="d-flex flex-wrap gap-2">
          <a href="/start" class="btn btn-primary btn-lg px-4">
            <i class="bi bi-shield-lock"></i>
            <span>输入 PIN 开始使用</span>
          </a>
        </div>

        <div class="console-feature-grid">
          <div class="console-feature-item">
            <i class="bi bi-collection"></i>
            <h4>本地题库管理</h4>
            <p>支持选择、填空、信息三类题型的导入、查重、合并与自动备份。</p>
          </div>
          <div class="console-feature-item">
            <i class="bi bi-stopwatch"></i>
            <h4>抢答 / 轮答计分</h4>
            <p>支持键盘抢答、自动轮次流转、提早抢答警告与自动加减分。</p>
          </div>
          <div class="console-feature-item">
            <i class="bi bi-display"></i>
            <h4>投屏大屏展示</h4>
            <p>自适应现场投影仪与大屏分辨率，支持刷新恢复与高对比度播报。</p>
          </div>
        </div>
      </div>
    </div>
  </section>
)";
}

void start_page::init()
{
    is_index_page = false;
    address = "/start";
    browser_title = "开始 - Responder";
    breadcrumb = "";
    page_title = "开始";

    is_request_login = false;
}
void start_page::generate_page()
{
    content = R"(
  <section class="console-welcome">
    <div class="container">
      <div class="console-login-wrap">
        <div class="console-card p-4 p-md-5">
          <div class="row g-4 align-items-center">
            <div class="col-lg-6">
              <div class="console-welcome-badge mb-3">
                <i class="bi bi-key-fill"></i>
                <span>一次性身份验证</span>
              </div>
              <h1 class="fs-2 fw-bold mb-2">登录控制台</h1>
              <p class="text-secondary mb-0">请在桌面管理端「服务管理」卡片或命令行日志中查看 6 位登录 PIN 码并输入验证。</p>
            </div>
            <div class="col-lg-6">
              <div class="p-4 rounded-3" style="background: var(--r-field); border: 1px solid var(--r-border);">
                <form class="pinlogin" method="post">
                  <label for="pin" class="form-label fw-semibold mb-2">6 位登录 PIN</label>
                  <input class="form-control console-pin-input mb-3" type="text" inputmode="numeric" pattern="[0-9]*" maxlength="6" name="pin" id="pin" placeholder="输入 6 位数字" autocomplete="off" autofocus required>
                  <button type="submit" class="btn btn-primary w-100 py-2" id="submitbutton">
                    <i class="bi bi-box-arrow-in-right"></i>
                    <span>验证并进入</span>
                  </button>
                </form>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <script>
    function getCookie(cname) {
        var name = cname + "=";
        var decodedCookie = decodeURIComponent(document.cookie);
        var ca = decodedCookie.split(';');
        for (var i = 0; i < ca.length; i++) {
            var c = ca[i];
            while (c.charAt(0) == ' ') {
                c = c.substring(1);
            }
            if (c.indexOf(name) == 0) {
                return c.substring(name.length, c.length);
            }
        }
        return "";
    }
    if (getCookie("token") != "") window.location.href = "/main";
  </script>

)";
}

void login_handler::init()
{
    address = "/start";

    is_request_login = false;
}
std::string login_handler::generate_response()
{
	// TODO: 增加次数检测，防止暴力破解

    if (params.find("pin") != params.end())
        if (params["pin"] == authentic_pin)
        {
			authentic_pin = generate_random_token(6, "0123456789"); // 生成新的PIN码，旧的PIN码失效

            std::cout << "=================================================" << std::endl;
            std::cout << "Successful login. New PIN code: " << authentic_pin << std::endl;
            std::cout << "=================================================" << std::endl;

		    return R"(
<script> 
    document.cookie = "token=)" + authentic_token + R"(; path=/;";
    window.location.href = "/main";
</script>
    )";
        }

    return R"(
<script> window.location.href = "/start"; </script>
)";
}

void main_page::init()
{
    address = "/main";
    browser_title = "主页 - Responder";
    breadcrumb = "";
    page_title = "控制台主页";

	is_request_login = true;
}
void main_page::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
    int completion_total = pl.completion_problems.size();
	int choice_total = pl.choice_problems.size();
	int info_total = pl.info_problems.size();

	int completion_unused = problem_list::count_unused(pl.completion_problems);
	int choice_unused = problem_list::count_unused(pl.choice_problems);
	int info_unused = problem_list::count_unused(pl.info_problems);

    content = (std::string)R"(
<div class="row g-4 align-items-stretch">

  <div class="col-lg-4 d-flex">
    <div class="console-card w-100 d-flex flex-column justify-content-between">
      <div>
        <div class="console-welcome-badge mb-3">
          <i class="bi bi-lightning-charge-fill"></i>
          <span>Responder 控制台</span>
        </div>
        <h2 class="fs-4 fw-bold mb-2">竞赛组织工作台</h2>
        <p class="text-secondary mb-4" style="font-size:14px; line-height:1.6;">
          由成都七中科学技术协会开发，适用于校园知识竞答比赛的赛前出题、现场抢答轮答与实时计分。
        </p>
      </div>
      <div class="d-flex flex-wrap gap-2">
        <a href="/main/competition/new" class="btn btn-primary btn-sm"><i class="bi bi-plus-lg"></i> 新建比赛</a>
        <a href="/main/problems" class="btn btn-outline-primary btn-sm"><i class="bi bi-collection"></i> 管理题库</a>
      </div>
    </div>
  </div>

  <div class="col-lg-4 d-flex">
    <div class="console-card w-100">
      <div class="d-flex align-items-center justify-content-between mb-3 pb-2 border-bottom">
        <h3 class="mb-0 border-0 pb-0" style="font-size:16px;font-weight:700;">题库存量</h3>
        <a href="/main/problems" class="btn btn-outline-primary btn-sm">详情</a>
      </div>
      <table class="table table-hover">
      <thead>
      <tr>
        <th>题型</th>
        <th>可用 / 总数</th>
      </tr>
      </thead>
      <tbody>
      <tr>
        <td>选择题</td>
        <td class="tabular-nums">
          )" + std::to_string(choice_unused) + R"( / )" + std::to_string(choice_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(choice_total == 0 ? 0 : (int)round((double)choice_unused / choice_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      <tr>
        <td>填空题</td>
        <td class="tabular-nums">
          )" + std::to_string(completion_unused) + R"( / )" + std::to_string(completion_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(completion_total == 0 ? 0 : (int)round((double)completion_unused / completion_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      <tr>
        <td>信息题</td>
        <td class="tabular-nums">
          )" + std::to_string(info_unused) + R"( / )" + std::to_string(info_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(info_total == 0 ? 0 : (int)round((double)info_unused / info_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      </tbody>
      </table>
    </div> 
  </div>

  <div class="col-lg-4 d-flex">
    <div class="console-card w-100">
      <div class="d-flex align-items-center justify-content-between mb-3 pb-2 border-bottom">
        <h3 class="mb-0 border-0 pb-0" style="font-size:16px;font-weight:700;">比赛列表</h3>
        <a href="/main/competition/new" class="btn btn-primary btn-sm"><i class="bi bi-plus-lg"></i> 新建比赛</a>
      </div>
      <div style="overflow:auto; max-height:260px">
      <table class="table table-hover">
      <thead>
      <tr>
        <th>名称</th>
        <th>状态</th>
        <th>操作</th>
      </tr>
      </thead>
      <tbody>
)";

    auto comlist = load_competition_list("data/competitions/list.json");
    std::string status;
    for (auto com : comlist)
    {
        competition c;
        c.load(com);

        switch (c.competition_status)
        {
        case competition::status::FINISHED:
            status = R"(<span class="badge text-bg-danger">已结束</span>)";
            break;
        case competition::status::NOT_STARTED:
            status = R"(<span class="badge text-bg-success">未开赛</span>)";
            break;
        case competition::status::ONGOING:
            status = R"(<span class="badge text-bg-primary">进行中</span>)";
            break;
        case competition::status::PAUSED:
            status = R"(<span class="badge text-bg-warning">暂停中</span>)";
            break;
        }

        content += R"(
      <tr>
        <td class="fw-semibold">)" + c.name + R"(</td>
        <td>)" + status + R"(</td>
        <td><a href="/main/competition/detail?guid=)" + c.guid + R"(" class="btn btn-outline-primary btn-sm">管理</a></td>
      </tr>)";
    }

    content += R"(
      </tbody>
      </table>
)";

    if (comlist.size() == 0)
		content += R"(<div class="text-center text-secondary py-4" style="font-size:14px;">暂无比赛，点击右上角「新建比赛」开始。</div>)";

    content += R"(
      </div> 
    </div> 
  </div>

</div>)";
}

void logout_page::init()
{
    address = "/logout";

    is_full_custom = true;
    is_request_login = true;
}
void logout_page::generate_page()
{
	content = R"(
<script>
    document.cookie="token=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;";
    window.location.href = "/";
</script>
    )";
}
