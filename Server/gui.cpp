// gui.cpp : Responder 桌面管理程序（Win32 原生窗口）。
// 服务、题库、比赛和设置共用同一个桌面管理窗口。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

// 注意：httplib.h 会引入 winsock2.h，必须先于 Windows.h 包含，
// 否则 Windows.h 会先引入旧的 winsock.h 导致类型重定义。
#include "server_core.h"

#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <objidl.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <map>
#include <algorithm>
#include <cstring>

namespace Gdiplus {
	using std::min;
	using std::max;
}
#include <gdiplus.h>

#include "responder_problem.h"
#include "responder_pages.h"
#include "functions.h"
#include "logging.h"
#include "responder_competition.h"
#include "question_editor_ids.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

// ===== 控件 ID =====
#define IDC_STATUS        1001
#define IDC_URL_LOCAL     1002
#define IDC_URL_LAN       1003
#define IDC_LOGIN_PIN     1004
#define IDC_BTN_START     1010
#define IDC_BTN_STOP      1011
#define IDC_BTN_BROWSER   1012
#define IDC_BTN_COPY_LOCAL 1013
#define IDC_BTN_COPY_LAN   1014
#define IDC_BTN_COPY_PIN   1015
#define IDC_STATS         1020
#define IDC_BTN_CONVERT   1021
#define IDC_BTN_MERGE     1022
#define IDC_BTN_DEDUP     1023
#define IDC_BTN_BACKUP    1024
#define IDC_LOG           1030
#define IDC_HINT          1040

// 纯标签控件
#define IDC_TITLE         1049
#define IDC_SUBTITLE      1050
#define IDC_STATIC_SVC    1051
#define IDC_STATIC_URL1   1061
#define IDC_STATIC_URL2   1062
#define IDC_STATIC_PIN    1063
#define IDC_STATIC_PB1    1064
#define IDC_STATIC_PB2    1065
#define IDC_STATIC_LOG    1066

enum : int {
	NavService = 1100, NavProblems, NavMatches, NavSettings,
	Brand, BrandCaption, MatchTitle, MatchList, MatchRefresh, MatchOpen, MatchHint,
	SettingsTitle, AutoScroll, SettingsHint, BankList, BankFilter, BankAdd, BankEdit,
	BankSearchLabel, BankSearch, BankClearSearch
};
static const wchar_t* const pageNames[] = { L"服务管理", L"题库管理", L"比赛管理", L"设置" };
static int currentPage = NavService;
static bool followLog = true;
static const wchar_t* const prefsFile = L".\\data\\desktop.ini";
static HFONT iconFont = nullptr;

// ===== DPI 缩放 =====
// 布局与字号都按此比例缩放，保证在 125% / 150% / 200% 缩放下观感一致。
static double g_scale = 1.0;

static int S(int v)
{
	return (int)(v * g_scale + 0.5);
}

static void InitDpiScale()
{
	HDC dc = GetDC(nullptr);
	int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
	if (dc) ReleaseDC(nullptr, dc);
	if (dpi <= 0) dpi = 96;
	g_scale = dpi / 96.0;
}

// ===== 配色（与网页端 / 比赛大屏统一设计系统）=====
namespace theme
{
	const COLORREF BG            = RGB(243, 246, 251); // 窗口底色 #F3F6FB
	const COLORREF CARD          = RGB(255, 255, 255); // 卡片 #FFFFFF
	const COLORREF BORDER        = RGB(220, 230, 242); // 卡片描边 #DCE6F2
	const COLORREF TEXT          = RGB(19,  35,  58);  // 主文字 #13233A
	const COLORREF TEXT_SUB      = RGB(81,  100, 125); // 次要文字 #51647D
	const COLORREF TEXT_MUTED    = RGB(124, 142, 166); // 弱化文字 #7C8EA6
	const COLORREF ACCENT        = RGB(29,  99,  220); // 主色 #1D63DC
	const COLORREF ACCENT_HOT    = RGB(23,  82,  184); // 悬停 #1752B8
	const COLORREF ACCENT_DN     = RGB(18,  67,  153); // 按下 #124399
	const COLORREF ACCENT_SOFT   = RGB(235, 242, 255); // 浅蓝底 #EBF2FF
	const COLORREF ACCENT_BORDER = RGB(191, 219, 254); // 浅蓝描边 #BFDBFE
	const COLORREF ON            = RGB(21,  128, 61);  // 在线绿 #15803D
	const COLORREF ON_SOFT       = RGB(220, 252, 231); // #DCFCE7
	const COLORREF ON_BORDER     = RGB(134, 239, 172); // #86EFAC
	const COLORREF OFF           = RGB(100, 116, 139); // 离线灰 #64748B
	const COLORREF OFF_SOFT      = RGB(241, 245, 249); // #F1F5F9
	const COLORREF WARN          = RGB(220, 38,  38);  // 错误红 #DC2626
	const COLORREF WARN_SOFT     = RGB(254, 226, 226); // #FEE2E2
	const COLORREF WARN_BORDER   = RGB(252, 165, 165); // #FCA5A5
	const COLORREF AMBER         = RGB(180, 83,  9);   // 过渡黄 #B45309
	const COLORREF AMBER_SOFT    = RGB(254, 243, 199); // #FEF3C7
	const COLORREF AMBER_BORDER  = RGB(252, 211, 77);  // #FCD34D
	const COLORREF FIELD         = RGB(248, 250, 252); // 输入框底 #F8FAFC
}

// ===== 字体 =====
static HFONT g_font_ui = nullptr;      // 正文 14px
static HFONT g_font_small = nullptr;   // 小字 12px
static HFONT g_font_sect = nullptr;    // 区块标题 15px 粗
static HFONT g_font_title = nullptr;   // 窗口标题 22px 粗
static HFONT g_font_pin = nullptr;     // PIN 24px 粗等宽
static HFONT g_font_stat = nullptr;    // 统计数字 15px
static HFONT g_font_mono = nullptr;    // 日志 13px 等宽

// ===== 运行时状态（仅 GUI 线程访问）=====
static HWND g_status = nullptr;
static HWND g_url_local = nullptr;
static HWND g_url_lan = nullptr;
static HWND g_pin = nullptr;
static HWND g_stats = nullptr;
static HWND g_log = nullptr;
static HWND g_hint = nullptr;
static HWND g_btn_start = nullptr;
static HWND g_btn_stop = nullptr;
static HWND g_btn_copy_local = nullptr;
static HWND g_btn_copy_lan = nullptr;
static HWND g_btn_copy_pin = nullptr;

static HWND g_btn_convert = nullptr;
static HWND g_btn_merge = nullptr;
static HWND g_btn_dedup = nullptr;
static HWND g_btn_backup = nullptr;
static HWND g_btn_browser = nullptr;

static std::atomic<bool> g_op_running{ false };  // 是否正在执行题库操作
static std::atomic<bool> g_stopping{ false };    // 是否正在停止服务
static std::atomic<bool> g_starting{ false };    // 是否正在启动服务
static std::string g_last_shown_pin;
static int g_status_kind = -1; // 0=离线 1=在线 2=过渡 3=错误
static std::wstring g_status_text;
static ULONGLONG g_copy_feedback_until[3] = { 0, 0, 0 };
static bool g_search_focused = false;
static int g_bank_filter = 0; // 0=全部题型 1=选择题 2=填空题 3=信息题

// 卡片与容器矩形（WM_SIZE 时按客户区重算）
static RECT g_panel{};
static RECT g_card_log = { 0, 0, 0, 0 };
static RECT g_box_url_local = { 0, 0, 0, 0 };
static RECT g_box_url_lan = { 0, 0, 0, 0 };
static RECT g_box_pin = { 0, 0, 0, 0 };
static RECT g_box_log = { 0, 0, 0, 0 };
static RECT g_box_search = { 0, 0, 0, 0 };
static RECT g_box_bank_list = { 0, 0, 0, 0 };
static RECT g_box_match_list = { 0, 0, 0, 0 };

extern problem_list pl;
extern problem_list pl_unused;

namespace
{
	// 题库相关路径（与 problem_pages.cpp 保持一致）
	const char* PROBLEM_DIR = "data/problems/";
	const char* PROBLEM_LIST_FILE = "data/problems/problem_list.json";
	const char* BACKUP_DIR = "data/problems_backup/";
	const char* UPLOAD_FILE = "data/upload_tmp/upload.txt";

	void SetText(HWND h, const std::wstring& s)
	{
		if (h) SetWindowTextW(h, s.c_str());
	}

	std::wstring GetText(HWND h)
	{
		if (!h) return L"";
		int len = GetWindowTextLengthW(h);
		if (len <= 0) return L"";
		std::wstring out((size_t)len + 1, L'\0');
		int n = GetWindowTextW(h, out.data(), len + 1);
		out.resize(n);
		return out;
	}

