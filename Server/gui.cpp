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
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <map>
#include <algorithm>

#include "responder_problem.h"
#include "responder_pages.h"
#include "functions.h"
#include "logging.h"
#include "responder_competition.h"
#include "question_editor_ids.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "uxtheme.lib")

// ===== 控件 ID =====
#define IDC_STATUS        1001
#define IDC_URL_LOCAL     1002
#define IDC_URL_LAN       1003
#define IDC_LOGIN_PIN           1004
#define IDC_BTN_START     1010
#define IDC_BTN_STOP      1011
#define IDC_BTN_BROWSER   1012
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

// ===== 配色 =====
namespace theme
{
	const COLORREF BG         = RGB(242, 247, 253);  // 窗口底色
	const COLORREF CARD       = RGB(0xFF, 0xFF, 0xFF);  // 卡片
	const COLORREF BORDER     = RGB(211, 225, 241);  // 卡片描边
	const COLORREF TEXT       = RGB(24, 47, 78);  // 主文字
	const COLORREF TEXT_SUB   = RGB(76, 99, 126);  // 次要文字
	const COLORREF ACCENT     = RGB(0x2B, 0x6C, 0xE6);  // 主色（按钮）
	const COLORREF ACCENT_HOT = RGB(0x1E, 0x5A, 0xCC);
	const COLORREF ACCENT_DN  = RGB(0x17, 0x49, 0xA8);
	const COLORREF ON         = RGB(0x16, 0xA3, 0x4A);  // 已上线
	const COLORREF OFF        = RGB(0x9C, 0xA3, 0xAF);  // 已下线
	const COLORREF WARN       = RGB(0xD9, 0x53, 0x1E);  // 错误
	const COLORREF FIELD      = RGB(0xF8, 0xF9, 0xFB);  // 只读输入框底
}

// ===== 字体 =====
static HFONT g_font_ui = nullptr;      // 正文 14px
static HFONT g_font_small = nullptr;   // 小字 12px
static HFONT g_font_sect = nullptr;    // 区块标题 15px 粗
static HFONT g_font_title = nullptr;   // 窗口标题 21px 粗
static HFONT g_font_pin = nullptr;     // PIN 26px 粗等宽
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

static HWND g_btn_convert = nullptr;
static HWND g_btn_merge = nullptr;
static HWND g_btn_dedup = nullptr;
static HWND g_btn_backup = nullptr;
static HWND g_btn_browser = nullptr;

static std::atomic<bool> g_op_running{ false };  // 是否正在执行题库操作
static std::atomic<bool> g_stopping{ false };    // 是否正在停止服务
static std::atomic<bool> g_starting{ false };    // 是否正在启动服务
static std::string g_last_shown_pin;

