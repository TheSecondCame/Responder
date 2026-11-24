// responder_competition.h : Responder 比赛类的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_competition.h"
#include <winsock2.h>
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "ole32.lib")

namespace
{
	std::mutex g_comp_mutex_map_lock;
	std::unordered_map<std::string, std::shared_ptr<std::mutex>> g_comp_mutexes;

	std::string session_file_path(const std::string& guid)
	{
		return "data/competitions/" + guid + "/session.json";
	}

	int read_int(const json& j, const char* key, int fallback)
	{
		if (j.contains(key) && j[key].is_number_integer())
			return j[key].get<int>();
		return fallback;
	}

	std::int64_t read_int64(const json& j, const char* key, std::int64_t fallback)
	{
		if (j.contains(key) && j[key].is_number_integer())
			return j[key].get<std::int64_t>();
		return fallback;
	}

	bool read_bool(const json& j, const char* key, bool fallback)
	{
		if (j.contains(key) && j[key].is_boolean())
			return j[key].get<bool>();
		if (j.contains(key) && j[key].is_number_integer())
			return j[key].get<int>() != 0;
		return fallback;
	}

	std::string read_string(const json& j, const char* key, const std::string& fallback)
	{
		if (j.contains(key) && j[key].is_string())
			return j[key].get<std::string>();
		return fallback;
	}

	void normalize_team_vectors(competition_session& s, size_t team_count)
	{
		if (team_count == 0)
		{
			s.early_warning_counts.clear();
			s.team_order.clear();
			return;
		}

		if (s.early_warning_counts.size() != team_count)
			s.early_warning_counts.assign(team_count, 0);

		bool order_valid = (s.team_order.size() == team_count);
		if (order_valid)
		{
			std::vector<bool> seen(team_count, false);
			for (int idx : s.team_order)
			{
				if (idx < 0 || static_cast<size_t>(idx) >= team_count || seen[idx])
				{
					order_valid = false;
					break;
				}
				seen[idx] = true;
			}
		}
		if (!order_valid)
		{
			s.team_order.resize(team_count);
			for (size_t i = 0; i < team_count; ++i)
				s.team_order[i] = static_cast<int>(i);
		}
	}

	bool load_session_unlocked(competition_session& s, const std::string& guid, size_t team_count)
	{
		s.reset_defaults(team_count);
		if (guid.empty())
			return false;

		std::ifstream fin(session_file_path(guid));
		if (fin.fail())
			return false;

		json jsbuf = json::parse(fin, nullptr, false);
		fin.close();
		if (!jsbuf.is_object())
			return false;

		s.from_json(jsbuf, team_count);
		return true;
	}