	std::wstring ToWide(const std::string& s)
	{
		if (s.empty()) return L"";
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring out((size_t)n, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
		return out;
	}

	std::string ToUtf8(const std::wstring& s)
	{
		if (s.empty()) return "";
		int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
		std::string out((size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
		return out;
	}

	Gdiplus::Color ToGpColor(COLORREF c, BYTE alpha = 255)
	{
		return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
	}

	void BuildRoundedPath(Gdiplus::GraphicsPath& path, Gdiplus::REAL x, Gdiplus::REAL y,
		Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL radius)
	{
		path.Reset();
		if (w <= 0 || h <= 0) return;
		Gdiplus::REAL d = std::min(radius * 2.0f, std::min(w, h));
		if (d <= 0.5f) {
			path.AddRectangle(Gdiplus::RectF(x, y, w, h));
			return;
		}
		path.AddArc(x, y, d, d, 180.0f, 90.0f);
		path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
		path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
		path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
		path.CloseFigure();
	}

	// GDI+ 抗锯齿圆角矩形（支持卡片阴影与细描边）
	void FillRoundRectAA(Gdiplus::Graphics& g, const RECT& r, float radius,
		COLORREF fill, COLORREF border, float borderWidth = 1.0f, bool shadow = false)
	{
		Gdiplus::REAL x = (Gdiplus::REAL)r.left;
		Gdiplus::REAL y = (Gdiplus::REAL)r.top;
		Gdiplus::REAL w = (Gdiplus::REAL)(r.right - r.left) - 1.0f;
		Gdiplus::REAL h = (Gdiplus::REAL)(r.bottom - r.top) - 1.0f;
		if (w <= 1.0f || h <= 1.0f) return;

		if (shadow) {
			for (int i = 2; i >= 1; --i) {
				Gdiplus::GraphicsPath sp;
				BuildRoundedPath(sp, x, y + (Gdiplus::REAL)i, w, h, radius);
				Gdiplus::Pen spPen(Gdiplus::Color(i == 2 ? 9 : 14, 19, 35, 58), (Gdiplus::REAL)(i * 2));
				spPen.SetLineJoin(Gdiplus::LineJoinRound);
				g.DrawPath(&spPen, &sp);
			}
		}

		Gdiplus::GraphicsPath path;
		BuildRoundedPath(path, x, y, w, h, radius);
		Gdiplus::SolidBrush br(ToGpColor(fill));
		g.FillPath(&br, &path);
		if (borderWidth > 0.0f) {
			Gdiplus::Pen pen(ToGpColor(border), borderWidth);
			g.DrawPath(&pen, &path);
		}
	}

	COLORREF Blend(COLORREF from, COLORREF to, float weight)
	{
		auto mix = [weight](int a, int b) { return int(a + (b - a) * weight + 0.5f); };
		return RGB(mix(GetRValue(from), GetRValue(to)), mix(GetGValue(from), GetGValue(to)),
			mix(GetBValue(from), GetBValue(to)));
	}

	float EaseOutCubic(float t)
	{
		float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
		return 1.0f - u * u * u;
	}

	void DrawFocusRingAA(Gdiplus::Graphics& g, const RECT& r, float radius, COLORREF color)
	{
		RECT fr = r;
		InflateRect(&fr, -S(2), -S(2));
		Gdiplus::REAL x = (Gdiplus::REAL)fr.left;
		Gdiplus::REAL y = (Gdiplus::REAL)fr.top;
		Gdiplus::REAL w = (Gdiplus::REAL)(fr.right - fr.left) - 1.0f;
		Gdiplus::REAL h = (Gdiplus::REAL)(fr.bottom - fr.top) - 1.0f;
		if (w <= 2.0f || h <= 2.0f) return;
		Gdiplus::GraphicsPath path;
		BuildRoundedPath(path, x, y, w, h, std::max(1.0f, radius - (float)S(2)));
		Gdiplus::Pen pen(ToGpColor(color, 180), 1.5f);
		g.DrawPath(&pen, &path);
	}

	void DrawDropdownChevron(Gdiplus::Graphics& g, const RECT& r, COLORREF color)
	{
		Gdiplus::REAL cx = (Gdiplus::REAL)(r.right - S(14));
		Gdiplus::REAL cy = (Gdiplus::REAL)(r.top + r.bottom) * 0.5f;
		Gdiplus::REAL dx = (Gdiplus::REAL)S(4);
		Gdiplus::REAL dy = (Gdiplus::REAL)S(2);
		Gdiplus::PointF pts[3] = {
			{ cx - dx, cy - dy },
			{ cx,      cy + dy },
			{ cx + dx, cy - dy }
		};
		Gdiplus::Pen pen(ToGpColor(color), std::max(1.4f, (float)S(15) * 0.1f));
		pen.SetStartCap(Gdiplus::LineCapRound);
		pen.SetEndCap(Gdiplus::LineCapRound);
		pen.SetLineJoin(Gdiplus::LineJoinRound);
		g.DrawLines(&pen, pts, 3);
	}

	int ShowDropdownMenu(HWND owner, HWND btn, const wchar_t* const* items, int count, int current)
	{
		HMENU menu = CreatePopupMenu();
		if (!menu) return -1;
		for (int i = 0; i < count; ++i) {
			UINT flags = MF_STRING | (i == current ? MF_CHECKED : 0);
			AppendMenuW(menu, flags, (UINT_PTR)(i + 1), items[i]);
		}
		RECT wr{};
		GetWindowRect(btn, &wr);
		SetForegroundWindow(owner);
		int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
			wr.left, wr.bottom + S(3), owner, nullptr);
		PostMessageW(owner, WM_NULL, 0, 0);
		DestroyMenu(menu);
		return cmd > 0 ? cmd - 1 : -1;
	}

	void TrackOwnerButton(HWND button);
	void ReadButtonMotion(HWND button, float& hover, float& press);

	void ApplyDwmTheme(HWND hwnd)
	{
		BOOL dark = FALSE;
		DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
		COLORREF cap = theme::BG;
		COLORREF txt = theme::TEXT;
		COLORREF brd = theme::BORDER;
		DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &cap, sizeof(cap));
		DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &txt, sizeof(txt));
		DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &brd, sizeof(brd));
	}

	bool CopyTextToClipboard(HWND hwnd, const std::wstring& text)
	{
		if (text.empty() || text == L"—") return false;
		if (!OpenClipboard(hwnd)) return false;
		EmptyClipboard();
		size_t bytes = (text.size() + 1) * sizeof(wchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (hMem) {
			void* ptr = GlobalLock(hMem);
			if (ptr) {
				std::memcpy(ptr, text.c_str(), bytes);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			} else {
				GlobalFree(hMem);
				hMem = nullptr;
			}
		}
		CloseClipboard();
		return hMem != nullptr;
	}

	void TriggerCopyFeedback(HWND hwnd, int index, int btnId)
	{
		g_copy_feedback_until[index] = GetTickCount64() + 1500;
		SetText(GetDlgItem(hwnd, btnId), L"✓ 已复制");
		InvalidateRect(GetDlgItem(hwnd, btnId), nullptr, FALSE);
	}

	void AppendLog(const std::wstring& line)
	{
		if (!g_log) return;
		DWORD start = 0, end = 0;
		const int firstLine = (int)SendMessageW(g_log, EM_GETFIRSTVISIBLELINE, 0, 0);
		if (!followLog) SendMessageW(g_log, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
		int len = GetWindowTextLengthW(g_log);
		SendMessageW(g_log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		std::wstring text = line + L"\r\n";
		SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
		if (followLog) SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
		else {
			SendMessageW(g_log, EM_SETSEL, start, end);
			SendMessageW(g_log, EM_LINESCROLL, 0, firstLine - SendMessageW(g_log, EM_GETFIRSTVISIBLELINE, 0, 0));
		}
	}

	// 把 std::cout 产生的日志刷进窗口
	void DrainLogToWindow()
	{
		std::vector<std::string> lines = logging::drain();
		for (const auto& l : lines)
			AppendLog(ToWide(l));
	}

	const wchar_t* const typeNames[] = { L"选择题", L"填空题", L"信息题" };
	const wchar_t* const filterNames[] = { L"全部题型", L"选择题", L"填空题", L"信息题" };
	const wchar_t* const levelNames[] = { L"未指定", L"1 · 简单", L"2 · 中等", L"3 · 困难" };
	const char* const filePrefixes[] = { "choice_", "completion_", "info_" };
	struct Question {
		int type = 0, id = 0, level = 1;
		std::string stem, answer;
		std::vector<std::string> lines;
		bool operator==(const Question&) const = default;
	};
	template<class T> Question ReadQuestion(int type, const T& p)
	{
		Question q{type, p.id, p.complexity, p.content, p.answer};
		if constexpr (std::is_same_v<T, problem_choice>) q.lines = p.options;
		if constexpr (std::is_same_v<T, problem_info>) q.lines = p.info;
		return q;
	}
	std::vector<Question> visibleQuestions;

	std::wstring SearchText(HWND hwnd)
	{
		HWND edit = GetDlgItem(hwnd, BankSearch);
		std::wstring text(GetWindowTextLengthW(edit) + 1, L'\0');
		int length = GetWindowTextW(edit, text.data(), (int)text.size());
		text.resize(length);
		const auto first = text.find_first_not_of(L" \t\r\n　");
		if (first == std::wstring::npos) return {};
		return text.substr(first, text.find_last_not_of(L" \t\r\n　") - first + 1);
	}

	void RefreshBank(HWND hwnd, int selectType = -1, int selectId = 0)
	{
		HWND list = GetDlgItem(hwnd, BankList);
		int filter = g_bank_filter - 1;
		auto query = SearchText(hwnd);
		auto contains = [&](const std::wstring& field) {
			return !field.empty() && FindStringOrdinal(FIND_FROMSTART, field.data(), (int)field.size(),
				query.data(), (int)query.size(), TRUE) >= 0;
		};
		int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
		if (selectType < 0 && selected >= 0 && (size_t)selected < visibleQuestions.size()) {
			selectType = visibleQuestions[selected].type;
			selectId = visibleQuestions[selected].id;
		}
		int used = 0;
		bool redraw = IsWindowVisible(list) != FALSE;
		if (redraw) SendMessageW(list, WM_SETREDRAW, FALSE, 0);
		ListView_DeleteAllItems(list);
		visibleQuestions.clear();
		auto append = [&](int type, const auto& items) {
			if (filter >= 0 && type != filter) return;
			for (const auto& [p, flag] : items) {
				auto id = std::to_wstring(p.id);
				auto stem = ToWide(p.content);
				auto question = ReadQuestion(type, p);
				if (!query.empty() && id != query && !contains(stem) && !contains(ToWide(p.answer))
					&& std::none_of(question.lines.begin(), question.lines.end(),
						[&](const auto& line) { return contains(ToWide(line)); }))
					continue;
				LVITEMW item{};
				item.mask = LVIF_TEXT;
				item.pszText = id.data();
				item.iItem = (int)visibleQuestions.size();
				int row = ListView_InsertItem(list, &item);
				auto level = std::to_wstring(p.complexity);
				ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(typeNames[type]));
				ListView_SetItemText(list, row, 2, stem.data());
				ListView_SetItemText(list, row, 3, level.data());
				ListView_SetItemText(list, row, 4, const_cast<wchar_t*>(flag ? L"已用" : L"未用"));
				visibleQuestions.push_back(std::move(question));
				if (flag) ++used;
				if (type == selectType && p.id == selectId) {
					ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					ListView_EnsureVisible(list, row, FALSE);
				}
			}
		};
		{
			std::lock_guard lock(problem_bank_mutex);
			append(0, pl.choice_problems);
			append(1, pl.completion_problems);
			append(2, pl.info_problems);
		}
		if (redraw) {
			SendMessageW(list, WM_SETREDRAW, TRUE, 0);
			RedrawWindow(list, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
		}
		if (!query.empty() && visibleQuestions.empty()) {
			SetText(g_stats, L"没有匹配的题目，请更换关键词或题型。");
			return;
		}
		SetText(g_stats, (query.empty() ? L"共 " : L"找到 ") + std::to_wstring(visibleQuestions.size()) + L" 题    ·    未用 "
			+ std::to_wstring(visibleQuestions.size() - used) + L"    ·    已用 " + std::to_wstring(used));
	}

	void ReloadProblemList()
	{
		pl.choice_problems.clear();
		pl.completion_problems.clear();
		pl.info_problems.clear();
		pl.load(PROBLEM_LIST_FILE);
		pl_unused = pl;
		pl_unused.remove_used();
	}

	// 备份题库到 data/problems_backup/
	void BackupProblems()
	{
		RemoveDir(BACKUP_DIR);
		CreateDirectoryA("data", NULL);
		CreateDirectoryA(BACKUP_DIR, NULL);
		CopyDir(PROBLEM_DIR, BACKUP_DIR);
	}

	// Prepare both memory pools before publishing files. For new questions the
	// index is committed last, so readers never see an entry without its body.
	template<class T>
	int SaveQuestion(const Question& draft, const Question& original,
		std::vector<std::pair<T, int>> problem_list::* member)
	{
		std::lock_guard lock(problem_bank_mutex);
		problem_list next = pl;
		auto& items = next.*member;
		bool adding = original.id == 0;
		int id = adding ? (int)items.size() + 1 : original.id;
		if (adding) {
			for (size_t i = 0; i < items.size(); ++i)
				if (items[i].first.id != i + 1) throw std::runtime_error("当前题库编号不连续，请先检查题库。");
			items.emplace_back(T{}, 0);
		} else if (id < 1 || (size_t)id > items.size() || ReadQuestion(draft.type, items[id-1].first) != original) {
			throw std::runtime_error("这道题已被其他操作更改。请取消并重新打开题目。");
		}
		T& p = items[id-1].first;
		p.id = id;
		p.content = draft.stem;
		p.answer = draft.answer;
		p.complexity = draft.level;
		if constexpr (std::is_same_v<T, problem_choice>) p.options = draft.lines;
		if constexpr (std::is_same_v<T, problem_info>) p.info = draft.lines;
		problem_list unused = next;
		unused.remove_used();

		std::string file = PROBLEM_DIR + std::string(filePrefixes[draft.type]) + std::to_string(id) + ".json";
		std::string temp = file + ".tmp", indexTemp = std::string(PROBLEM_LIST_FILE) + ".tmp";
		BackupProblems();
		bool published = false;
		try {
			p.save(temp);
			if (adding) next.save(indexTemp);
			if (!MoveFileExA(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				throw std::runtime_error("无法写入题目文件，请检查题库目录权限。");
			published = true;
			if (adding && !MoveFileExA(indexTemp.c_str(), PROBLEM_LIST_FILE, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				throw std::runtime_error("无法更新题目索引，请检查目录权限后重试。");
		} catch (...) {
			DeleteFileA(temp.c_str());
			DeleteFileA(indexTemp.c_str());
			if (adding && published) DeleteFileA(file.c_str());
			throw;
		}
		pl = std::move(next);
		pl_unused = std::move(unused);
		return id;
	}

	struct QuestionForm {
		Question original;
		int type = 0, level = 1;
		int savedType = -1, savedId = 0;
		int focusedEdit = 0;
		bool layoutCaptured = false;
		int fullHeight = 0;
		int compactDelta = 0;
		RECT rcAnsLabel{};
		RECT rcAnswer{};
		RECT rcFeedback{};
		RECT rcOk{};
		RECT rcCancel{};
		RECT rcBoxStem{};
		RECT rcBoxLine[4]{};
		RECT rcBoxAnswer{};
	};

	void ApplyQuestionLayout(HWND dlg, QuestionForm& form, int type)
	{
		bool compact = (type == 1);
		SetDlgItemTextW(dlg, QE_EXTRA_LABEL, type == 0 ? L"四个选项（依次为 A、B、C、D）" : L"四条线索（按展示顺序）");
		const wchar_t* prefixes[4] = {
			type == 0 ? L"A." : L"线索 1",
			type == 0 ? L"B." : L"线索 2",
			type == 0 ? L"C." : L"线索 3",
			type == 0 ? L"D." : L"线索 4"
		};
		for (int i = 0; i < 4; ++i)
			SetDlgItemTextW(dlg, QE_PREFIX1 + i, prefixes[i]);

		for (int id : { QE_EXTRA_LABEL, QE_PREFIX1, QE_PREFIX2, QE_PREFIX3, QE_PREFIX4,
			QE_LINE1, QE_LINE2, QE_LINE3, QE_LINE4 })
			ShowWindow(GetDlgItem(dlg, id), compact ? SW_HIDE : SW_SHOWNA);

		if (form.layoutCaptured && form.compactDelta > 0) {
			int shift = compact ? -form.compactDelta : 0;
			auto moveRel = [&](int id, const RECT& base) {
				HWND c = GetDlgItem(dlg, id);
				if (c) MoveWindow(c, base.left, base.top + shift, base.right - base.left, base.bottom - base.top, TRUE);
			};
			moveRel(QE_ANS_LABEL, form.rcAnsLabel);
			moveRel(QE_ANSWER, form.rcAnswer);
			moveRel(QE_FEEDBACK, form.rcFeedback);
			moveRel(IDOK, form.rcOk);
			moveRel(IDCANCEL, form.rcCancel);

			RECT wr{};
			GetWindowRect(dlg, &wr);
			SetWindowPos(dlg, nullptr, 0, 0, wr.right - wr.left, form.fullHeight + shift,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
	}

	void DrawDialogButton(LPDRAWITEMSTRUCT dis)
	{
		HDC dc = dis->hDC;
		RECT r = dis->rcItem;
		int w = r.right - r.left, h = r.bottom - r.top;
		if (w <= 0 || h <= 0) return;

		HDC memDC = CreateCompatibleDC(dc);
		HBITMAP memBmp = CreateCompatibleBitmap(dc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		HBRUSH bgBr = CreateSolidBrush(theme::BG);
		RECT local = { 0, 0, w, h };
		FillRect(memDC, &local, bgBr);
		DeleteObject(bgBr);

		bool dropdown = (dis->CtlID == QE_TYPE || dis->CtlID == QE_LEVEL);
		bool primary = (dis->CtlID == IDOK);
		bool disabled = (dis->itemState & ODS_DISABLED) != 0;
		bool keyboardFocus = (dis->itemState & ODS_FOCUS) != 0 && (dis->itemState & ODS_NOFOCUSRECT) == 0;
		float rawHot = 0.0f, rawPress = 0.0f;
		ReadButtonMotion(dis->hwndItem, rawHot, rawPress);
		float hot = disabled ? 0.0f : EaseOutCubic(rawHot);
		float press = disabled ? 0.0f : EaseOutCubic(rawPress);
		if (!disabled && (dis->itemState & ODS_SELECTED)) press = std::max(press, 0.85f);

		COLORREF bg, fg, border;
		if (dropdown) {
			bg = disabled ? theme::FIELD
				: Blend(Blend(theme::CARD, theme::ACCENT_SOFT, hot * 0.55f), RGB(219, 232, 252), press);
			fg = disabled ? theme::TEXT_SUB : theme::TEXT;
			border = disabled ? theme::BORDER
				: Blend(theme::BORDER, theme::ACCENT, std::max(hot * 0.65f, keyboardFocus ? 1.0f : 0.0f));
		} else if (primary) {
			bg = Blend(Blend(theme::ACCENT, theme::ACCENT_HOT, hot), theme::ACCENT_DN, press);
			fg = RGB(255, 255, 255);
			border = bg;
		} else {
			bg = Blend(Blend(theme::CARD, theme::ACCENT_SOFT, hot), RGB(212, 227, 252), press);
			fg = Blend(theme::TEXT, theme::ACCENT_DN, hot);
			border = Blend(theme::BORDER, theme::ACCENT_BORDER, std::max(hot, press));
		}

		RECT rr = local;
		InflateRect(&rr, -1, -1);
		const float radius = (float)S(8);
		{
			Gdiplus::Graphics g(memDC);
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
			FillRoundRectAA(g, rr, radius, bg, border, 1.0f, false);
			if (dropdown) {
				COLORREF chev = disabled ? theme::TEXT_MUTED : Blend(theme::TEXT_SUB, theme::ACCENT, hot);
				DrawDropdownChevron(g, rr, chev);
			}
			if (keyboardFocus)
				DrawFocusRingAA(g, rr, radius, primary ? RGB(255, 255, 255) : theme::ACCENT);
		}

		wchar_t text[64] = { 0 };
		GetWindowTextW(dis->hwndItem, text, 64);
		SetBkMode(memDC, TRANSPARENT);
		SetTextColor(memDC, fg);
		HFONT oldF = (HFONT)SelectObject(memDC, g_font_ui ? g_font_ui : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
		if (dropdown) {
			RECT textBox = rr;
			textBox.left += S(10);
			textBox.right -= S(24);
			DrawTextW(memDC, text, -1, &textBox, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		} else {
			DrawTextW(memDC, text, -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		SelectObject(memDC, oldF);

		BitBlt(dc, r.left, r.top, w, h, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBmp);
		DeleteObject(memBmp);
		DeleteDC(memDC);
	}

	INT_PTR CALLBACK QuestionDialog(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg) {
		case WM_INITDIALOG: {
			auto* form = reinterpret_cast<QuestionForm*>(lp);
			SetWindowLongPtrW(dlg, DWLP_USER, lp);
			ApplyDwmTheme(dlg);
			HICON hSm = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_RESPONDER), IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
			if (hSm) SendMessageW(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hSm);

			const auto& q = form->original;
			form->type = std::clamp(q.type, 0, 2);
			form->level = std::clamp(q.level, 0, 3);
			std::wstring title = q.id ? std::wstring(L"编辑") + typeNames[form->type] + L" · #" + std::to_wstring(q.id) : L"新增题目";
			SetWindowTextW(dlg, title.c_str());
			SetDlgItemTextW(dlg, QE_BADGE, q.id ? (std::wstring(typeNames[form->type]) + L" · #" + std::to_wstring(q.id)).c_str() : L"新题录入");

			SetDlgItemTextW(dlg, QE_TYPE, typeNames[form->type]);
			SetDlgItemTextW(dlg, QE_LEVEL, levelNames[form->level]);
			EnableWindow(GetDlgItem(dlg, QE_TYPE), !q.id);
			for (int cid : { QE_TYPE, QE_LEVEL, IDOK, IDCANCEL })
				TrackOwnerButton(GetDlgItem(dlg, cid));

			SetDlgItemTextW(dlg, QE_STEM, ToWide(q.stem).c_str());
			SetDlgItemTextW(dlg, QE_ANSWER, ToWide(q.answer).c_str());
			for (size_t i = 0; i < std::min<size_t>(4, q.lines.size()); ++i)
				SetDlgItemTextW(dlg, QE_LINE1 + (int)i, ToWide(q.lines[i]).c_str());

			auto childRect = [&](int id) {
				RECT rc{};
				GetWindowRect(GetDlgItem(dlg, id), &rc);
				MapWindowPoints(nullptr, dlg, (LPPOINT)&rc, 2);
				return rc;
			};
			auto inflateBox = [&](int id, int padX, int padY) {
				RECT rc = childRect(id);
				InflateRect(&rc, S(padX), S(padY));
				return rc;
			};
			RECT wr{};
			GetWindowRect(dlg, &wr);
			form->fullHeight = wr.bottom - wr.top;
			RECT extraRc = childRect(QE_EXTRA_LABEL);
			form->rcAnsLabel = childRect(QE_ANS_LABEL);
			form->rcAnswer = childRect(QE_ANSWER);
			form->rcFeedback = childRect(QE_FEEDBACK);
			form->rcOk = childRect(IDOK);
			form->rcCancel = childRect(IDCANCEL);
			form->rcBoxStem = inflateBox(QE_STEM, 6, 4);
			for (int i = 0; i < 4; ++i)
				form->rcBoxLine[i] = inflateBox(QE_LINE1 + i, 6, 5);
			form->rcBoxAnswer = inflateBox(QE_ANSWER, 6, 4);
			form->compactDelta = form->rcAnsLabel.top - extraRc.top;
			form->layoutCaptured = (form->compactDelta > 0);

			ApplyQuestionLayout(dlg, *form, form->type);
			SetFocus(GetDlgItem(dlg, q.id ? QE_STEM : QE_TYPE));
			return FALSE;
		}
		case WM_ERASEBKGND:
			return TRUE;
		case WM_PAINT: {
			auto* form = reinterpret_cast<QuestionForm*>(GetWindowLongPtrW(dlg, DWLP_USER));
			PAINTSTRUCT ps{};
			HDC dc = BeginPaint(dlg, &ps);
			RECT rc{};
			GetClientRect(dlg, &rc);
			int w = rc.right - rc.left, h = rc.bottom - rc.top;
			if (w > 0 && h > 0) {
				HDC memDC = CreateCompatibleDC(dc);
				HBITMAP memBmp = CreateCompatibleBitmap(dc, w, h);
				HGDIOBJ oldBmp = SelectObject(memDC, memBmp);
				HBRUSH bgBr = CreateSolidBrush(theme::BG);
				FillRect(memDC, &rc, bgBr);
				DeleteObject(bgBr);

				if (form) {
					Gdiplus::Graphics g(memDC);
					g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
					g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
					auto paintBox = [&](int id, const RECT& box) {
						bool focused = (form->focusedEdit == id);
						FillRoundRectAA(g, box, (float)S(8), theme::CARD,
							focused ? theme::ACCENT : theme::BORDER, focused ? 1.5f : 1.0f, false);
					};
					paintBox(QE_STEM, form->rcBoxStem);
					bool compact = (form->type == 1);
					if (!compact) {
						for (int i = 0; i < 4; ++i)
							paintBox(QE_LINE1 + i, form->rcBoxLine[i]);
					}
					RECT ansBox = form->rcBoxAnswer;
					if (compact && form->layoutCaptured && form->compactDelta > 0)
						OffsetRect(&ansBox, 0, -form->compactDelta);
					paintBox(QE_ANSWER, ansBox);
				}

				BitBlt(dc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
				SelectObject(memDC, oldBmp);
				DeleteObject(memBmp);
				DeleteDC(memDC);
			}
			EndPaint(dlg, &ps);
			return TRUE;
		}
		case WM_SETCURSOR: {
			HWND target = (HWND)wp;
			if (target && IsWindowEnabled(target)) {
				wchar_t cls[32] = { 0 };
				GetClassNameW(target, cls, 32);
				if (_wcsicmp(cls, L"Button") == 0) {
					SetCursor(LoadCursorW(nullptr, IDC_HAND));
					SetWindowLongPtrW(dlg, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}
			break;
		}
		case WM_CTLCOLORDLG: {
			static HBRUSH dlgBg = CreateSolidBrush(theme::BG);
			return (INT_PTR)dlgBg;
		}
		case WM_CTLCOLORSTATIC: {
			HDC dc = (HDC)wp;
			HWND ctl = (HWND)lp;
			int cid = GetDlgCtrlID(ctl);
			SetBkMode(dc, TRANSPARENT);
			if (cid == QE_FEEDBACK) SetTextColor(dc, theme::WARN);
			else if (cid == QE_BADGE) SetTextColor(dc, theme::ACCENT);
			else if (cid >= QE_PREFIX1 && cid <= QE_PREFIX4) SetTextColor(dc, theme::ACCENT_DN);
			else if (cid == QE_STEM_LABEL || cid == QE_EXTRA_LABEL || cid == QE_ANS_LABEL) SetTextColor(dc, theme::TEXT);
			else SetTextColor(dc, theme::TEXT_SUB);
			static HBRUSH dlgStaticBg = CreateSolidBrush(theme::BG);
			return (INT_PTR)dlgStaticBg;
		}
		case WM_CTLCOLOREDIT: {
			HDC dc = (HDC)wp;
			SetBkMode(dc, OPAQUE);
			SetBkColor(dc, theme::CARD);
			SetTextColor(dc, theme::TEXT);
			static HBRUSH dlgEditBg = CreateSolidBrush(theme::CARD);
			return (INT_PTR)dlgEditBg;
		}
		case WM_DRAWITEM:
			DrawDialogButton((LPDRAWITEMSTRUCT)lp);
			return TRUE;
		case WM_COMMAND: {
			auto* form = reinterpret_cast<QuestionForm*>(GetWindowLongPtrW(dlg, DWLP_USER));
			if (!form) return FALSE;
			int cid = LOWORD(wp);
			if ((cid == QE_STEM || cid == QE_ANSWER || (cid >= QE_LINE1 && cid <= QE_LINE4))
				&& (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS)) {
				form->focusedEdit = (HIWORD(wp) == EN_SETFOCUS) ? cid : 0;
				InvalidateRect(dlg, nullptr, FALSE);
				return TRUE;
			}
			switch (cid) {
			case QE_TYPE:
				if (HIWORD(wp) == BN_CLICKED && !form->original.id) {
					int picked = ShowDropdownMenu(dlg, (HWND)lp, typeNames, 3, form->type);
					if (picked >= 0 && picked != form->type) {
						form->type = picked;
						SetDlgItemTextW(dlg, QE_TYPE, typeNames[form->type]);
						ApplyQuestionLayout(dlg, *form, form->type);
					}
				}
				return TRUE;
			case QE_LEVEL:
				if (HIWORD(wp) == BN_CLICKED) {
					int picked = ShowDropdownMenu(dlg, (HWND)lp, levelNames, 4, form->level);
					if (picked >= 0 && picked != form->level) {
						form->level = picked;
						SetDlgItemTextW(dlg, QE_LEVEL, levelNames[form->level]);
						InvalidateRect((HWND)lp, nullptr, FALSE);
					}
				}
				return TRUE;
			case IDCANCEL:
				EndDialog(dlg, IDCANCEL);
				return TRUE;
			case IDOK:
				try {
					auto text = [&](int id) {
						HWND field = GetDlgItem(dlg, id);
						std::wstring s(GetWindowTextLengthW(field) + 1, L'\0');
						auto n = GetWindowTextW(field, s.data(), (int)s.size());
						s.resize(n);
						return ToUtf8(s);
					};
					auto required = [&](int id, const char* error) {
						auto s = text(id);
						if (s.find_first_not_of(" \t\r\n") == std::string::npos) {
							SetFocus(GetDlgItem(dlg, id));
							throw std::runtime_error(error);
						}
						return s;
					};
					int type = form->type;
					Question q;
					q.type = type;
					q.level = form->level;
					if (q.level < 0) throw std::runtime_error("请选择难度。");
					q.stem = required(QE_STEM, "请输入题干。");
					if (type != 1)
						for (int i = 0; i < 4; ++i)
							q.lines.push_back(required(QE_LINE1 + i, type == 0 ? "请填写全部四个选项。" : "请填写全部四条线索。"));
					q.answer = required(QE_ANSWER, "请输入答案。");
					// Imported choice banks may use the answer text rather than a letter.
					if (type == 0 && q.answer.size() == 1 && q.answer[0] >= 'a' && q.answer[0] <= 'd')
						q.answer[0] -= 'a' - 'A';
					if (type == 0) form->savedId = SaveQuestion(q, form->original, &problem_list::choice_problems);
					else if (type == 1) form->savedId = SaveQuestion(q, form->original, &problem_list::completion_problems);
					else form->savedId = SaveQuestion(q, form->original, &problem_list::info_problems);
					form->savedType = type;
					EndDialog(dlg, IDOK);
				} catch (const std::exception& ex) {
					SetDlgItemTextW(dlg, QE_FEEDBACK, ToWide(ex.what()).c_str());
				}
				return TRUE;
			}
			break;
		}
		}
		return FALSE;
	}

	void OpenQuestion(HWND hwnd, bool adding)
	{
		if (g_op_running) return;
		QuestionForm form;
		if (adding) {
			form.original.type = std::max(0, g_bank_filter - 1);
		} else {
			int row = ListView_GetNextItem(GetDlgItem(hwnd, BankList), -1, LVNI_SELECTED);
			if (row < 0 || (size_t)row >= visibleQuestions.size()) return;
			form.original = visibleQuestions[row];
		}
		auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_QUESTION), hwnd, QuestionDialog, (LPARAM)&form);
		if (result == -1) MessageBoxW(hwnd, L"无法打开题目编辑窗口。", L"题库管理", MB_ICONERROR);
		if (result == IDOK) {
			if (g_bank_filter > 0 && g_bank_filter != form.savedType + 1) {
				g_bank_filter = form.savedType + 1;
				SetDlgItemTextW(hwnd, BankFilter, filterNames[g_bank_filter]);
			}
			AppendLog(adding ? L"已新增题目。" : L"已保存题目修改。");
		}
		RefreshBank(hwnd, result == IDOK ? form.savedType : form.original.type,
			result == IDOK ? form.savedId : form.original.id);
	}

	void SetOperationButtonsEnabled(BOOL enabled)
	{
		EnableWindow(g_btn_convert, enabled);
		EnableWindow(g_btn_merge, enabled);
		EnableWindow(g_btn_dedup, enabled);
		EnableWindow(g_btn_backup, enabled);
		HWND hwnd = GetParent(g_btn_backup);
		EnableWindow(GetDlgItem(hwnd, BankAdd), enabled);
		EnableWindow(GetDlgItem(hwnd, BankFilter), enabled);
		EnableWindow(GetDlgItem(hwnd, BankSearch), enabled);
		EnableWindow(GetDlgItem(hwnd, BankClearSearch), enabled);
		EnableWindow(GetDlgItem(hwnd, BankEdit), enabled &&
			ListView_GetNextItem(GetDlgItem(hwnd, BankList), -1, LVNI_SELECTED) >= 0);
	}

	// 弹出选择 txt 的对话框；返回空串表示取消
	std::string PickTextFile(HWND owner)
	{
		wchar_t buf[MAX_PATH] = { 0 };
		OPENFILENAMEW ofn = { 0 };
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = owner;
		ofn.lpstrFile = buf;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = L"题库文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (!GetOpenFileNameW(&ofn))
			return "";
		return ToUtf8(buf);
	}

	// 把选中的 txt 复制到 data/upload_tmp/upload.txt
	bool StageUploadFile(const std::string& src)
	{
		CreateDirectoryA("data", NULL);
		CreateDirectoryA("data/upload_tmp", NULL);

		std::ifstream in(src, std::ios::binary);
		if (!in) return false;
		std::ofstream out(UPLOAD_FILE, std::ios::binary);
		if (!out) return false;
		out << in.rdbuf();
		return true;
	}

	// 在后台线程执行题库操作，完成后通过 g_op_running 通知 GUI
	void RunProblemOperation(int op)
	{
		std::lock_guard bank_lock(problem_bank_mutex);

		try
		{
			if (op == IDC_BTN_BACKUP)
			{
				std::cout << "=== 操作：备份当前题库 ===" << std::endl;
				BackupProblems();
				std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;
			}
			else if (op == IDC_BTN_DEDUP)
			{
				std::cout << "=== 操作：去除重复题并重排 ID ===" << std::endl;
				BackupProblems();
				std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;

				responder_problem_output_handler handler;
				handler.load(PROBLEM_LIST_FILE);

				int before = (int)(handler.choice_problems.size() + handler.completion_problems.size() + handler.info_problems.size());
				handler.delete_duplicates();
				int after = (int)(handler.choice_problems.size() + handler.completion_problems.size() + handler.info_problems.size());

				RemoveDir(PROBLEM_DIR);
				CreateDirectoryA(PROBLEM_DIR, NULL);
				handler.output_to_folder(PROBLEM_DIR);

				ReloadProblemList();
				std::cout << "共删除 " << (before - after) << " 道重复题目（操作前 "
					<< before << " 道，操作后 " << after << " 道）。" << std::endl;
			}
			else if (op == IDC_BTN_CONVERT)
			{
				std::cout << "=== 操作：转换题库（覆盖现有题库）===" << std::endl;
				BackupProblems();
				std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;

				problem_list::convert(UPLOAD_FILE, PROBLEM_DIR);
				ReloadProblemList();
			}
			else if (op == IDC_BTN_MERGE)
			{
				std::cout << "=== 操作：合并题库（保留现有题目）===" << std::endl;
				BackupProblems();
				std::cout << "已将当前题库备份到 " << BACKUP_DIR << std::endl;

				problem_list::merge(UPLOAD_FILE, PROBLEM_DIR);
				ReloadProblemList();
			}
		}
		catch (const std::exception& e)
		{
			std::cout << "发生异常：" << e.what() << std::endl;
		}
		catch (...)
		{
			std::cout << "发生未知异常。" << std::endl;
		}

		remove(UPLOAD_FILE);
		std::cout << "=== 操作结束 ===" << std::endl;
		g_op_running = false;
	}

	void ApplyServerState(HWND hwnd)
	{
		bool running = server_core::is_running();
		EnableWindow(GetDlgItem(hwnd, MatchOpen), running);

		int nextKind = 0;
		std::wstring nextStatusText = L"服务未启动";
		if (running)
		{
			nextKind = 1;
			nextStatusText = L"服务运行中";
			EnableWindow(g_btn_start, FALSE);
			EnableWindow(g_btn_stop, TRUE);
			EnableWindow(g_btn_browser, TRUE);

			std::string ip = get_first_ip();
			std::wstring localUrl = L"http://127.0.0.1:" + std::to_wstring(server_core::PORT);
			std::wstring lanUrl = ip.empty()
				? L"未检测到局域网地址"
				: L"http://" + ToWide(ip) + L":" + std::to_wstring(server_core::PORT);
			if (GetText(g_url_local) != localUrl) SetText(g_url_local, localUrl);
			if (GetText(g_url_lan) != lanUrl) SetText(g_url_lan, lanUrl);
			SetText(g_hint, L"前端电脑可通过局域网地址和登录 PIN 进入比赛。");

			EnableWindow(g_btn_copy_local, TRUE);
			EnableWindow(g_btn_copy_lan, !ip.empty());
			EnableWindow(g_btn_copy_pin, !authentic_pin.empty());

			if (g_last_shown_pin != authentic_pin)
			{
				g_last_shown_pin = authentic_pin;
				SetText(g_pin, ToWide(g_last_shown_pin));
			}
		}
		else
		{
			EnableWindow(g_btn_start, TRUE);
			EnableWindow(g_btn_stop, FALSE);
			EnableWindow(g_btn_browser, FALSE);
			EnableWindow(g_btn_copy_local, FALSE);
			EnableWindow(g_btn_copy_lan, FALSE);
			EnableWindow(g_btn_copy_pin, FALSE);
			if (GetText(g_url_local) != L"—") SetText(g_url_local, L"—");
			if (GetText(g_url_lan) != L"—") SetText(g_url_lan, L"—");
			if (GetText(g_pin) != L"—") SetText(g_pin, L"—");
			g_last_shown_pin.clear();

			std::string err = server_core::last_error();
			if (!err.empty()) {
				nextKind = 3;
				nextStatusText = L"启动失败";
				SetText(g_hint, ToWide(err));
			} else if (g_stopping) {
				nextKind = 2;
				nextStatusText = L"正在下线…";
				SetText(g_hint, L"正在下线…");
			} else if (g_starting) {
				nextKind = 2;
				nextStatusText = L"正在上线…";
				SetText(g_hint, L"正在上线…");
			} else {
				nextKind = 0;
				nextStatusText = L"服务未启动";
				SetText(g_hint, L"点击「上线」后，前端电脑即可连接。");
			}
		}

		if (g_status_kind != nextKind || g_status_text != nextStatusText) {
			g_status_kind = nextKind;
			g_status_text = nextStatusText;
			SetText(g_status, nextStatusText);
			InvalidateRect(g_status, nullptr, FALSE);
		}

		const ULONGLONG now = GetTickCount64();
		const int copyIds[3] = { IDC_BTN_COPY_LOCAL, IDC_BTN_COPY_LAN, IDC_BTN_COPY_PIN };
		const wchar_t* copyLabels[3] = { L"复制", L"复制地址", L"复制 PIN" };
		for (int i = 0; i < 3; ++i) {
			if (g_copy_feedback_until[i] && now >= g_copy_feedback_until[i]) {
				g_copy_feedback_until[i] = 0;
				HWND btn = GetDlgItem(hwnd, copyIds[i]);
				SetText(btn, copyLabels[i]);
				InvalidateRect(btn, nullptr, FALSE);
			}
		}
	}

	void DoStart(HWND hwnd)
	{
		if (server_core::is_running() || g_starting)
			return;

		g_starting = true;
		g_stopping = false;
		SetText(g_hint, L"正在上线…");
		AppendLog(L"=== 正在启动服务 ===");

		std::thread([]() {
			server_core::start();
			g_starting = false;
		}).detach();
	}

	void DoStop()
	{
		if (g_stopping) return;
		g_stopping = true;
		AppendLog(L"=== 正在停止服务 ===");
		std::thread([]() {
			server_core::stop();
			g_stopping = false;
		}).detach();
	}

	void RefreshMatches(HWND hwnd)
	{
		HWND list = GetDlgItem(hwnd, MatchList);
		ListView_DeleteAllItems(list);
		int count = 0, failed = 0;
		try {
			for (const auto& guid : load_competition_list("data/competitions/list.json")) {
				std::ifstream input("data/competitions/" + guid + "/competition.json");
				auto record = json::parse(input, nullptr, false);
				if (!record.is_object() || !record.contains("name") || !record["name"].is_string()) {
					++failed;
					continue;
				}
				auto name = ToWide(record["name"].get<std::string>());
				LVITEMW item{};
				item.mask = LVIF_TEXT;
				item.iItem = count;
				item.pszText = name.data();
				int row = ListView_InsertItem(list, &item);
				const wchar_t* states[] = { L"未开始", L"进行中", L"已暂停", L"已结束" };
				int state = record["competition_status"].is_number_integer() ? record["competition_status"].get<int>() : -1;
				auto teams = record["teams"].is_array() ? std::to_wstring(record["teams"].size()) : L"—";
				ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(state >= 0 && state < 4 ? states[state] : L"未知"));
				ListView_SetItemText(list, row, 2, teams.data());
				++count;
			}
		} catch (const json::exception&) { ++failed; }
		std::wstring summary = count ? L"共 " + std::to_wstring(count) + L" 场比赛" : L"暂无比赛记录";
		if (failed) summary += L"   ·   部分比赛数据无法读取";
		SetText(GetDlgItem(hwnd, MatchHint), summary);
	}

	void LayoutControls(HWND hwnd)
	{
		RECT rc{};
		GetClientRect(hwnd, &rc);
		const int w = MulDiv(rc.right, 96, S(96)), h = MulDiv(rc.bottom, 96, S(96));
		const int left = 198, panelW = w - left - 24, inner = panelW - 40;
		const int x = left + 20, rightEdge = x + inner;
		auto place = [&](int id, int px, int py, int width, int height, bool visible = true) {
			HWND ctl = GetDlgItem(hwnd, id);
			if (visible) MoveWindow(ctl, S(px), S(py), S(width), S(height), TRUE);
			ShowWindow(ctl, visible ? SW_SHOWNA : SW_HIDE);
		};
		place(Brand, 22, 22, 134, 34);
		place(BrandCaption, 23, 58, 134, 22);
		for (int i = 0; i < 4; ++i) place(NavService + i, 12, 108 + 50 * i, 150, 42);
		place(IDC_TITLE, left, 18, 220, 38);
		place(IDC_STATUS, w - 168, 22, 144, 32);

		const bool service = currentPage == NavService, problems = currentPage == NavProblems;
		const bool matches = currentPage == NavMatches, settings = currentPage == NavSettings;
		int panelBottom = matches || problems ? h - 24 : service ? 324 : 216;
		g_panel = { S(left), S(74), S(w - 24), S(panelBottom) };

		// 服务管理卡片
		place(IDC_STATIC_SVC, x, 96, 120, 26, service);
		place(IDC_BTN_BROWSER, rightEdge - 114, 90, 114, 34, service);
		place(IDC_BTN_STOP, rightEdge - 196, 90, 74, 34, service);
		place(IDC_BTN_START, rightEdge - 278, 90, 74, 34, service);

		place(IDC_STATIC_URL1, x, 145, 84, 24, service);
		g_box_url_local = { S(x + 90), S(141), S(rightEdge - 98), S(173) };
		place(IDC_URL_LOCAL, x + 100, 147, std::max(60, inner - 90 - 98 - 18), 20, service);
		place(IDC_BTN_COPY_LOCAL, rightEdge - 90, 141, 90, 32, service);

		place(IDC_STATIC_URL2, x, 185, 84, 24, service);
		g_box_url_lan = { S(x + 90), S(181), S(rightEdge - 98), S(213) };
		place(IDC_URL_LAN, x + 100, 187, std::max(60, inner - 90 - 98 - 18), 20, service);
		place(IDC_BTN_COPY_LAN, rightEdge - 90, 181, 90, 32, service);

		place(IDC_STATIC_PIN, x, 231, 84, 28, service);
		g_box_pin = { S(x + 90), S(224), S(x + 244), S(266) };
		place(IDC_LOGIN_PIN, x + 98, 230, 138, 29, service);
		place(IDC_BTN_COPY_PIN, rightEdge - 90, 229, 90, 32, service);
		place(IDC_HINT, x, 282, inner, 24, service);

		// 题库管理卡片
		place(IDC_STATIC_PB1, x, 96, 110, 26, problems);
		place(BankSearchLabel, x, 143, 0, 0, false);
		g_box_search = { S(x), S(138), S(x + inner - 184), S(170) };
		place(BankSearch, x + 32, 144, std::max(60, inner - 184 - 40), 20, problems);
		place(BankClearSearch, x + inner - 176, 138, 58, 32, problems);
		place(BankFilter, x + inner - 110, 138, 110, 32, problems);
		place(BankEdit, rightEdge - 208, 90, 96, 34, problems);
		place(BankAdd, rightEdge - 104, 90, 104, 34, problems);
		g_box_bank_list = { S(x), S(180), S(x + inner), S(180 + std::max(46, h - 338)) };
		place(BankList, x + 3, 183, std::max(60, inner - 6), std::max(40, h - 344), problems);
		if (problems) {
			HWND list = GetDlgItem(hwnd, BankList);
			const int widths[] = { 48, 70, std::max(100, inner - 254), 48 };
			for (int i = 0; i < 4; ++i) ListView_SetColumnWidth(list, i, S(widths[i]));
			ListView_SetColumnWidth(list, 4, LVSCW_AUTOSIZE_USEHEADER);
		}
		place(IDC_STATS, x, h - 150, inner, 24, problems);
		for (int i = 0; i < 4; ++i)
			place(IDC_BTN_CONVERT + i, x + i * (inner + 8) / 4, h - 114, (inner - 24) / 4, 34, problems);
		place(IDC_STATIC_PB2, x, h - 71, inner, 38, problems);

		// 日志卡片
		const int logY = panelBottom + 14;
		g_card_log = { S(left), S(logY), S(w - 24), S(h - 18) };
		g_box_log = { S(x), S(logY + 42), S(rightEdge), S(h - 32) };
		place(IDC_STATIC_LOG, x, logY + 12, inner, 24, service);
		place(IDC_LOG, x + 10, logY + 49, std::max(40, inner - 20), std::max(16, h - logY - 88), service);

		// 比赛管理卡片
		place(MatchTitle, x, 96, 150, 26, matches);
		place(MatchOpen, rightEdge - 216, 90, 122, 34, matches);
		place(MatchRefresh, rightEdge - 86, 90, 86, 34, matches);
		g_box_match_list = { S(x), S(142), S(x + inner), S(142 + std::max(46, h - 228)) };
		place(MatchList, x + 3, 145, std::max(60, inner - 6), std::max(40, h - 234), matches);
		if (matches) {
			HWND list = GetDlgItem(hwnd, MatchList);
			ListView_SetColumnWidth(list, 0, S(std::max(120, inner - 210)));
			ListView_SetColumnWidth(list, 1, S(106));
			ListView_SetColumnWidth(list, 2, LVSCW_AUTOSIZE_USEHEADER);
		}
		place(MatchHint, x, h - 65, inner, 24, matches);

		// 设置卡片
		place(SettingsTitle, x, 96, inner, 26, settings);
		place(AutoScroll, x, 138, inner, 28, settings);
		place(SettingsHint, x, 178, inner, 24, settings);
	}
}

// 说明：为便于布局，这里补充了几个纯标签控件的 ID
namespace
{
	// 建立字体
	void CreateFonts()
	{
		LOGFONTW lf = { 0 };
		lf.lfCharSet = DEFAULT_CHARSET;
		lf.lfQuality = CLEARTYPE_QUALITY;

		auto make = [&](int h, int weight, const wchar_t* face) -> HFONT {
			lf.lfHeight = -S(-h);
			lf.lfWeight = weight;
			wcscpy_s(lf.lfFaceName, face);
			return CreateFontIndirectW(&lf);
		};

		iconFont = make(-17, FW_NORMAL, L"Segoe MDL2 Assets");
		g_font_ui    = make(-14, FW_NORMAL, L"Microsoft YaHei UI");
		g_font_small = make(-12, FW_NORMAL, L"Microsoft YaHei UI");
		g_font_sect  = make(-16, FW_SEMIBOLD, L"Microsoft YaHei UI");
		g_font_title = make(-22, FW_BOLD, L"Microsoft YaHei UI");
		g_font_stat  = make(-14, FW_SEMIBOLD, L"Microsoft YaHei UI");
		g_font_pin   = make(-24, FW_BOLD, L"Consolas");
		g_font_mono  = make(-13, FW_NORMAL, L"Consolas");
	}

	struct ButtonMotion {
		float hover = 0, press = 0;
		bool inside = false, down = false;
		ULONGLONG lastFrame = 0;
	};
	std::map<HWND, ButtonMotion> motions;
	constexpr UINT_PTR motionTimer = 0x5354;

	void AnimateButton(HWND button, ButtonMotion& motion, bool inside, bool down)
	{
		if (motion.inside == inside && motion.down == down) return;
		motion.inside = inside;
		motion.down = down;
		BOOL animate = TRUE;
		SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
		if (animate && SetTimer(button, motionTimer, 10, nullptr)) {
			// 即使两次输入落在同一帧内，轻点也有可见的按压反馈。
			if (down) motion.press = std::max(motion.press, 0.55f);
			motion.lastFrame = GetTickCount64();
		} else {
			motion.hover = inside ? 1.f : 0.f;
			motion.press = down ? 1.f : 0.f;
		}
		RedrawWindow(button, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}

	LRESULT CALLBACK ButtonInput(HWND button, UINT msg, WPARAM wp, LPARAM lp,
		UINT_PTR subclass, DWORD_PTR data)
	{
		auto& motion = *reinterpret_cast<ButtonMotion*>(data);
		switch (msg) {
		case WM_MOUSEMOVE: {
			RECT bounds;
			GetClientRect(button, &bounds);
			POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
			bool inside = IsWindowEnabled(button) && PtInRect(&bounds, pt);
			if (inside && !motion.inside) {
				TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, button, 0 };
				TrackMouseEvent(&track);
			}
			AnimateButton(button, motion, inside, motion.down);
			break;
		}
		case WM_MOUSELEAVE:
			AnimateButton(button, motion, false, motion.down);
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			if (IsWindowEnabled(button)) AnimateButton(button, motion, true, true);
			break;
		case WM_KEYDOWN:
			if (wp == VK_SPACE && IsWindowEnabled(button)) AnimateButton(button, motion, motion.inside, true);
			break;
		case WM_KEYUP:
			if (wp != VK_SPACE) break;
			[[fallthrough]];
		case WM_LBUTTONUP:
		case WM_CAPTURECHANGED:
		case WM_CANCELMODE:
		case WM_KILLFOCUS:
			AnimateButton(button, motion, motion.inside, false);
			break;
		case BM_SETSTATE:
			AnimateButton(button, motion, motion.inside, wp != 0 && IsWindowEnabled(button));
			break;
		case WM_SHOWWINDOW:
		case WM_ENABLE:
			if (!wp) {
				KillTimer(button, motionTimer);
				motion = {};
			}
			break;
		case WM_TIMER:
			if (wp == motionTimer) {
				const auto now = GetTickCount64();
				const float elapsed = std::min(40.f, float(now - motion.lastFrame));
				motion.lastFrame = now;
				auto advance = [elapsed](float value, bool towardsOne, float duration) {
					return towardsOne ? std::min(1.f, value + elapsed / duration)
						: std::max(0.f, value - elapsed / duration);
				};
				motion.hover = advance(motion.hover, motion.inside, 160.f);
				motion.press = advance(motion.press, motion.down, motion.down ? 70.f : 130.f);
				if (motion.hover == float(motion.inside) && motion.press == float(motion.down))
					KillTimer(button, motionTimer);
				RedrawWindow(button, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
				return 0;
			}
			break;
		case WM_NCDESTROY:
			KillTimer(button, motionTimer);
			RemoveWindowSubclass(button, ButtonInput, subclass);
			motions.erase(button);
			break;
		}
		return DefSubclassProc(button, msg, wp, lp);
	}

	void TrackOwnerButton(HWND button)
	{
		if (!button) return;
		auto& motion = motions[button];
		if (!SetWindowSubclass(button, ButtonInput, 1, reinterpret_cast<DWORD_PTR>(&motion)))
			motions.erase(button);
	}

	void ReadButtonMotion(HWND button, float& hover, float& press)
	{
		auto it = motions.find(button);
		if (it == motions.end()) {
			hover = 0.0f;
			press = 0.0f;
			return;
		}
		hover = it->second.hover;
		press = it->second.press;
	}

	HWND Mk(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
		int x, int y, int w, int h, int id, HFONT f, DWORD exStyle = 0)
	{
		HWND c = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style | (_wcsicmp(cls, L"BUTTON") == 0 ? WS_TABSTOP : 0),
			x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
		SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
		if (c && _wcsicmp(cls, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_OWNERDRAW)
			TrackOwnerButton(c);
		return c;
	}

	void DrawStatusBadge(HDC dc, const RECT& r)
	{
		int w = r.right - r.left, h = r.bottom - r.top;
		if (w <= 0 || h <= 0) return;

		HDC memDC = CreateCompatibleDC(dc);
		HBITMAP memBmp = CreateCompatibleBitmap(dc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		HBRUSH bgBr = CreateSolidBrush(theme::BG);
		RECT local = { 0, 0, w, h };
		FillRect(memDC, &local, bgBr);
		DeleteObject(bgBr);

		COLORREF bg = theme::OFF_SOFT, border = theme::BORDER, dot = theme::OFF, fg = theme::TEXT_SUB;
		if (g_status_kind == 1) {
			bg = theme::ON_SOFT; border = theme::ON_BORDER; dot = theme::ON; fg = theme::ON;
		} else if (g_status_kind == 2) {
			bg = theme::AMBER_SOFT; border = theme::AMBER_BORDER; dot = theme::AMBER; fg = theme::AMBER;
		} else if (g_status_kind == 3) {
			bg = theme::WARN_SOFT; border = theme::WARN_BORDER; dot = theme::WARN; fg = theme::WARN;
		}

		const wchar_t* label = g_status_text.empty() ? L"服务未启动" : g_status_text.c_str();
		HFONT oldF = (HFONT)SelectObject(memDC, g_font_small ? g_font_small : g_font_ui);
		SIZE ts{};
		GetTextExtentPoint32W(memDC, label, (int)wcslen(label), &ts);
		int pillW = std::min<int>(w - 2, (int)ts.cx + S(36));
		int pillH = std::min(h - 2, S(28));
		RECT pill = { w - pillW - 1, (h - pillH) / 2, w - 1, (h + pillH) / 2 };

		{
			Gdiplus::Graphics g(memDC);
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
			FillRoundRectAA(g, pill, (float)pillH * 0.5f, bg, border, 1.0f, false);

			float dotSize = (float)S(8);
			float dotX = (float)pill.left + (float)S(12);
			float dotY = (float)pill.top + ((float)pillH - dotSize) * 0.5f;
			Gdiplus::SolidBrush dotBr(ToGpColor(dot));
			g.FillEllipse(&dotBr, dotX, dotY, dotSize, dotSize);
		}

		SetBkMode(memDC, TRANSPARENT);
		SetTextColor(memDC, fg);
		RECT textBox = pill;
		textBox.left += S(25);
		textBox.right -= S(10);
		DrawTextW(memDC, label, -1, &textBox, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
		SelectObject(memDC, oldF);

		BitBlt(dc, r.left, r.top, w, h, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBmp);
		DeleteObject(memBmp);
		DeleteDC(memDC);
	}

	// 双缓冲 + GDI+ 抗锯齿自绘按钮与状态徽章
	void DrawButton(LPDRAWITEMSTRUCT dis)
	{
		if (dis->CtlID == IDC_STATUS) {
			DrawStatusBadge(dis->hDC, dis->rcItem);
			return;
		}

		HDC dc = dis->hDC;
		RECT r = dis->rcItem;
		int w = r.right - r.left, h = r.bottom - r.top;
		if (w <= 0 || h <= 0) return;

		HDC memDC = CreateCompatibleDC(dc);
		HBITMAP memBmp = CreateCompatibleBitmap(dc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		RECT local = { 0, 0, w, h };
		bool disabled = (dis->itemState & ODS_DISABLED) != 0;
		bool keyboardFocus = (dis->itemState & ODS_FOCUS) != 0 && (dis->itemState & ODS_NOFOCUSRECT) == 0;
		auto it = motions.find(dis->hwndItem);
		float rawHot = disabled || it == motions.end() ? 0.f : it->second.hover;
		float rawPress = disabled ? 0.f : it != motions.end() ? it->second.press
			: float((dis->itemState & ODS_SELECTED) != 0);
		float hot = EaseOutCubic(rawHot);
		float press = EaseOutCubic(rawPress);
		int id = (int)dis->CtlID;
		const float radius = (float)S(8);

		if (id >= NavService && id <= NavSettings) {
			bool active = currentPage == id;
			FillRect(memDC, &local, (HBRUSH)GetStockObject(WHITE_BRUSH));
			COLORREF surface = active ? theme::ACCENT_SOFT : theme::CARD;
			surface = Blend(Blend(surface, active ? RGB(221, 234, 255) : theme::FIELD, hot),
				RGB(205, 224, 255), press);
			RECT pill = local;
			{
				Gdiplus::Graphics g(memDC);
				g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
				FillRoundRectAA(g, pill, radius, surface, active ? theme::ACCENT_BORDER : surface, 1.0f, false);
				if (active) {
					RECT bar = { pill.left + S(2), pill.top + S(9), pill.left + S(5), pill.bottom - S(9) };
					FillRoundRectAA(g, bar, (float)S(2), theme::ACCENT, theme::ACCENT, 0.0f, false);
				}
				if (keyboardFocus)
					DrawFocusRingAA(g, pill, radius, theme::ACCENT);
			}
			SetBkMode(memDC, TRANSPARENT);
			SetTextColor(memDC, active ? theme::ACCENT_DN : Blend(theme::TEXT_SUB, theme::ACCENT, hot));
			HGDIOBJ prev = SelectObject(memDC, iconFont);
			const wchar_t glyphs[] = { 0xE968, 0xE8F1, 0xE9D9, 0xE713 };
			RECT textBox = pill;
			textBox.left += S(16);
			DrawTextW(memDC, &glyphs[id - NavService], 1, &textBox, DT_SINGLELINE | DT_VCENTER);
			textBox.left += S(30);
			SelectObject(memDC, active ? g_font_sect : g_font_ui);
			DrawTextW(memDC, pageNames[id - NavService], -1, &textBox, DT_SINGLELINE | DT_VCENTER);
			SelectObject(memDC, prev);
			BitBlt(dc, r.left, r.top, w, h, memDC, 0, 0, SRCCOPY);
			SelectObject(memDC, oldBmp);
			DeleteObject(memBmp);
			DeleteDC(memDC);
			return;
		}

		bool dropdown = (id == BankFilter);
		bool primary = (id == IDC_BTN_START || id == BankAdd || id == MatchOpen);
		bool copied = (id == IDC_BTN_COPY_LOCAL && g_copy_feedback_until[0])
			|| (id == IDC_BTN_COPY_LAN && g_copy_feedback_until[1])
			|| (id == IDC_BTN_COPY_PIN && g_copy_feedback_until[2]);
		bool dangerOutline = (id == IDC_BTN_STOP);

		COLORREF bg, fg, border;
		if (disabled) {
			bg = theme::OFF_SOFT;
			fg = theme::TEXT_MUTED;
			border = theme::BORDER;
		} else if (dropdown) {
			bg = Blend(Blend(theme::FIELD, theme::ACCENT_SOFT, hot * 0.55f), RGB(219, 232, 252), press);
			fg = theme::TEXT;
			border = Blend(theme::BORDER, theme::ACCENT, std::max(hot * 0.65f, keyboardFocus ? 1.0f : 0.0f));
		} else if (copied) {
			bg = theme::ON_SOFT;
			fg = theme::ON;
			border = theme::ON_BORDER;
		} else if (primary) {
			bg = Blend(Blend(theme::ACCENT, theme::ACCENT_HOT, hot), theme::ACCENT_DN, press);
			fg = RGB(255, 255, 255);
			border = bg;
		} else if (dangerOutline) {
			bg = Blend(Blend(theme::CARD, theme::WARN_SOFT, hot), RGB(254, 202, 202), press);
			fg = Blend(theme::WARN, RGB(185, 28, 28), hot);
			border = Blend(theme::BORDER, theme::WARN_BORDER, std::max(hot, press));
		} else {
			bg = Blend(Blend(theme::CARD, theme::ACCENT_SOFT, hot), RGB(212, 227, 252), press);
			fg = Blend(theme::TEXT, theme::ACCENT_DN, hot);
			border = Blend(theme::BORDER, theme::ACCENT_BORDER, std::max(hot, press));
		}

		HBRUSH bgBr = CreateSolidBrush(theme::CARD);
		FillRect(memDC, &local, bgBr);
		DeleteObject(bgBr);

		RECT rr = local;
		InflateRect(&rr, -1, -1);
		{
			Gdiplus::Graphics g(memDC);
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
			FillRoundRectAA(g, rr, radius, bg, border, 1.0f, false);
			if (dropdown) {
				COLORREF chev = disabled ? theme::TEXT_MUTED : Blend(theme::TEXT_SUB, theme::ACCENT, hot);
				DrawDropdownChevron(g, rr, chev);
			}
			if (keyboardFocus)
				DrawFocusRingAA(g, rr, radius, primary ? RGB(255, 255, 255) : theme::ACCENT);
		}

		wchar_t text[128] = { 0 };
		GetWindowTextW(dis->hwndItem, text, 128);

		SetBkMode(memDC, TRANSPARENT);
		SetTextColor(memDC, fg);
		bool smallBtn = (id == IDC_BTN_COPY_LOCAL || id == IDC_BTN_COPY_LAN || id == IDC_BTN_COPY_PIN || id == BankClearSearch);
		HFONT old = (HFONT)SelectObject(memDC, smallBtn && g_font_small ? g_font_small : g_font_ui);
		if (dropdown) {
			RECT textBox = rr;
			textBox.left += S(12);
			textBox.right -= S(24);
			DrawTextW(memDC, text, -1, &textBox, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		} else {
			DrawTextW(memDC, text, -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		SelectObject(memDC, old);

		BitBlt(dc, r.left, r.top, w, h, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBmp);
		DeleteObject(memBmp);
		DeleteDC(memDC);
	}

	// 双缓冲 + GDI+ 抗锯齿画卡片背景、阴影、描边与扁平输入容器
	void DrawCard(HWND hwnd)
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);

		RECT rc;
		GetClientRect(hwnd, &rc);
		int w = rc.right - rc.left, h = rc.bottom - rc.top;
		if (w <= 0 || h <= 0) {
			EndPaint(hwnd, &ps);
			return;
		}

		HDC memDC = CreateCompatibleDC(dc);
		HBITMAP memBmp = CreateCompatibleBitmap(dc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		HBRUSH bg = CreateSolidBrush(theme::BG);
		FillRect(memDC, &rc, bg);
		DeleteObject(bg);

		RECT sidebar = { 0, 0, S(174), rc.bottom };
		FillRect(memDC, &sidebar, (HBRUSH)GetStockObject(WHITE_BRUSH));
		HBRUSH line = CreateSolidBrush(theme::BORDER);
		RECT divider = { sidebar.right - 1, 0, sidebar.right, rc.bottom };
		FillRect(memDC, &divider, line);
		DeleteObject(line);

		{
			Gdiplus::Graphics g(memDC);
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

			FillRoundRectAA(g, g_panel, (float)S(12), theme::CARD, theme::BORDER, 1.0f, true);

			if (currentPage == NavService) {
				FillRoundRectAA(g, g_card_log, (float)S(12), theme::CARD, theme::BORDER, 1.0f, true);
				FillRoundRectAA(g, g_box_url_local, (float)S(8), theme::FIELD, theme::BORDER, 1.0f, false);
				FillRoundRectAA(g, g_box_url_lan, (float)S(8), theme::FIELD, theme::BORDER, 1.0f, false);
				FillRoundRectAA(g, g_box_pin, (float)S(8), theme::ACCENT_SOFT, theme::ACCENT_BORDER, 1.0f, false);
				FillRoundRectAA(g, g_box_log, (float)S(8), theme::FIELD, theme::BORDER, 1.0f, false);
			} else if (currentPage == NavProblems) {
				COLORREF sBorder = g_search_focused ? theme::ACCENT : theme::BORDER;
				FillRoundRectAA(g, g_box_search, (float)S(8), theme::FIELD, sBorder, g_search_focused ? 1.5f : 1.0f, false);
				FillRoundRectAA(g, g_box_bank_list, (float)S(8), theme::CARD, theme::BORDER, 1.0f, false);
			} else if (currentPage == NavMatches) {
				FillRoundRectAA(g, g_box_match_list, (float)S(8), theme::CARD, theme::BORDER, 1.0f, false);
			}
		}

		if (currentPage == NavProblems && iconFont) {
			SetBkMode(memDC, TRANSPARENT);
			SetTextColor(memDC, g_search_focused ? theme::ACCENT : theme::TEXT_MUTED);
			HGDIOBJ prev = SelectObject(memDC, iconFont);
			RECT iconRc = { g_box_search.left + S(10), g_box_search.top, g_box_search.left + S(30), g_box_search.bottom };
			const wchar_t searchGlyph = 0xE721;
			DrawTextW(memDC, &searchGlyph, 1, &iconRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
			SelectObject(memDC, prev);
		}

		BitBlt(dc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBmp);
		DeleteObject(memBmp);
		DeleteDC(memDC);

		EndPaint(hwnd, &ps);
	}

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_CREATE:
		{
			ApplyDwmTheme(hwnd);
			CreateFonts();

			Mk(hwnd, L"STATIC", L"Responder", SS_LEFT, 0, 0, 0, 0, Brand, g_font_title);
			Mk(hwnd, L"STATIC", L"STA  /  竞赛控制台", SS_LEFT, 0, 0, 0, 0, BrandCaption, g_font_small);
			for (int i = 0; i < 4; ++i)
				Mk(hwnd, L"BUTTON", pageNames[i], BS_OWNERDRAW, 0, 0, 0, 0, NavService + i, g_font_ui);
			Mk(hwnd, L"STATIC", pageNames[0], SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_TITLE, g_font_title);
			Mk(hwnd, L"STATIC", L"本机比赛记录", SS_LEFT, 0, 0, 0, 0, MatchTitle, g_font_sect);
			Mk(hwnd, L"BUTTON", L"打开比赛管理", BS_OWNERDRAW, 0, 0, 0, 0, MatchOpen, g_font_ui);
			Mk(hwnd, L"BUTTON", L"刷新", BS_OWNERDRAW, 0, 0, 0, 0, MatchRefresh, g_font_ui);
			Mk(hwnd, L"STATIC", L"", SS_LEFT, 0, 0, 0, 0, MatchHint, g_font_small);
			HWND list = Mk(hwnd, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
				0, 0, 0, 0, MatchList, g_font_ui);
			ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
			SetWindowTheme(list, L"Explorer", nullptr);
			ListView_SetBkColor(list, theme::CARD);
			ListView_SetTextBkColor(list, theme::CARD);
			ListView_SetTextColor(list, theme::TEXT);
			const wchar_t* columns[] = { L"比赛名称", L"比赛状态", L"队伍数" };
			for (int i = 0; i < 3; ++i) {
				LVCOLUMNW col{};
				col.mask = LVCF_TEXT | LVCF_WIDTH;
				col.cx = S(100);
				col.pszText = const_cast<wchar_t*>(columns[i]);
				ListView_InsertColumn(list, i, &col);
			}
			Mk(hwnd, L"STATIC", L"日志显示", SS_LEFT, 0, 0, 0, 0, SettingsTitle, g_font_sect);
			Mk(hwnd, L"BUTTON", L"自动滚动到最新日志", BS_AUTOCHECKBOX, 0, 0, 0, 0, AutoScroll, g_font_ui);
			Mk(hwnd, L"STATIC", L"关闭后，可在服务管理中停留在当前日志位置。", SS_LEFT, 0, 0, 0, 0, SettingsHint, g_font_small);
			followLog = GetPrivateProfileIntW(L"Window", L"FollowLog", 1, prefsFile) != 0;
			CheckDlgButton(hwnd, AutoScroll, followLog ? BST_CHECKED : BST_UNCHECKED);
			g_status = Mk(hwnd, L"STATIC", L"服务未启动", SS_OWNERDRAW,
				0, 0, 0, 0, IDC_STATUS, g_font_small);

			// ---- 服务状态卡片 ----
			Mk(hwnd, L"STATIC", L"服务状态", SS_LEFT, 0, 0, 0, 0, IDC_STATIC_SVC, g_font_sect);
			Mk(hwnd, L"STATIC", L"本机地址", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_URL1, g_font_ui);
			Mk(hwnd, L"STATIC", L"局域网地址", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_URL2, g_font_ui);
			Mk(hwnd, L"STATIC", L"登录 PIN", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_PIN, g_font_ui);

			g_url_local = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_AUTOHSCROLL,
				0, 0, 0, 0, IDC_URL_LOCAL, g_font_mono);
			g_url_lan = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_AUTOHSCROLL,
				0, 0, 0, 0, IDC_URL_LAN, g_font_mono);
			g_pin = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_CENTER,
				0, 0, 0, 0, IDC_LOGIN_PIN, g_font_pin);

			g_btn_start = Mk(hwnd, L"BUTTON", L"上线", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_START, g_font_ui);
			g_btn_stop = Mk(hwnd, L"BUTTON", L"下线", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_STOP, g_font_ui);
			g_btn_browser = Mk(hwnd, L"BUTTON", L"打开浏览器", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_BROWSER, g_font_ui);
			g_btn_copy_local = Mk(hwnd, L"BUTTON", L"复制", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_COPY_LOCAL, g_font_small);
			g_btn_copy_lan = Mk(hwnd, L"BUTTON", L"复制地址", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_COPY_LAN, g_font_small);
			g_btn_copy_pin = Mk(hwnd, L"BUTTON", L"复制 PIN", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_COPY_PIN, g_font_small);

			g_hint = Mk(hwnd, L"STATIC", L"点击「上线」后，前端电脑即可连接。", SS_LEFT,
				0, 0, 0, 0, IDC_HINT, g_font_small);

			// ---- 题库管理卡片 ----
			Mk(hwnd, L"STATIC", L"题目列表", SS_LEFT, 0, 0, 0, 0, IDC_STATIC_PB1, g_font_sect);
			Mk(hwnd, L"STATIC", L"搜索", SS_LEFT, 0, 0, 0, 0, BankSearchLabel, g_font_ui);
			HWND search = Mk(hwnd, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL,
				0, 0, 0, 0, BankSearch, g_font_ui);
			SendMessageW(search, EM_SETCUEBANNER, FALSE, (LPARAM)L"搜索题号、题干、答案、选项或线索");
			Mk(hwnd, L"BUTTON", L"清空", BS_OWNERDRAW, 0, 0, 0, 0, BankClearSearch, g_font_small);
			Mk(hwnd, L"BUTTON", filterNames[g_bank_filter], BS_OWNERDRAW,
				0, 0, 0, 0, BankFilter, g_font_ui);
			Mk(hwnd, L"BUTTON", L"编辑选中题", BS_OWNERDRAW, 0, 0, 0, 0, BankEdit, g_font_ui);
			EnableWindow(GetDlgItem(hwnd, BankEdit), FALSE);
			Mk(hwnd, L"BUTTON", L"新增题目", BS_OWNERDRAW, 0, 0, 0, 0, BankAdd, g_font_ui);
			HWND bank = Mk(hwnd, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
				0, 0, 0, 0, BankList, g_font_ui);
			ListView_SetExtendedListViewStyle(bank, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
			SetWindowTheme(bank, L"Explorer", nullptr);
			ListView_SetBkColor(bank, theme::CARD);
			ListView_SetTextBkColor(bank, theme::CARD);
			ListView_SetTextColor(bank, theme::TEXT);
			const wchar_t* bankColumns[] = { L"题号", L"题型", L"题干", L"难度", L"状态" };
			for (int i = 0; i < 5; ++i) {
				LVCOLUMNW col{};
				col.mask = LVCF_TEXT | LVCF_WIDTH;
				col.cx = S(80);
				col.pszText = const_cast<wchar_t*>(bankColumns[i]);
				ListView_InsertColumn(bank, i, &col);
			}
			g_stats = Mk(hwnd, L"STATIC", L"", SS_LEFT, 0, 0, 0, 0, IDC_STATS, g_font_stat);
			Mk(hwnd, L"STATIC",
				L"转换会替换整库，合并仅追加新题。修改前自动备份至 data\\problems_backup\\。",
				SS_LEFT, 0, 0, 0, 0, IDC_STATIC_PB2, g_font_small);

			g_btn_convert = Mk(hwnd, L"BUTTON", L"转换题库…", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_CONVERT, g_font_ui);
			g_btn_merge = Mk(hwnd, L"BUTTON", L"合并题库…", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_MERGE, g_font_ui);
			g_btn_dedup = Mk(hwnd, L"BUTTON", L"去除重复题", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_DEDUP, g_font_ui);
			g_btn_backup = Mk(hwnd, L"BUTTON", L"立即备份", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_BACKUP, g_font_ui);

			// ---- 日志卡片 ----
			Mk(hwnd, L"STATIC", L"运行日志", SS_LEFT, 0, 0, 0, 0, IDC_STATIC_LOG, g_font_sect);
			g_log = Mk(hwnd, L"EDIT", L"",
				ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | ES_LEFT,
				0, 0, 0, 0, IDC_LOG, g_font_mono);

			LayoutControls(hwnd);
			ApplyServerState(hwnd);
			AppendLog(L"Responder 管理程序已启动。");
			return 0;
		}

		case WM_SIZE:
			if (wp != SIZE_MINIMIZED)
			{
				LayoutControls(hwnd);
				InvalidateRect(hwnd, nullptr, TRUE);
			}
			return 0;

		case WM_GETMINMAXINFO:
		{
			MINMAXINFO* mmi = (MINMAXINFO*)lp;
			mmi->ptMinTrackSize.x = S(760);
			mmi->ptMinTrackSize.y = S(560);
			return 0;
		}

		case WM_ERASEBKGND:
			return 1; // 背景在 WM_PAINT 里一次画完，避免闪烁

		case WM_PAINT:
			DrawCard(hwnd);
			return 0;

		case WM_DRAWITEM:
			DrawButton((LPDRAWITEMSTRUCT)lp);
			return TRUE;

		case WM_CTLCOLORBTN:
		case WM_CTLCOLORSTATIC:
		{
			HDC dc = (HDC)wp;
			HWND ctl = (HWND)lp;
			SetBkMode(dc, TRANSPARENT);

			int ctlId = GetDlgCtrlID(ctl);
			if (ctlId == Brand || ctl == g_pin) SetTextColor(dc, theme::ACCENT);
			else if (ctlId == IDC_TITLE || ctlId == IDC_STATIC_SVC || ctlId == IDC_STATIC_PB1 || ctlId == MatchTitle || ctlId == SettingsTitle || ctlId == IDC_STATIC_LOG || ctl == g_log) SetTextColor(dc, theme::TEXT);
			else if (ctl == g_hint)
			{
				SetTextColor(dc, server_core::last_error().empty() ? theme::TEXT_SUB : theme::WARN);
			}
			else if (ctl == g_stats)
			{
				SetTextColor(dc, theme::TEXT);
			}
			else
			{
				SetTextColor(dc, theme::TEXT_SUB);
			}
			if (ctlId == IDC_TITLE) {
				static HBRUSH header = CreateSolidBrush(theme::BG);
				return (LRESULT)header;
			}
			if (ctl == g_url_local || ctl == g_url_lan) {
				SetBkMode(dc, OPAQUE);
				SetBkColor(dc, theme::FIELD);
				SetTextColor(dc, theme::TEXT);
				static HBRUSH roFieldBr = CreateSolidBrush(theme::FIELD);
				return (LRESULT)roFieldBr;
			}
			if (ctl == g_pin) {
				SetBkMode(dc, OPAQUE);
				SetBkColor(dc, theme::ACCENT_SOFT);
				SetTextColor(dc, theme::ACCENT_DN);
				static HBRUSH roPinBr = CreateSolidBrush(theme::ACCENT_SOFT);
				return (LRESULT)roPinBr;
			}
			if (ctl == g_log) {
				SetBkMode(dc, OPAQUE);
				SetBkColor(dc, theme::FIELD);
				SetTextColor(dc, theme::TEXT);
				static HBRUSH roLogBr = CreateSolidBrush(theme::FIELD);
				return (LRESULT)roLogBr;
			}
			// 返回卡片底色画刷，避免文字下面残留窗口底色
			static HBRUSH cardBr = CreateSolidBrush(theme::CARD);
			return (LRESULT)cardBr;
		}

		case WM_SETCURSOR:
		{
			// 自绘按钮在可点击时显示手型光标
			HWND target = (HWND)wp;
			if (target && IsWindowEnabled(target))
			{
				wchar_t cls[32] = { 0 };
				GetClassNameW(target, cls, 32);
				if (_wcsicmp(cls, L"Button") == 0)
				{
					SetCursor(LoadCursorW(nullptr, IDC_HAND));
					return TRUE;
				}
			}
			break;
		}

		case WM_CTLCOLOREDIT:
		{
			HDC dc = (HDC)wp;
			HWND ctl = (HWND)lp;
			SetBkMode(dc, OPAQUE);
			if (ctl == g_log)
			{
				SetBkColor(dc, theme::FIELD);
				SetTextColor(dc, theme::TEXT);
				static HBRUSH logBr = CreateSolidBrush(theme::FIELD);
				return (LRESULT)logBr;
			}
			if (ctl == g_pin)
			{
				// PIN 用浅蓝胶囊底 + 深蓝大字突出
				SetBkColor(dc, theme::ACCENT_SOFT);
				SetTextColor(dc, theme::ACCENT_DN);
				static HBRUSH pinBr = CreateSolidBrush(theme::ACCENT_SOFT);
				return (LRESULT)pinBr;
			}
			// 访问地址与搜索框：深色文字、浅灰底
			SetBkColor(dc, theme::FIELD);
			SetTextColor(dc, theme::TEXT);
			static HBRUSH fieldBr = CreateSolidBrush(theme::FIELD);
			return (LRESULT)fieldBr;
		}

		case WM_NOTIFY:
		{
			auto* note = (NMHDR*)lp;
			if (note->idFrom == BankList) {
				if (note->code == LVN_ITEMCHANGED)
					EnableWindow(GetDlgItem(hwnd, BankEdit), !g_op_running && ListView_GetNextItem(note->hwndFrom, -1, LVNI_SELECTED) >= 0);
				if (note->code == NM_DBLCLK || note->code == NM_RETURN) OpenQuestion(hwnd, false);
			}
			break;
		}
		case WM_COMMAND:
		{
			int id = LOWORD(wp);
			if (id >= NavService && id <= NavSettings && HIWORD(wp) == BN_CLICKED) {
				currentPage = id;
				SetText(GetDlgItem(hwnd, IDC_TITLE), pageNames[id - NavService]);
				if (id == NavMatches) RefreshMatches(hwnd);
				if (id == NavProblems && !g_op_running) RefreshBank(hwnd);
				LayoutControls(hwnd);
				RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
				return 0;
			}
			switch (id)
			{
			case BankAdd:
			case BankEdit:
				OpenQuestion(hwnd, id == BankAdd);
				return 0;
			case BankFilter:
				if (HIWORD(wp) == BN_CLICKED && !g_op_running) {
					int picked = ShowDropdownMenu(hwnd, (HWND)lp, filterNames, 4, g_bank_filter);
					if (picked >= 0 && picked != g_bank_filter) {
						g_bank_filter = picked;
						SetDlgItemTextW(hwnd, BankFilter, filterNames[g_bank_filter]);
						InvalidateRect((HWND)lp, nullptr, FALSE);
						RefreshBank(hwnd);
					}
				}
				return 0;
			case BankSearch:
				if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) {
					g_search_focused = (HIWORD(wp) == EN_SETFOCUS);
					InvalidateRect(hwnd, &g_box_search, FALSE);
				}
				if (HIWORD(wp) == EN_CHANGE && currentPage == NavProblems && !g_op_running)
					RefreshBank(hwnd);
				return 0;
			case BankClearSearch:
				SetDlgItemTextW(hwnd, BankSearch, L"");
				SetFocus(GetDlgItem(hwnd, BankSearch));
				return 0;
			case MatchRefresh:
				RefreshMatches(hwnd);
				return 0;
			case AutoScroll:
				if (HIWORD(wp) == BN_CLICKED) {
					followLog = IsDlgButtonChecked(hwnd, AutoScroll) == BST_CHECKED;
					if (!WritePrivateProfileStringW(L"Window", L"FollowLog", followLog ? L"1" : L"0", prefsFile))
						MessageBoxW(hwnd, L"设置已在本次运行中生效，但无法保存到 data 目录。", L"设置", MB_ICONINFORMATION);
				}
				return 0;
			case IDC_BTN_START:
				DoStart(hwnd);
				return 0;

			case IDC_BTN_STOP:
				DoStop();
				return 0;

			case IDC_BTN_COPY_LOCAL:
				if (CopyTextToClipboard(hwnd, GetText(g_url_local)))
					TriggerCopyFeedback(hwnd, 0, IDC_BTN_COPY_LOCAL);
				return 0;

			case IDC_BTN_COPY_LAN:
				if (CopyTextToClipboard(hwnd, GetText(g_url_lan)))
					TriggerCopyFeedback(hwnd, 1, IDC_BTN_COPY_LAN);
				return 0;

			case IDC_BTN_COPY_PIN:
				if (CopyTextToClipboard(hwnd, GetText(g_pin)))
					TriggerCopyFeedback(hwnd, 2, IDC_BTN_COPY_PIN);
				return 0;

			case MatchOpen:
			case IDC_BTN_BROWSER:
			{
				std::wstring url = L"http://127.0.0.1:" + std::to_wstring(server_core::PORT);
				if (id == MatchOpen) url += L"/main";
				ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				return 0;
			}

			case IDC_BTN_CONVERT:
			case IDC_BTN_MERGE:
			{
				if (g_op_running) return 0;
				std::string file = PickTextFile(hwnd);
				if (file.empty()) return 0;
				if (!StageUploadFile(file))
				{
					MessageBoxW(hwnd, L"无法读取所选的题库文件。", L"错误", MB_ICONERROR);
					return 0;
				}
				g_op_running = true;
				SetOperationButtonsEnabled(FALSE);
				AppendLog(L"已载入文件：" + ToWide(file));
				std::thread(RunProblemOperation, id).detach();
				return 0;
			}

			case IDC_BTN_DEDUP:
			case IDC_BTN_BACKUP:
			{
				if (g_op_running) return 0;

				if (id == IDC_BTN_DEDUP)
				{
					if (MessageBoxW(hwnd,
						L"将对当前题库去除重复题并重新编号。\n操作前会自动备份到 data\\problems_backup\\。\n\n确定继续？",
						L"确认", MB_ICONQUESTION | MB_YESNO) != IDYES)
						return 0;
				}

				g_op_running = true;
				SetOperationButtonsEnabled(FALSE);
				std::thread(RunProblemOperation, id).detach();
				return 0;
			}
			}
			return 0;
		}

		case WM_CLOSE:
		{
			if (server_core::is_running())
			{
				if (MessageBoxW(hwnd, L"服务正在运行，退出将自动下线。确定退出？",
					L"确认退出", MB_ICONQUESTION | MB_YESNO) != IDYES)
					return 0;
				server_core::stop();
			}
			DestroyWindow(hwnd);
			return 0;
		}

		case WM_DESTROY:
			if (iconFont) DeleteObject(iconFont);
			if (g_font_ui) DeleteObject(g_font_ui);
			if (g_font_small) DeleteObject(g_font_small);
			if (g_font_sect) DeleteObject(g_font_sect);
			if (g_font_title) DeleteObject(g_font_title);
			if (g_font_pin) DeleteObject(g_font_pin);
			if (g_font_stat) DeleteObject(g_font_stat);
			if (g_font_mono) DeleteObject(g_font_mono);
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wp, lp);
	}
}

// 桌面管理程序的入口（由 main.cpp 调用）
int RunGui()
{
	InitDpiScale();

	Gdiplus::GdiplusStartupInput gdipInput;
	ULONG_PTR gdipToken = 0;
	Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
	InitCommonControlsEx(&icc);

	HINSTANCE hInst = GetModuleHandleW(nullptr);
	HICON hBigIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_RESPONDER), IMAGE_ICON,
		GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	HICON hSmIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_RESPONDER), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	if (!hBigIcon) hBigIcon = (HICON)LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
	if (!hSmIcon) hSmIcon = hBigIcon;

	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(theme::BG);
	wc.lpszClassName = L"ResponderManagerWnd";
	wc.hIcon = hBigIcon;
	wc.hIconSm = hSmIcon;
	if (!RegisterClassExW(&wc))
	{
		if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
		MessageBoxW(nullptr, L"窗口类注册失败。", L"错误", MB_ICONERROR);
		return 1;
	}

	// 屏幕居中。窗口含标题栏，客户区按缩放后的尺寸预留
	const int clientW = S(960), clientH = S(640);
	RECT want = { 0, 0, clientW, clientH };
	AdjustWindowRectEx(&want, WS_OVERLAPPEDWINDOW, FALSE, 0);
	int ww = want.right - want.left, wh = want.bottom - want.top;
	int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
	// 屏幕装不下时按比例收缩，避免窗口超出可见区域
	if (ww > sw - S(20))
	{
		double k = (double)(sw - S(20)) / ww;
		ww = sw - S(20);
		wh = (int)(wh * k);
	}
	if (wh > sh - S(40))
		wh = sh - S(40);
	int x = (sw - ww) / 2, y = (sh - wh) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Responder 管理程序  by STA",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, ww, wh, nullptr, nullptr, wc.hInstance, nullptr);
	if (!hwnd)
	{
		if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
		MessageBoxW(nullptr, L"窗口创建失败。", L"错误", MB_ICONERROR);
		return 1;
	}
	if (hBigIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
	if (hSmIcon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmIcon);

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	// 消息循环：100ms 超时轮询，刷新日志与服务状态
	MSG msg;
	int exitCode = 0;
	for (;;)
	{
		DWORD r = MsgWaitForMultipleObjectsEx(0, nullptr, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		if (r == WAIT_TIMEOUT)
		{
			DrainLogToWindow();
			ApplyServerState(hwnd);
			if (!g_op_running && !IsWindowEnabled(g_btn_backup))
			{
				SetOperationButtonsEnabled(TRUE);
				if (currentPage == NavProblems) RefreshBank(hwnd);
			}
			continue;
		}
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) {
				exitCode = (int)msg.wParam;
				if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
				return exitCode;
			}
			if (msg.message == WM_KEYDOWN && (msg.wParam == VK_TAB || msg.wParam == VK_RETURN) && IsDialogMessageW(hwnd, &msg)) continue;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
}
