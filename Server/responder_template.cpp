// responder_template.cpp : Responder 代码模板的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "build.h"
#include "responder_template.h"

std::string get_compile_info()
{
    std::string compile_info;
    const char* month_names[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
    std::string timetemp;
    timetemp = __DATE__;
    if (timetemp[4] == ' ')
        timetemp[4] = '0';
    std::string year = timetemp.substr(9, 2);
    std::string day = timetemp.substr(4, 2);
    char month[5];
    std::string s_month = timetemp.substr(0, 3);
    int tmonth;
    for (int i = 0; i <= 11; i++)
        if (s_month == month_names[i])
        {
            tmonth = i + 1;
            break;
        }
    if (tmonth < 10)
        month[0] = '0', month[1] = tmonth + '0', month[2] = '\0';
    else
        month[0] = '1', month[1] = tmonth % 10 + '0', month[2] = '\0';
    timetemp = __TIME__;
    std::string hour = timetemp.substr(0, 2);
    std::string min = timetemp.substr(3, 2);
    compile_info = "Build " + (std::string)PROGRAM_BUILD_S + "." + BRANCH + "." + year + month + day + "-" + hour + min;
    return compile_info;
}

// 统一控制台页面模板
std::string output_html_code(std::string _browser_title, std::string _breadcrumb, std::string _page_title, 
    std::string _content, int _is_index_page, bool is_login_required)
{
    if (_is_index_page) // PT_INDEX
    {
        std::string breadcrumb_html;
        if (!_breadcrumb.empty())
            breadcrumb_html = "<ol class=\"console-breadcrumb\">" + _breadcrumb + "</ol>";

        return output_html_code(_browser_title, _breadcrumb, _page_title, R"(
  <main id="main" class="console-main">
    <section class="console-page-header">
      <div class="container">
        <div class="console-page-header-inner">
          <div>
            )" + breadcrumb_html + R"(
            <h1 class="console-page-title">)" + _page_title + R"(</h1>
          </div>
        </div>
      </div>
    </section>

    <section class="console-content">
      <div class="container">
)" + _content + R"(
      </div>
    </section>
  </main>
)", false, is_login_required);
    }

    std::string home_href = is_login_required ? "/main" : "/";

    // PT_FRAME
    return R"(<!DOCTYPE html>
<html lang="zh-CN">

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta content="width=device-width, initial-scale=1.0" name="viewport">

  <title>)" + _browser_title + R"(</title>
  <link rel="icon" href="/assets/favicon.ico" type="image/x-icon">

  <!-- Local Fonts -->
  <link href="/assets/fonts/fonts.css" rel="stylesheet">

  <!-- Vendor CSS Files -->
  <link href="/assets/vendor/bootstrap/css/bootstrap.min.css" rel="stylesheet">
  <link href="/assets/vendor/bootstrap-icons/bootstrap-icons.css" rel="stylesheet">
  <link href="/assets/vendor/notyf/notyf.min.css" rel="stylesheet">

  <!-- Console Main CSS File -->
  <link href="/assets/css/style.css?)" + std::string(PROGRAM_BUILD_S) + R"(" rel="stylesheet">

  <!-- Addons JS File -->
  <script src="/assets/js/jquery-3.6.3.min.js"></script>
  <script src="/assets/js/sweetalert.min.js"></script>
</head>

<body class="console-body">

  <header id="header" class="console-header">
    <div class="container d-flex align-items-center justify-content-between">
      <a class="console-brand" href=")" + home_href + R"(">
        <span class="console-brand-mark"><i class="bi bi-lightning-charge-fill"></i></span>
        <span class="console-brand-text">
          <span class="console-brand-name">Responder<span>.</span></span>
          <span class="console-brand-sub">STA / 竞赛控制台</span>
        </span>
      </a>
)" + (is_login_required ? R"(
      <nav id="navbar" class="console-nav">
        <ul>
          <li><a class="nav-link" href="/main"><i class="bi bi-grid-1x2"></i><span>主页</span></a></li>
          <li><a class="nav-link" href="/main/problems"><i class="bi bi-collection"></i><span>题库管理</span></a></li>
          <li><a class="nav-link" href="/main/competition/new"><i class="bi bi-plus-circle"></i><span>新建比赛</span></a></li>
          <li><a class="nav-link nav-link-logout" href="/logout"><i class="bi bi-box-arrow-right"></i><span>退出</span></a></li>
        </ul>
        <button type="button" class="mobile-nav-toggle" aria-label="切换导航"><i class="bi bi-list"></i></button>
      </nav>
)" : R"(
      <div class="console-header-meta d-none d-sm-flex align-items-center gap-2">
        <span class="console-org-pill">成都七中科学技术协会</span>
      </div>
)") + R"(
    </div>
  </header>

  <div class="console-shell-body">
)" + _content + R"(
  </div>

  <footer id="footer" class="console-footer">
    <div class="container d-flex flex-column flex-md-row align-items-center justify-content-between gap-2">
      <div class="console-footer-brand">
        <strong>Responder by STA</strong>
        <span>· 版本 )" + std::string(VERSION) + R"(</span>)" +
#ifdef BETA
        "<span> · 测试版 " + get_compile_info() + "</span>" +
#else
        "<span> · " + get_compile_info() + "</span>" +
#endif
        R"(
      </div>
      <div class="console-footer-copy">
        &copy; 2023-2026 Science &amp; Technology Association of Chengdu No.7 High School (STA)
      </div>
    </div>
  </footer>

  <!-- Vendor JS Files -->
  <script src="/assets/vendor/bootstrap/js/bootstrap.bundle.min.js"></script>
  <script src="/assets/vendor/notyf/notyf.min.js"></script>

  <!-- Console Main JS File -->
  <script src="/assets/js/main.js?)" + std::string(PROGRAM_BUILD_S) + R"("></script>

</body>

</html>
)";
}