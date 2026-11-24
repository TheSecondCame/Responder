// responder_competition.h : Responder 比赛类的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_COMPETITION_H
#define RESPONDER_COMPETITION_H

#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include "json.hpp"
#include "responder_problem.h"

using json = nlohmann::json;

inline std::mutex competition_session_mutex;
std::shared_ptr<std::mutex> get_competition_mutex(const std::string& guid);

std::vector<std::string> load_competition_list(std::string filename); // return guid list
void save_competition_list(std::string filename, const std::vector<std::string>& guid_list);

struct competition_session
{
	int current_type = -1;            // 0=选择, 1=填空, 2=信息, -1=无当前题
	int current_id = 0;               // 当前题目 ID (>0 表示有效)
	std::int64_t shown_at = 0;        // 抽题时间戳 (Unix 秒)

	int round_team = 0;               // 轮答当前队 (1-based)
	int round_team_cnt = 0;           // 轮答当前位次 (1-based)
	int round_count = 0;              // 当前轮次
	int team_problems_count = 0;      // 赢者通吃模式当前队已答题数
	bool round_answering = false;     // 当前轮答是否已展示题目
	bool round_delayed = false;       // 是否因弃题/中断保留当前位次

	bool round_flow_running = false;
	bool responder_flow_running = false;
	bool responding = false;
	bool early_responding = false;
	int selected_team = 0;            // 当前选中/抢答队伍 (1-based, 0=未选中)

	std::string mode = "responder";   // "responder" | "round" | "timer"
	std::int64_t timer_started_at = 0;// 服务器端记录的计时起点 (Unix 秒)
	int timer_duration = 0;           // 本次倒计时秒数
	int timer_mode = 0;               // IntervalMode: 0=常规, 1=抢答前, 2=轮答前
	bool timer_paused = false;        // 是否已暂停
	int timer_remaining = 0;          // 暂停时剩余秒数

	bool auto_scoring = true;
	bool auto_complexity = true;
	bool winner_takes_all = true;
	bool early_responder_warning = true;
	int early_warning_trigger_count = 3;
	int early_warning_sub_value = 1;
	std::vector<int> early_warning_counts;
	int auto_add_value = 1;
	int auto_sub_value = 1;
	int time_before = 3;
	int time_after = 15;
	int default_time = 15;
	int target_complexity = 0;

	int problem_number = 0;           // 页面显示的题号 ("第X题")
	int info_id = 0;                  // 信息题已展示线索数 (0..4)
	bool show_answer = false;         // 是否已显示答案
	bool discarded = false;           // 当前题目是否已弃用
	bool confirm_pending = false;     // 自动计分确认提示是否待处理
	int default_type = 0;             // 题型设置选中的下一题型
	std::vector<int> team_order;      // 队伍展示顺序 (原始下标 0..N-1)

	void reset_defaults(size_t team_count);
	json to_json() const;
	void from_json(const json& j, size_t team_count);
	void apply_client_state(const json& j, size_t team_count);

	bool load(const std::string& guid, size_t team_count);
	bool save(const std::string& guid) const;
	static bool update(const std::string& guid, size_t team_count,
		const std::function<void(competition_session&, bool)>& fn);
	static void clear(const std::string& guid);
};

class competition
{
public:
	std::string guid;
	std::string name;
	std::string description;
	std::vector<std::pair<std::string, char>> teams; // pair<team_name, key>
	std::vector<int> team_scores; // 与 teams 一一对应，记录当前分数

	enum status
	{
		NOT_STARTED,
		ONGOING,
		PAUSED,
		FINISHED
	} competition_status = NOT_STARTED;

	int choice_count = 0;
	int completion_count = 0;
	int info_count = 0;
	int last_problem_type = -1;
	int last_problem_id = 0;

	problem_list problems;        // 已使用的题目

	void load(std::string guid);
	void save();
	bool save_scores() const;

	static std::string generate_guid();
};

bool is_valid_competition_guid(const std::string& guid);
const char* competition_status_name(competition::status s);

#endif // RESPONDER_COMPETITION_H