	bool save_session_unlocked(const competition_session& s, const std::string& guid)
	{
		if (guid.empty())
			return false;

		CreateDirectoryA("data", NULL);
		CreateDirectoryA("data/competitions", NULL);
		CreateDirectoryA(("data/competitions/" + guid).c_str(), NULL);

		const std::string dst = session_file_path(guid);
		const std::string tmp = dst + ".tmp";
		try
		{
			std::ofstream fout;
			fout.exceptions(std::ios::badbit | std::ios::failbit);
			fout.open(tmp, std::ios::binary | std::ios::trunc);
			fout << s.to_json().dump();
			fout.close();

			if (!MoveFileExA(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				DeleteFileA(tmp.c_str());
				return false;
			}
			return true;
		}
		catch (...)
		{
			DeleteFileA(tmp.c_str());
			return false;
		}
	}
}

void competition_session::reset_defaults(size_t team_count)
{
	current_type = -1;
	current_id = 0;
	shown_at = 0;

	round_team = static_cast<int>(team_count);
	round_team_cnt = 0;
	round_count = 0;
	team_problems_count = 0;
	round_answering = false;
	round_delayed = false;

	round_flow_running = false;
	responder_flow_running = false;
	responding = false;
	early_responding = false;
	selected_team = 0;

	mode = "responder";
	timer_started_at = 0;
	timer_duration = 0;
	timer_mode = 0;
	timer_paused = false;
	timer_remaining = 0;

	auto_scoring = true;
	auto_complexity = true;
	winner_takes_all = true;
	early_responder_warning = true;
	early_warning_trigger_count = 3;
	early_warning_sub_value = 1;
	early_warning_counts.assign(team_count, 0);
	auto_add_value = 1;
	auto_sub_value = 1;
	time_before = 3;
	time_after = 15;
	default_time = 15;
	target_complexity = 0;

	problem_number = 0;
	info_id = 0;
	show_answer = false;
	discarded = false;
	confirm_pending = false;
	default_type = 0;
	team_order.resize(team_count);
	for (size_t i = 0; i < team_count; ++i)
		team_order[i] = static_cast<int>(i);
}

json competition_session::to_json() const
{
	json j;
	j["current_type"] = current_type;
	j["current_id"] = current_id;
	j["shown_at"] = shown_at;

	j["round_team"] = round_team;
	j["round_team_cnt"] = round_team_cnt;
	j["round_count"] = round_count;
	j["team_problems_count"] = team_problems_count;
	j["round_answering"] = round_answering;
	j["round_delayed"] = round_delayed;

	j["round_flow_running"] = round_flow_running;
	j["responder_flow_running"] = responder_flow_running;
	j["responding"] = responding;
	j["early_responding"] = early_responding;
	j["selected_team"] = selected_team;

	j["mode"] = mode;
	j["timer_started_at"] = timer_started_at;
	j["timer_duration"] = timer_duration;
	j["timer_mode"] = timer_mode;
	j["timer_paused"] = timer_paused;
	j["timer_remaining"] = timer_remaining;

	j["auto_scoring"] = auto_scoring;
	j["auto_complexity"] = auto_complexity;
	j["winner_takes_all"] = winner_takes_all;
	j["early_responder_warning"] = early_responder_warning;
	j["early_warning_trigger_count"] = early_warning_trigger_count;
	j["early_warning_sub_value"] = early_warning_sub_value;
	j["early_warning_counts"] = early_warning_counts;
	j["auto_add_value"] = auto_add_value;
	j["auto_sub_value"] = auto_sub_value;
	j["time_before"] = time_before;
	j["time_after"] = time_after;
	j["default_time"] = default_time;
	j["target_complexity"] = target_complexity;

	j["problem_number"] = problem_number;
	j["info_id"] = info_id;
	j["show_answer"] = show_answer;
	j["discarded"] = discarded;
	j["confirm_pending"] = confirm_pending;
	j["default_type"] = default_type;
	j["team_order"] = team_order;
	return j;
}

void competition_session::apply_client_state(const json& j, size_t team_count)
{
	if (!j.is_object())
		return;

	round_team = read_int(j, "round_team", round_team);
	round_team_cnt = std::max(0, read_int(j, "round_team_cnt", round_team_cnt));
	round_count = std::max(0, read_int(j, "round_count", round_count));
	team_problems_count = std::max(0, read_int(j, "team_problems_count", team_problems_count));
	round_answering = read_bool(j, "round_answering", round_answering);
	round_delayed = read_bool(j, "round_delayed", round_delayed);

	round_flow_running = read_bool(j, "round_flow_running", round_flow_running);
	responder_flow_running = read_bool(j, "responder_flow_running", responder_flow_running);
	responding = read_bool(j, "responding", responding);
	early_responding = read_bool(j, "early_responding", early_responding);
	selected_team = std::max(0, read_int(j, "selected_team", selected_team));
	if (team_count > 0 && static_cast<size_t>(selected_team) > team_count)
		selected_team = 0;

	std::string m = read_string(j, "mode", mode);
	if (m == "responder" || m == "round" || m == "timer")
		mode = m;

	auto_scoring = read_bool(j, "auto_scoring", auto_scoring);
	auto_complexity = read_bool(j, "auto_complexity", auto_complexity);
	winner_takes_all = read_bool(j, "winner_takes_all", winner_takes_all);
	early_responder_warning = read_bool(j, "early_responder_warning", early_responder_warning);
	early_warning_trigger_count = std::max(0, read_int(j, "early_warning_trigger_count", early_warning_trigger_count));
	early_warning_sub_value = std::max(0, read_int(j, "early_warning_sub_value", early_warning_sub_value));
	if (j.contains("early_warning_counts") && j["early_warning_counts"].is_array())
	{
		std::vector<int> counts;
		for (const auto& item : j["early_warning_counts"])
		{
			if (item.is_number_integer())
				counts.push_back(std::max(0, item.get<int>()));
		}
		if (counts.size() == team_count)
			early_warning_counts = std::move(counts);
	}
	auto_add_value = std::max(0, read_int(j, "auto_add_value", auto_add_value));
	auto_sub_value = std::max(0, read_int(j, "auto_sub_value", auto_sub_value));
	time_before = std::max(0, read_int(j, "time_before", time_before));
	time_after = std::max(0, read_int(j, "time_after", time_after));
	default_time = std::max(1, read_int(j, "default_time", default_time));
	target_complexity = std::clamp(read_int(j, "target_complexity", target_complexity), 0, 3);

	problem_number = std::max(0, read_int(j, "problem_number", problem_number));
	info_id = std::clamp(read_int(j, "info_id", info_id), 0, 4);
	show_answer = read_bool(j, "show_answer", show_answer);
	discarded = read_bool(j, "discarded", discarded);
	confirm_pending = read_bool(j, "confirm_pending", confirm_pending);
	default_type = std::clamp(read_int(j, "default_type", default_type), 0, 2);

	if (j.contains("team_order") && j["team_order"].is_array())
	{
		std::vector<int> order;
		for (const auto& item : j["team_order"])
		{
			if (item.is_number_integer())
				order.push_back(item.get<int>());
		}
		if (order.size() == team_count)
			team_order = std::move(order);
	}

	normalize_team_vectors(*this, team_count);
}

void competition_session::from_json(const json& j, size_t team_count)
{
	reset_defaults(team_count);
	if (!j.is_object())
		return;

	int t = read_int(j, "current_type", -1);
	current_type = (t >= 0 && t <= 2) ? t : -1;
	current_id = std::max(0, read_int(j, "current_id", 0));
	shown_at = std::max<std::int64_t>(0, read_int64(j, "shown_at", 0));

	timer_started_at = std::max<std::int64_t>(0, read_int64(j, "timer_started_at", 0));
	timer_duration = std::max(0, read_int(j, "timer_duration", 0));
	timer_mode = std::clamp(read_int(j, "timer_mode", 0), 0, 2);
	timer_paused = read_bool(j, "timer_paused", false);
	timer_remaining = std::max(0, read_int(j, "timer_remaining", 0));

	apply_client_state(j, team_count);
}

bool competition_session::load(const std::string& guid, size_t team_count)
{
	std::lock_guard<std::mutex> lock(competition_session_mutex);
	return load_session_unlocked(*this, guid, team_count);
}

bool competition_session::save(const std::string& guid) const
{
	std::lock_guard<std::mutex> lock(competition_session_mutex);
	return save_session_unlocked(*this, guid);
}

bool competition_session::update(const std::string& guid, size_t team_count,
	const std::function<void(competition_session&, bool)>& fn)
{
	std::lock_guard<std::mutex> lock(competition_session_mutex);
	competition_session s;
	bool existed = load_session_unlocked(s, guid, team_count);
	if (fn)
		fn(s, existed);
	return save_session_unlocked(s, guid);
}

void competition_session::clear(const std::string& guid)
{
	if (guid.empty())
		return;
	std::lock_guard<std::mutex> lock(competition_session_mutex);
	DeleteFileA(session_file_path(guid).c_str());
	DeleteFileA((session_file_path(guid) + ".tmp").c_str());
}

std::shared_ptr<std::mutex> get_competition_mutex(const std::string& guid)
{
	std::lock_guard<std::mutex> map_lock(g_comp_mutex_map_lock);
	auto& ptr = g_comp_mutexes[guid];
	if (!ptr)
		ptr = std::make_shared<std::mutex>();
	return ptr;
}

namespace
{
	std::mutex g_comp_list_mutex;

