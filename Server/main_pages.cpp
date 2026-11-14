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
    browser_title = "欢迎 - Responder | 简单易用的知识竞赛组织系统";

	is_request_login = false;
}
void welcome_page::generate_page() // '/'
{
    content = R"(
  <!-- ======= Hero Section ======= -->
  <section id="hero" class="d-flex align-items-center">
    <div class="container" data-aos="zoom-out" data-aos-delay="100">
      <h1>欢迎使用 <span>Responder</span></h1>
      <h2>由STA开发的，简单易用的知识竞赛组织系统</h2>
      <div class="d-flex">
        <a href="/start" class="btn-get-started">开始使用</a>
      </div>
    </div>
  </section><!-- End Hero -->
  <br>
)";
}

void start_page::init()
{
    address = "/start";
    browser_title = "开始 - Responder | 简单易用的知识竞赛组织系统";
    breadcrumb = "";
    page_title = "开始";

    is_request_login = false;
}
void start_page::generate_page()
{
    content = R"(
  <!-- ======= Portfolio Details Section ======= -->
  <section id="portfolio-details" class="portfolio-details">
    <div class="container">

    <div class="p-5 mb-4 bg-body-tertiary rounded-3">
      <div class="container-fluid py-7">
        <div class="row" style="display:flex; align-items:center; justify-content:center;">

        <div class="col-lg-7">
          <h1 class="display-7 fw-bold">开始</h1>
          <p class="fs-6">请使用服务端程序上提供的一次性 6 位 PIN 码登录 Responder 。</p>
        </div>
        <div class="col-lg-5">
          <div class="portfolio-info">
            <h3>身份验证</h3>
            <ul>
			  <p>请输入PIN码，验证您的访问权限。</p>
              <form class="pinlogin" method="post">
                <div class="form-floating mb-3">
                  <input class="form-control form-control-lg rounded-3 text-primary" type="number" name="pin" id="pin" autocomplete="off" required>
                  <label for="PIN">PIN</label>
                </div>
                <button type="submit" class="btn btn-primary" id="submitbutton">确定</button>
              </form>

            </ul>
          </div>
        </div>

        </div>

      </div>
    </div>

    </div>
  </section><!-- End Portfolio Details Section -->

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
    browser_title = "主页 - Responder | 简单易用的知识竞赛组织系统";
    breadcrumb = "";
    page_title = "主页";

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
<div class="row d-flex align-items-stretch">

  <div class="col-lg-4 d-flex">
    <div class="p-5 mb-4 bg-body-tertiary w-100" style="display:flex; align-items:center; justify-content:center;">
      <div class="container-fluid py-7">
        <h1 class="display-7 fw-bold" style="font-family: 'Poppins', sans-serif;">欢迎使用 <span class="text-primary">Responder</span></h1>
        <p class="fs-6">这是一个由成都七中科学技术协会开发，用于服务学校科技活动月等场景下的知识竞答比赛的专用系统。</p>
      </div>
    </div>
  </div>

  <div class="col-lg-4 d-flex" style="margin-bottom:25px">
    <div class="portfolio-info w-100">
      <h3>题目</h3>
      <table class="table table-hover">
      <tr>
        <th>类型</th>
        <th>剩余数量</th>
      </tr>
      <tr>
        <td>选择题</td>
        <td>
          )" + std::to_string(choice_unused) + R"( / )" + std::to_string(choice_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(choice_total == 0 ? 0 : (int)round((double)choice_unused / choice_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      <tr>
        <td>填空题</td>
        <td>
          )" + std::to_string(completion_unused) + R"( / )" + std::to_string(completion_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(completion_total == 0 ? 0 : (int)round((double)completion_unused / completion_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      <tr>
        <td>信息题</td>
        <td>
          )" + std::to_string(info_unused) + R"( / )" + std::to_string(info_total) + R"(
          <div class="progress" role="progressbar">
            <div class="progress-bar" style="width: )" + std::to_string(info_total == 0 ? 0 : (int)round((double)info_unused / info_total * 100)) + R"(%"></div>
          </div>
        </td>
      </tr>
      </table>
    </div> 
  </div>

  <div class="col-lg-4 d-flex" style="margin-bottom:25px">
    <div class="portfolio-info w-100">
      <div class="row">
        <div class="col-md">
          <h3>比赛列表</h3>
        </div>
        <div class="col-md" align="right">
          <a href="/main/competition/new" class="btn btn-success btn-sm"><i class="bi bi-plus-lg"></i> 新建比赛</a>
        </div>
      </div>
      <div style="overflow:auto; height:250px">
      <table class="table table-hover" >
      <tr>
        <th>名称</th>
        <th>状态</th>
        <th>操作</th>
      </tr>
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
        <td>)" + c.name + R"(</td>
        <td>)" + status + R"(</td>
        <td><a href="/main/competition/detail?guid=)" + c.guid + R"(" class="btn btn-primary btn-sm">管理</a></td>
      </tr>)";
    }

    content += R"(
      </table>
)";

    if (comlist.size() == 0)
		content += "<p align=\"center\"> 暂无比赛。</p>";

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
