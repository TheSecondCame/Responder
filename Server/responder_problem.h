// responder_problem.h : Responder 题目类的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RESPONDER_PROBLEM_H
#define RESPONDER_PROBLEM_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

// Shared by the live draw pool, index and desktop/browser bank operations.
inline std::mutex problem_bank_mutex;

class problem_completion
{
public:
	int id;
	std::string content;
	std::string answer;
	int complexity; // 1-3, 1 for easiest, 3 for hardest, 0 for undefined
	virtual void load(std::string filename, std::ostream& log = std::cout);
	virtual void save(std::string filename);
};

class problem_choice : public problem_completion
{
public:
	std::vector<std::string> options;
	void load(std::string filename, std::ostream& log = std::cout) override;
	void save(std::string filename) override;
};

class problem_info : public problem_completion
{
public:
	std::vector<std::string> info;
	void load(std::string filename, std::ostream& log = std::cout) override;
	void save(std::string filename) override;
};

class problem_list
{
public:
	std::vector<std::pair<problem_choice, int>> choice_problems;
	std::vector<std::pair<problem_completion, int>> completion_problems;
	std::vector<std::pair<problem_info, int>> info_problems;   // int: 0 for unused, 1 for used
	void load(std::string filename, std::ostream& log = std::cout);
	void save(std::string filename);

	void delete_duplicates(); // 删除重复题目（根据题目内容和选项/信息判断）

	template <typename T>
	static int count_unused(std::vector<std::pair<T, int>>& problems)
	{
		int ans = 0;
		for (const auto& problem : problems)
			ans += (problem.second == 0);
		return ans;
	}
	template <typename T>
	static int count_unused(std::vector<std::pair<T, int>>& problems, int complexity)
	{
		int ans = 0;
		for (const auto& problem : problems)
			ans += (problem.second == 0 && problem.first.complexity == complexity);
		return ans;
	}

	static void convert(std::string filename_in, std::string folder_out, std::ostream& log = std::cout);
	static void merge(std::string filename_in, std::string folder_out, std::ostream& log = std::cout);

private:
	template <typename T>
	static void _remove_used(std::vector<std::pair<T, int>>& problems)
	{
		for (int i = 0; i < problems.size(); i++)
			if (problems[i].second == 1)
			{
				problems.erase(problems.begin() + i);
				i--;
			}
	}

public:
	void remove_used()
	{
		_remove_used(choice_problems);
		_remove_used(completion_problems);
		_remove_used(info_problems);
	}
};

class responder_problem_output_handler : public problem_list
{
public:
	void output_to_folder(std::string output_folder);
};

#endif // RESPONDER_PROBLEM_H