// 卡片矩形（WM_SIZE 时按客户区重算）
static RECT g_panel{};
static RECT g_card_log = { 0, 0, 0, 0 };

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

	// 画圆角矩形（GDI 无原生支持，用圆角矩形路径）
	void FillRoundRect(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF border)
	{
		HBRUSH br = CreateSolidBrush(fill);
		HPEN pen = CreatePen(PS_SOLID, 1, border);

		HGDIOBJ oldBr = SelectObject(dc, br);
		HGDIOBJ oldPen = SelectObject(dc, pen);

		RoundRect(dc, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);

		SelectObject(dc, oldPen);
		SelectObject(dc, oldBr);
		DeleteObject(br);
		DeleteObject(pen);
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
		int filter = (int)SendDlgItemMessageW(hwnd, BankFilter, CB_GETCURSEL, 0, 0) - 1;
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
		int savedType = -1, savedId = 0;
	};

	void ShowTypeFields(HWND dlg, int type)
	{
		SetDlgItemTextW(dlg, QE_EXTRA_LABEL, type == 0 ? L"四个选项（依次为 A、B、C、D）" : L"四条线索（按展示顺序）");
		for (int id : { QE_EXTRA_LABEL, QE_LINE1, QE_LINE2, QE_LINE3, QE_LINE4 })
			ShowWindow(GetDlgItem(dlg, id), type == 1 ? SW_HIDE : SW_SHOWNA);
	}

	INT_PTR CALLBACK QuestionDialog(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_INITDIALOG) {
			auto* form = reinterpret_cast<QuestionForm*>(lp);
			SetWindowLongPtrW(dlg, DWLP_USER, lp);
			const auto& q = form->original;
			std::wstring title = q.id ? std::wstring(L"编辑") + typeNames[q.type] + L" · " + std::to_wstring(q.id) : L"新增题目";
			SetWindowTextW(dlg, title.c_str());
			for (auto name : typeNames) SendDlgItemMessageW(dlg, QE_TYPE, CB_ADDSTRING, 0, (LPARAM)name);
			for (auto name : { L"未指定", L"1 · 简单", L"2 · 中等", L"3 · 困难" })
				SendDlgItemMessageW(dlg, QE_LEVEL, CB_ADDSTRING, 0, (LPARAM)name);
			SendDlgItemMessageW(dlg, QE_TYPE, CB_SETCURSEL, q.type, 0);
			SendDlgItemMessageW(dlg, QE_LEVEL, CB_SETCURSEL, q.level, 0);
			EnableWindow(GetDlgItem(dlg, QE_TYPE), !q.id);
			SetDlgItemTextW(dlg, QE_STEM, ToWide(q.stem).c_str());
			SetDlgItemTextW(dlg, QE_ANSWER, ToWide(q.answer).c_str());
			for (size_t i = 0; i < std::min<size_t>(4, q.lines.size()); ++i)
				SetDlgItemTextW(dlg, QE_LINE1 + (int)i, ToWide(q.lines[i]).c_str());
			ShowTypeFields(dlg, q.type);
			SetFocus(GetDlgItem(dlg, q.id ? QE_STEM : QE_TYPE));
			return FALSE;
		}
		if (msg != WM_COMMAND) return FALSE;
		auto& form = *reinterpret_cast<QuestionForm*>(GetWindowLongPtrW(dlg, DWLP_USER));
		int type = (int)SendDlgItemMessageW(dlg, QE_TYPE, CB_GETCURSEL, 0, 0);
		switch (LOWORD(wp)) {
		case QE_TYPE:
			if (HIWORD(wp) == CBN_SELCHANGE) ShowTypeFields(dlg, type);
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
				Question q;
				q.type = type;
				q.level = (int)SendDlgItemMessageW(dlg, QE_LEVEL, CB_GETCURSEL, 0, 0);
				if (q.level < 0) throw std::runtime_error("请选择难度。");
				q.stem = required(QE_STEM, "请输入题干。");
				if (type != 1)
					for (int i = 0; i < 4; ++i)
						q.lines.push_back(required(QE_LINE1 + i, type == 0 ? "请填写全部四个选项。" : "请填写全部四条线索。"));
				q.answer = required(QE_ANSWER, "请输入答案。");
				// Imported choice banks may use the answer text rather than a letter.
				if (type == 0 && q.answer.size() == 1 && q.answer[0] >= 'a' && q.answer[0] <= 'd')
					q.answer[0] -= 'a' - 'A';
				if (type == 0) form.savedId = SaveQuestion(q, form.original, &problem_list::choice_problems);
				else if (type == 1) form.savedId = SaveQuestion(q, form.original, &problem_list::completion_problems);
				else form.savedId = SaveQuestion(q, form.original, &problem_list::info_problems);
				form.savedType = type;
				EndDialog(dlg, IDOK);
			} catch (const std::exception& ex) {
				SetDlgItemTextW(dlg, QE_FEEDBACK, ToWide(ex.what()).c_str());
			}
			return TRUE;
		}
		return FALSE;
	}

	void OpenQuestion(HWND hwnd, bool adding)
	{
		if (g_op_running) return;
		QuestionForm form;
		if (adding) {
			int filter = (int)SendDlgItemMessageW(hwnd, BankFilter, CB_GETCURSEL, 0, 0);
			form.original.type = std::max(0, filter - 1);
		} else {
			int row = ListView_GetNextItem(GetDlgItem(hwnd, BankList), -1, LVNI_SELECTED);
			if (row < 0 || (size_t)row >= visibleQuestions.size()) return;
			form.original = visibleQuestions[row];
		}
		auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_QUESTION), hwnd, QuestionDialog, (LPARAM)&form);
		if (result == -1) MessageBoxW(hwnd, L"无法打开题目编辑窗口。", L"题库管理", MB_ICONERROR);
		if (result == IDOK) {
			int filter = (int)SendDlgItemMessageW(hwnd, BankFilter, CB_GETCURSEL, 0, 0);
			if (filter > 0 && filter != form.savedType + 1)
				SendDlgItemMessageW(hwnd, BankFilter, CB_SETCURSEL, form.savedType + 1, 0);
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

		if (running)
		{
			SetText(g_status, L"● 已上线");
			EnableWindow(g_btn_start, FALSE);
			EnableWindow(g_btn_stop, TRUE);
			EnableWindow(g_btn_browser, TRUE);

			std::string ip = get_first_ip();
			SetText(g_url_local, L"http://127.0.0.1:" + std::to_wstring(server_core::PORT));
			SetText(g_url_lan, ip.empty()
				? L"未检测到局域网地址"
				: L"http://" + ToWide(ip) + L":" + std::to_wstring(server_core::PORT));
			SetText(g_hint, L"前端电脑可通过局域网地址和登录 PIN 进入比赛。");

			if (g_last_shown_pin != authentic_pin)
			{
				g_last_shown_pin = authentic_pin;
				SetText(g_pin, ToWide(g_last_shown_pin));
			}
		}
		else
		{
			SetText(g_status, L"○ 已下线");
			EnableWindow(g_btn_start, TRUE);
			EnableWindow(g_btn_stop, FALSE);
			EnableWindow(g_btn_browser, FALSE);
			SetText(g_url_local, L"—");
			SetText(g_url_lan, L"—");
			SetText(g_pin, L"—");
			g_last_shown_pin.clear();

			std::string err = server_core::last_error();
			if (!err.empty())
				SetText(g_hint, ToWide(err));
			else if (g_stopping)
				SetText(g_hint, L"正在下线…");
			else if (g_starting)
				SetText(g_hint, L"正在上线…");
			else
				SetText(g_hint, L"点击「上线」后，前端电脑即可连接。");
		}

		// 状态颜色：上线绿、下线灰、有错误红
		InvalidateRect(g_status, nullptr, TRUE);
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
		auto place = [&](int id, int x, int y, int width, int height, bool visible = true) {
			HWND ctl = GetDlgItem(hwnd, id);
			if (visible) MoveWindow(ctl, S(x), S(y), S(width), S(height), TRUE);
			ShowWindow(ctl, visible ? SW_SHOWNA : SW_HIDE);
		};
		place(Brand, 22, 24, 130, 34);
		place(BrandCaption, 23, 61, 130, 22);
		for (int i = 0; i < 4; ++i) place(NavService + i, 10, 110 + 52 * i, 154, 44);
		place(IDC_TITLE, left, 19, 210, 38);
		place(IDC_STATUS, w - 160, 23, 136, 31);
		const bool service = currentPage == NavService, problems = currentPage == NavProblems;
		const bool matches = currentPage == NavMatches, settings = currentPage == NavSettings;
		int panelBottom = matches || problems ? h - 24 : service ? 306 : 214;
		g_panel = { S(left), S(76), S(w - 24), S(panelBottom) };
		int x = left + 20;
		place(IDC_STATIC_SVC, x, 98, 116, 26, service);
		place(IDC_BTN_START, w - 312, 90, 72, 36, service);
		place(IDC_BTN_STOP, w - 232, 90, 72, 36, service);
		place(IDC_BTN_BROWSER, w - 152, 90, 108, 36, service);
		place(IDC_STATIC_URL1, x, 150, 92, 24, service);
		place(IDC_URL_LOCAL, x + 104, 150, inner - 104, 25, service);
		place(IDC_STATIC_URL2, x, 184, 92, 24, service);
		place(IDC_URL_LAN, x + 104, 184, inner - 104, 25, service);
		place(IDC_STATIC_PIN, x, 221, 92, 32, service);
		place(IDC_LOGIN_PIN, x + 104, 220, 140, 36, service);
		place(IDC_HINT, x, 269, inner, 25, service);

		place(IDC_STATIC_PB1, x, 98, 90, 26, problems);
		place(BankSearchLabel, x, 143, 40, 23, problems);
		place(BankSearch, x + 44, 138, inner - 222, 29, problems);
		place(BankClearSearch, x + inner - 170, 138, 56, 29, problems);
		place(BankFilter, x + inner - 104, 138, 104, 134, problems);
		place(BankEdit, w - 242, 90, 92, 34, problems);
		place(BankAdd, w - 142, 90, 98, 34, problems);
		place(BankList, x, 179, inner, h - 337, problems);
		if (problems) {
			HWND list = GetDlgItem(hwnd, BankList);
			const int widths[] = { 48, 70, inner - 244, 48, 56 };
			for (int i = 0; i < 5; ++i) ListView_SetColumnWidth(list, i, S(widths[i]));
		}
		place(IDC_STATS, x, h - 150, inner, 24, problems);
		for (int i = 0; i < 4; ++i)
			place(IDC_BTN_CONVERT + i, x + i * (inner + 8) / 4, h - 112, (inner - 24) / 4, 32, problems);
		place(IDC_STATIC_PB2, x, h - 71, inner, 38, problems);
		const int logY = panelBottom + 14;
		g_card_log = { S(left), S(logY), S(w - 24), S(h - 18) };
		place(IDC_STATIC_LOG, x, logY + 12, inner, 23, service);
		place(IDC_LOG, x, logY + 40, inner, std::max(16, h - logY - 70), service);
		place(MatchTitle, x, 96, 150, 25, matches);
		place(MatchOpen, w - 258, 90, 118, 34, matches);
		place(MatchRefresh, w - 130, 90, 86, 34, matches);
		place(MatchList, x, 142, inner, h - 228, matches);
		if (matches) {
			HWND list = GetDlgItem(hwnd, MatchList);
			ListView_SetColumnWidth(list, 0, S(inner - 205));
			ListView_SetColumnWidth(list, 1, S(106));
			ListView_SetColumnWidth(list, 2, S(72));
		}
		place(MatchHint, x, h - 65, inner, 24, matches);
		place(SettingsTitle, x, 96, inner, 25, settings);
		place(AutoScroll, x, 137, inner, 27, settings);
		place(SettingsHint, x, 176, inner, 24, settings);
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

		iconFont = make(-18, FW_NORMAL, L"Segoe MDL2 Assets");
		g_font_ui    = make(-15, FW_NORMAL, L"Microsoft YaHei UI");
		g_font_small = make(-13, FW_NORMAL, L"Microsoft YaHei UI");
		g_font_sect  = make(-16, FW_SEMIBOLD, L"Microsoft YaHei UI");
		g_font_title = make(-23, FW_SEMIBOLD, L"Microsoft YaHei UI");
		g_font_stat  = make(-15, FW_NORMAL, L"Microsoft YaHei UI");
		g_font_pin   = make(-27, FW_BOLD, L"Consolas");
		g_font_mono  = make(-13, FW_NORMAL, L"Consolas");
	}

	struct ButtonMotion {
		float hover = 0, press = 0;
		bool inside = false, down = false;
		ULONGLONG lastFrame = 0;
	};
	std::map<HWND, ButtonMotion> motions;
	constexpr UINT_PTR motionTimer = 0x5354;

	COLORREF Blend(COLORREF from, COLORREF to, float weight)
	{
		auto mix = [weight](int a, int b) { return int(a + (b - a) * weight + 0.5f); };
		return RGB(mix(GetRValue(from), GetRValue(to)), mix(GetGValue(from), GetGValue(to)),
			mix(GetBValue(from), GetBValue(to)));
	}

	void AnimateButton(HWND button, ButtonMotion& motion, bool inside, bool down)
	{
		if (motion.inside == inside && motion.down == down) return;
		motion.inside = inside;
		motion.down = down;
		BOOL animate = TRUE;
		SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
		if (animate && SetTimer(button, motionTimer, 16, nullptr)) {
			// 即使两次输入落在同一帧内，轻点也有可见的按压反馈。
			if (down) motion.press = std::max(motion.press, 0.55f);
			motion.lastFrame = GetTickCount64();
		} else {
			motion.hover = inside ? 1.f : 0.f;
			motion.press = down ? 1.f : 0.f;
		}
		InvalidateRect(button, nullptr, FALSE);
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
				const float elapsed = float(now - motion.lastFrame);
				motion.lastFrame = now;
				auto advance = [elapsed](float value, bool towardsOne, float duration) {
					return towardsOne ? std::min(1.f, value + elapsed / duration)
						: std::max(0.f, value - elapsed / duration);
				};
				motion.hover = advance(motion.hover, motion.inside, 150.f);
				motion.press = advance(motion.press, motion.down, motion.down ? 65.f : 120.f);
				if (motion.hover == float(motion.inside) && motion.press == float(motion.down))
					KillTimer(button, motionTimer);
				InvalidateRect(button, nullptr, FALSE);
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

	HWND Mk(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
		int x, int y, int w, int h, int id, HFONT f, DWORD exStyle = 0)
	{
		HWND c = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style | (_wcsicmp(cls, L"BUTTON") == 0 ? WS_TABSTOP : 0),
			x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
		SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
		if (c && _wcsicmp(cls, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_OWNERDRAW) {
			auto& motion = motions[c];
			if (!SetWindowSubclass(c, ButtonInput, 1, reinterpret_cast<DWORD_PTR>(&motion)))
				motions.erase(c);
		}
		return c;
	}

	// 自绘按钮
	void DrawButton(LPDRAWITEMSTRUCT dis)
	{
		HDC dc = dis->hDC;
		RECT r = dis->rcItem;
		bool disabled = (dis->itemState & ODS_DISABLED) != 0;
		auto it = motions.find(dis->hwndItem);
		float hot = disabled || it == motions.end() ? 0.f : it->second.hover;
		float press = disabled ? 0.f : it != motions.end() ? it->second.press
			: float((dis->itemState & ODS_SELECTED) != 0);
		int inset = int(S(2) * press + 0.5f);
		int id = (int)dis->CtlID;

		if (id >= NavService && id <= NavSettings) {
			bool active = currentPage == id;
			FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
			COLORREF surface = active ? RGB(226, 238, 255) : theme::CARD;
			surface = Blend(Blend(surface, active ? RGB(211, 230, 255) : RGB(234, 244, 255), hot),
				RGB(195, 220, 255), press);
			InflateRect(&r, -inset, -inset);
			FillRoundRect(dc, r, S(7), surface, surface);
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, active ? theme::ACCENT_DN : Blend(theme::TEXT_SUB, theme::ACCENT_DN, hot));
			HGDIOBJ prev = SelectObject(dc, iconFont);
			const wchar_t glyphs[] = { 0xE968, 0xE8F1, 0xE9D9, 0xE713 };
			RECT textBox = r;
			textBox.left += S(16);
			DrawTextW(dc, &glyphs[id - NavService], 1, &textBox, DT_SINGLELINE | DT_VCENTER);
			textBox.left += S(32);
			SelectObject(dc, active ? g_font_sect : g_font_ui);
			DrawTextW(dc, pageNames[id - NavService], -1, &textBox, DT_SINGLELINE | DT_VCENTER);
			SelectObject(dc, prev);
			if (dis->itemState & ODS_FOCUS) { InflateRect(&r, -3, -3); DrawFocusRect(dc, &r); }
			return;
		}
		// 主按钮（上线 / 转换 / 合并）用主色，其余为浅色
		bool primary = (id == IDC_BTN_START || id == BankAdd);

		COLORREF bg, fg, border;
		if (disabled)
		{
			bg = RGB(0xED, 0xEF, 0xF2);
			fg = RGB(0xAE, 0xB4, 0xBE);
			border = RGB(0xE3, 0xE6, 0xEA);
		}
		else if (primary)
		{
			bg = Blend(Blend(theme::ACCENT, theme::ACCENT_HOT, hot), theme::ACCENT_DN, press);
			fg = RGB(0xFF, 0xFF, 0xFF);
			border = bg;
		}
		else
		{
			bg = Blend(Blend(theme::CARD, RGB(236, 245, 255), hot), RGB(208, 229, 255), press);
			fg = Blend(theme::TEXT, theme::ACCENT_DN, hot);
			border = Blend(theme::BORDER, RGB(138, 180, 238), hot);
		}

		// 底色（先铺满父窗口底色，再画圆角）
		HBRUSH bgBr = CreateSolidBrush(theme::CARD);
		FillRect(dc, &r, bgBr);
		DeleteObject(bgBr);

		RECT rr = r;
		InflateRect(&rr, -1 - inset, -1 - inset);
		FillRoundRect(dc, rr, S(6), bg, border);

		// 文字
		wchar_t text[128] = { 0 };
		GetWindowTextW(dis->hwndItem, text, 128);

		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, fg);
		HFONT old = (HFONT)SelectObject(dc, g_font_ui);
		OffsetRect(&rr, 0, int(S(1) * press + 0.5f));
		DrawTextW(dc, text, -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		SelectObject(dc, old);

		if (dis->itemState & ODS_FOCUS)
		{
			RECT fr = rr;
			InflateRect(&fr, -3, -3);
			DrawFocusRect(dc, &fr);
		}
	}

	// 画卡片背景与描边
	void DrawCard(HWND hwnd)
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);

		RECT rc;
		GetClientRect(hwnd, &rc);
		HBRUSH bg = CreateSolidBrush(theme::BG);
		FillRect(dc, &rc, bg);
		DeleteObject(bg);

		RECT sidebar = { 0, 0, S(174), rc.bottom };
		FillRect(dc, &sidebar, (HBRUSH)GetStockObject(WHITE_BRUSH));
		HBRUSH line = CreateSolidBrush(theme::BORDER);
		RECT divider = { sidebar.right - 1, 0, sidebar.right, rc.bottom };
		FillRect(dc, &divider, line);
		DeleteObject(line);
		FillRoundRect(dc, g_panel, S(9), theme::CARD, theme::BORDER);
		if (currentPage == NavService)
			FillRoundRect(dc, g_card_log, S(9), theme::CARD, theme::BORDER);

		EndPaint(hwnd, &ps);
	}

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_CREATE:
		{
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
			g_status = Mk(hwnd, L"STATIC", L"○ 已下线", SS_RIGHT | SS_CENTERIMAGE,
				0, 0, 0, 0, IDC_STATUS, g_font_sect);

			// ---- 服务状态卡片 ----
			Mk(hwnd, L"STATIC", L"服务状态", SS_LEFT, 0, 0, 0, 0, IDC_STATIC_SVC, g_font_sect);
			Mk(hwnd, L"STATIC", L"本机", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_URL1, g_font_ui);
			Mk(hwnd, L"STATIC", L"局域网", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_URL2, g_font_ui);
			Mk(hwnd, L"STATIC", L"登录 PIN", SS_LEFT | SS_CENTERIMAGE, 0, 0, 0, 0, IDC_STATIC_PIN, g_font_ui);

			g_url_local = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_AUTOHSCROLL,
				0, 0, 0, 0, IDC_URL_LOCAL, g_font_ui);
			g_url_lan = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_AUTOHSCROLL,
				0, 0, 0, 0, IDC_URL_LAN, g_font_ui);
			g_pin = Mk(hwnd, L"EDIT", L"—", ES_READONLY | ES_CENTER,
				0, 0, 0, 0, IDC_LOGIN_PIN, g_font_pin, WS_EX_STATICEDGE);

			g_btn_start = Mk(hwnd, L"BUTTON", L"上线", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_START, g_font_ui);
			g_btn_stop = Mk(hwnd, L"BUTTON", L"下线", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_STOP, g_font_ui);
			g_btn_browser = Mk(hwnd, L"BUTTON", L"打开浏览器", BS_OWNERDRAW, 0, 0, 0, 0, IDC_BTN_BROWSER, g_font_ui);

			g_hint = Mk(hwnd, L"STATIC", L"点击「上线」后，前端电脑即可连接。", SS_LEFT,
				0, 0, 0, 0, IDC_HINT, g_font_small);

			// ---- 题库管理卡片 ----
			Mk(hwnd, L"STATIC", L"题目列表", SS_LEFT, 0, 0, 0, 0, IDC_STATIC_PB1, g_font_sect);
			Mk(hwnd, L"STATIC", L"搜索", SS_LEFT, 0, 0, 0, 0, BankSearchLabel, g_font_ui);
			HWND search = Mk(hwnd, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL,
				0, 0, 0, 0, BankSearch, g_font_ui, WS_EX_CLIENTEDGE);
			SendMessageW(search, EM_SETCUEBANNER, FALSE, (LPARAM)L"题号、题干、答案、选项或线索");
			Mk(hwnd, L"BUTTON", L"清空", BS_OWNERDRAW, 0, 0, 0, 0, BankClearSearch, g_font_ui);
			HWND filter = Mk(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
				0, 0, 0, 0, BankFilter, g_font_ui);
			SendMessageW(filter, CB_ADDSTRING, 0, (LPARAM)L"全部题型");
			for (auto name : typeNames) SendMessageW(filter, CB_ADDSTRING, 0, (LPARAM)name);
			SendMessageW(filter, CB_SETCURSEL, 0, 0);
			Mk(hwnd, L"BUTTON", L"编辑选中题", BS_OWNERDRAW, 0, 0, 0, 0, BankEdit, g_font_ui);
			EnableWindow(GetDlgItem(hwnd, BankEdit), FALSE);
			Mk(hwnd, L"BUTTON", L"新增题目", BS_OWNERDRAW, 0, 0, 0, 0, BankAdd, g_font_ui);
			HWND bank = Mk(hwnd, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
				0, 0, 0, 0, BankList, g_font_ui);
			ListView_SetExtendedListViewStyle(bank, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
			SetWindowTheme(bank, L"Explorer", nullptr);
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
			mmi->ptMinTrackSize.x = S(720);
			mmi->ptMinTrackSize.y = S(520);
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
			else if (ctl == g_status)
			{
				if (server_core::is_running())
					SetTextColor(dc, theme::ON);
				else
					SetTextColor(dc, server_core::last_error().empty() ? theme::OFF : theme::WARN);
			}
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
			if (ctlId == IDC_TITLE || ctl == g_status) {
				static HBRUSH header = CreateSolidBrush(theme::BG);
				return (LRESULT)header;
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
				SetBkColor(dc, theme::CARD);
				SetTextColor(dc, theme::TEXT);
				static HBRUSH logBr = CreateSolidBrush(theme::CARD);
				return (LRESULT)logBr;
			}
			if (ctl == g_pin)
			{
				// PIN 用主色突出，方便一眼读到
				SetBkColor(dc, theme::FIELD);
				SetTextColor(dc, theme::ACCENT);
				static HBRUSH pinBr = CreateSolidBrush(theme::FIELD);
				return (LRESULT)pinBr;
			}
			// 访问地址等只读框：深色文字、浅灰底
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
				if (HIWORD(wp) == CBN_SELCHANGE && !g_op_running) RefreshBank(hwnd);
				return 0;
			case BankSearch:
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

	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
	InitCommonControlsEx(&icc);

	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(theme::BG);
	wc.lpszClassName = L"ResponderManagerWnd";
	wc.hIcon = (HICON)LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
	wc.hIconSm = wc.hIcon;
	if (!RegisterClassExW(&wc))
	{
		MessageBoxW(nullptr, L"窗口类注册失败。", L"错误", MB_ICONERROR);
		return 1;
	}

	// 屏幕居中。窗口含标题栏，客户区按缩放后的尺寸预留
	const int clientW = S(940), clientH = S(620);
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
		MessageBoxW(nullptr, L"窗口创建失败。", L"错误", MB_ICONERROR);
		return 1;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	// 消息循环：100ms 超时轮询，刷新日志与服务状态
	MSG msg;
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
			if (msg.message == WM_QUIT)
				return (int)msg.wParam;
			if (msg.message == WM_KEYDOWN && (msg.wParam == VK_TAB || msg.wParam == VK_RETURN) && IsDialogMessageW(hwnd, &msg)) continue;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
}