	bool try_parse_score_line(const std::string& raw, int& out)
	{
		size_t start = 0;
		size_t end = raw.size();
		while (start < end && (raw[start] == ' ' || raw[start] == '\t' || raw[start] == '\r'))
			++start;
		while (end > start && (raw[end - 1] == ' ' || raw[end - 1] == '\t' || raw[end - 1] == '\r'))
			--end;
		if (start == end)
			return false;
		const std::string trimmed = raw.substr(start, end - start);
		try
		{
			size_t idx = 0;
			int val = std::stoi(trimmed, &idx);
			if (idx != trimmed.size())
				return false;
			out = val;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
}

bool is_valid_competition_guid(const std::string& guid)
{
	if (guid.empty() || guid.size() > 128)
		return false;
	for (unsigned char ch : guid)
	{
		if (!((ch >= '0' && ch <= '9') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= 'a' && ch <= 'z') ||
			ch == '-' || ch == '_'))
		{
			return false;
		}
	}
	return true;
}

const char* competition_status_name(competition::status s)
{
	switch (s)
	{
	case competition::NOT_STARTED: return "NOT_STARTED";
	case competition::ONGOING: return "ONGOING";
	case competition::PAUSED: return "PAUSED";
	case competition::FINISHED: return "FINISHED";
	default: return "UNKNOWN";
	}
}

std::vector<std::string> load_competition_list(std::string filename)
{
	std::lock_guard<std::mutex> list_lock(g_comp_list_mutex);
	std::ifstream fin(filename);
	if (fin.fail())
	{
		std::cout << "Failed to load competition list from " << filename << std::endl;
		return std::vector<std::string>();
	}

	json jsbuf = json::parse(fin, nullptr, false);
	fin.close();
	if (!jsbuf.is_object() || !jsbuf.contains("competitions") || !jsbuf["competitions"].is_array())
	{
		std::cout << "[WARN] Invalid competition list JSON in " << filename << std::endl;
		return std::vector<std::string>();
	}

	std::vector<std::string> guid_list;
	for (const auto& item : jsbuf["competitions"])
	{
		if (item.is_string())
			guid_list.push_back(item.get<std::string>());
	}

	return guid_list;
}

void save_competition_list(std::string filename, const std::vector<std::string>& guid_list)
{
	std::lock_guard<std::mutex> list_lock(g_comp_list_mutex);
	CreateDirectoryA("data", NULL);
	CreateDirectoryA("data/competitions", NULL);

	json jsbuf;
	jsbuf["competitions"] = guid_list;

	const std::string tmp = filename + ".tmp";
	try
	{
		std::ofstream fout;
		fout.exceptions(std::ios::badbit | std::ios::failbit);
		fout.open(tmp, std::ios::binary | std::ios::trunc);
		fout << jsbuf.dump();
		fout.close();

		if (!MoveFileExA(tmp.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::cout << "[ERROR] Failed to replace competition list " << filename << std::endl;
			DeleteFileA(tmp.c_str());
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "[ERROR] Failed to write competition list " << filename << ": " << e.what() << std::endl;
		DeleteFileA(tmp.c_str());
	}
}

void competition::load(std::string _guid)
{
	guid = _guid;
	teams.clear();
	team_scores.clear();
	last_problem_type = -1;
	last_problem_id = 0;

	if (!is_valid_competition_guid(guid))
		return;

	std::string filename = "data/competitions/" + guid + "/competition.json";

	std::ifstream fin(filename);
	if (fin.fail())
	{
		std::cout << "Failed to load competition from " << filename << std::endl;
		return;
	}

	json jsbuf = json::parse(fin, nullptr, false);
	fin.close();
	if (!jsbuf.is_object())
	{
		std::cout << "[WARN] Invalid competition JSON in " << filename << std::endl;
		return;
	}

	name = read_string(jsbuf, "name", "");
	description = read_string(jsbuf, "description", "");
	if (jsbuf.contains("teams") && jsbuf["teams"].is_array())
		jsbuf["teams"].get_to(teams);
	int raw_status = read_int(jsbuf, "competition_status", static_cast<int>(status::NOT_STARTED));
	if (raw_status >= static_cast<int>(status::NOT_STARTED) && raw_status <= static_cast<int>(status::FINISHED))
		competition_status = static_cast<status>(raw_status);
	else
		competition_status = status::NOT_STARTED;
	choice_count = read_int(jsbuf, "choice_count", 0);
	completion_count = read_int(jsbuf, "completion_count", 0);
	info_count = read_int(jsbuf, "info_count", 0);
	last_problem_type = read_int(jsbuf, "last_problem_type", -1);
	last_problem_id = read_int(jsbuf, "last_problem_id", 0);

	problems.load("data/competitions/" + guid + "/problems.json");

	if (competition_status >= status::ONGOING)
	{
		const std::string scores_file = "data/competitions/" + guid + "/scores.json";
		fin.open(scores_file);
		if (!fin.fail())
		{
			std::string line;
			for (size_t i = 0; i < teams.size(); ++i)
			{
				if (!std::getline(fin, line))
				{
					std::cout << "[WARN] Truncated scores.json for competition " << guid
						<< " at team index " << i << "; defaulting remaining scores to 0." << std::endl;
					while (team_scores.size() < teams.size())
						team_scores.push_back(0);
					break;
				}
				int parsed_score = 0;
				if (!try_parse_score_line(line, parsed_score))
				{
					std::cout << "[WARN] Invalid score entry in " << scores_file
						<< " at line " << (i + 1) << ": \"" << line << "\"; defaulting to 0." << std::endl;
					parsed_score = 0;
				}
				team_scores.push_back(parsed_score);
			}
			fin.close();
			return;
		}
		fin.close();
	}

	for (size_t i = 0; i < teams.size(); ++i)
		team_scores.push_back(0);
}

void competition::save()
{
	if (!is_valid_competition_guid(guid))
		return;

	CreateDirectoryA("data", NULL);
	CreateDirectoryA("data/competitions", NULL);
	CreateDirectoryA(("data/competitions/" + guid + "/").c_str(), NULL);

	json jsbuf;
	jsbuf["name"] = name;
	jsbuf["description"] = description;
	jsbuf["teams"] = teams;
	jsbuf["competition_status"] = competition_status;
	jsbuf["choice_count"] = choice_count;
	jsbuf["completion_count"] = completion_count;
	jsbuf["info_count"] = info_count;
	if (last_problem_id > 0 && last_problem_type >= 0 && last_problem_type <= 2)
	{
		jsbuf["last_problem_type"] = last_problem_type;
		jsbuf["last_problem_id"] = last_problem_id;
	}

	problems.save("data/competitions/" + guid + "/problems.json");

	const std::string dst = "data/competitions/" + guid + "/competition.json";
	const std::string tmp = dst + ".tmp";
	try
	{
		std::ofstream fout;
		fout.exceptions(std::ios::badbit | std::ios::failbit);
		fout.open(tmp, std::ios::binary | std::ios::trunc);
		fout << jsbuf.dump();
		fout.close();

		if (!MoveFileExA(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			DeleteFileA(tmp.c_str());
	}
	catch (...)
	{
		DeleteFileA(tmp.c_str());
	}
}

bool competition::save_scores() const
{
	if (!is_valid_competition_guid(guid))
		return false;

	CreateDirectoryA("data", NULL);
	CreateDirectoryA("data/competitions", NULL);
	CreateDirectoryA(("data/competitions/" + guid + "/").c_str(), NULL);

	const std::string dst = "data/competitions/" + guid + "/scores.json";
	const std::string tmp = dst + ".tmp";
	try
	{
		std::ofstream fout;
		fout.exceptions(std::ios::badbit | std::ios::failbit);
		fout.open(tmp, std::ios::binary | std::ios::trunc);
		for (int score : team_scores)
			fout << score << '\n';
		fout.close();

		if (!MoveFileExA(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			DeleteFileA(tmp.c_str());
			return false;
		}
		return true;
	}
	catch (...)
	{
		DeleteFileA(tmp.c_str());
		return false;
	}
}

std::string competition::generate_guid()
{
	GUID guid;
	CoCreateGuid(&guid);
	char buf[64] = { 0 };
	sprintf_s(buf, sizeof(buf),
		"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return std::string(buf);
}
