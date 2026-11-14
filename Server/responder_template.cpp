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

// TODO: 替换部分冗余的旧版代码
std::string output_html_code(std::string _browser_title, std::string _breadcrumb, std::string _page_title, 
    std::string _content, int _is_index_page, bool is_login_required)
{
    if (_is_index_page) // PT_INDEX
        return output_html_code(_browser_title, _breadcrumb, _page_title, R"(
  <main id="main" data-aos="fade-up">

    <!-- ======= Breadcrumbs ======= -->
    <section class="breadcrumbs">
      <div class="container">

        <div class="d-flex justify-content-between align-items-center">
          <h2>)" + _page_title + R"(</h2>
        </div>
      </div>
    </section>

    <section id="portfolio-details" class="portfolio-details">
      <div class="container">
)" + _content + R"(
      </div>
    </section>

  </main><!-- End #main -->
)", false, is_login_required);

    std::string TopTitle = "成都七中科学技术协会", Email = "cdqz_sta@qq.com";

    // PT_FRAME
    return R"(
<!DOCTYPE html>
<html lang="zh">

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta content="width=device-width, initial-scale=1.0" name="viewport">

  <title>)" + _browser_title + R"(</title>
  <link rel="icon" href="/assets/favicon.ico" type="image/x-icon">

  <!-- Google Fonts -->
  <link href="/assets/fonts/fonts.css" rel="stylesheet">

  <!-- Vendor CSS Files -->
  <link href="/assets/vendor/aos/aos.css" rel="stylesheet">
  <link href="/assets/vendor/bootstrap/css/bootstrap.min.css" rel="stylesheet">
  <link href="/assets/vendor/bootstrap-icons/bootstrap-icons.css" rel="stylesheet">
  <link href="/assets/vendor/swiper/swiper-bundle.min.css" rel="stylesheet">
  <link href="/assets/vendor/notyf/notyf.min.css" rel="stylesheet">

  <!-- Template Main CSS File -->
  <link href="/assets/css/style.css" rel="stylesheet">

  <!-- Addons JS File -->
  <script src="/assets/js/jquery-3.6.3.min.js"></script>
  <script src="/assets/js/sweetalert.min.js"></script>

</head>

<body>
  <!-- ======= Top Bar ======= -->
  <section id="topbar" class="d-flex align-items-center">
    <div class="container d-flex justify-content-center justify-content-md-between">
      <div class="contact-info d-flex align-items-center">
        <i class="=d-flex align-items-center">)" + TopTitle + R"(</i>
      </div>
      <div class="social-links d-none d-md-flex align-items-center">
        <a href="mailto:)" + Email + R"(" class="envelope"><i class="bi bi-envelope"></i></a>
      </div>
    </div>
  </section>

  <!-- ======= Header ======= -->
  <header id="header" class="d-flex align-items-center">
    <div class="container d-flex align-items-center justify-content-between">

      <h1 class="logo"><a href="/main">Responder<span>.</span></a></h1>
)" + (is_login_required ? R"(
      <nav id="navbar" class="navbar">
        <ul>
          <li><a class="nav-link" href="/main">主页</a></li>
          <li><a class="nav-link" href="/main/problems">题库管理</a></li>
          <li><a class="nav-link" href="/logout">退出</a></li>
        </ul>
        <i class="bi bi-list mobile-nav-toggle"></i>
      </nav><!-- .navbar -->
)" : "") + R"(
    </div>
  </header><!-- End Header -->

)" + _content + R"(

  <!-- ======= Footer ======= -->
  <footer id="footer">
    <div class="container py-4">
      <div class="row justify-content-center">
		<center>
		  <br>
		  <h4 style="font-family: 'Poppins', sans-serif;">Responder by STA</h4>
		  <h5 style="font-family: 'Poppins', sans-serif;">版本 )" + VERSION + R"(</h5>)" +
#ifdef BETA
            + "\n<p> 仅用于测试。版本 " + get_compile_info() + "</p>" +
#else
            + "\n<p> 版本 " + get_compile_info() + "</p>" +
#endif
            R"(
		  <br>
          Copyright &copy; 2023-2026 <strong><span>Science &amp; Technology Association of Chengdu No.7 High School (STA)</span></strong>. All Rights Reserved.
	    </center>
      </div>
    </div>
  </footer><!-- End Footer -->

  <div id="preloader"></div>
  <a href="#" class="back-to-top d-flex align-items-center justify-content-center"><i class="bi bi-arrow-up-short"></i></a>


  <!-- Vendor JS Files -->
  <script src="/assets/vendor/aos/aos.js"></script>
  <script src="/assets/vendor/bootstrap/js/bootstrap.bundle.min.js"></script>
  <script src="/assets/vendor/isotope-layout/isotope.pkgd.min.js"></script>
  <script src="/assets/vendor/swiper/swiper-bundle.min.js"></script>
  <script src="/assets/vendor/waypoints/noframework.waypoints.js"></script>
  <script src="/assets/vendor/notyf/notyf.min.js"></script>

  <!-- Template Main JS File -->
  <script src="/assets/js/main.js"></script>

</body>

</html>
)";

}