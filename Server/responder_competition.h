// responder_competition.h : Responder 比赛类的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_COMPETITION_H
#define RESPONDER_COMPETITION_H

#include <string>
#include <fstream>
#include <iostream>
#include "json.hpp"
#include "responder_problem.h"

using json = nlohmann::json;

std::vector<std::string> load_competition_list(std::string filename); // return guid list
void save_competition_list(std::string filename, const std::vector<std::string>& guid_list);

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
	} competition_status;

	int choice_count;
	int completion_count;
	int info_count;

	problem_list problems;        // 已使用的题目

	void load(std::string guid);
	void save();

	static std::string generate_guid();
};

#endif // RESPONDER_COMPETITION_H

