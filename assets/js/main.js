/**
 * Responder 控制台前端脚本
 */
(function () {
  "use strict";

  const select = (el, all = false) => {
    el = el.trim();
    return all ? [...document.querySelectorAll(el)] : document.querySelector(el);
  };

  // 移动端导航折叠
  const mobileToggle = select(".mobile-nav-toggle");
  const navbar = select("#navbar");
  if (mobileToggle && navbar) {
    mobileToggle.addEventListener("click", function () {
      navbar.classList.toggle("navbar-mobile");
      const icon = mobileToggle.querySelector("i");
      if (icon) {
        icon.classList.toggle("bi-list");
        icon.classList.toggle("bi-x");
      }
    });
  }

  // 高亮当前导航项
  const path = window.location.pathname.replace(/\/+$/, "") || "/";
  select("#navbar .nav-link", true).forEach((link) => {
    const href = (link.getAttribute("href") || "").replace(/\/+$/, "");
    if (!href || href === "/logout") return;
    if (
      (href === "/main" && path === "/main") ||
      (href !== "/main" && path.startsWith(href))
    ) {
      link.classList.add("active");
    }
  });
})();