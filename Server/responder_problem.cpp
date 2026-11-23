// responder_problem.cpp : Responder 题目类的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_problem.h"
#include "functions.h"
#include <Windows.h>

// Report write failures to callers before they publish a new index or bank.
static void write_problem_json(const std::string& filename, const json& record)
{
	std::ofstream out;
	out.exceptions(std::ios::badbit | std::ios::failbit);
	out.open(filename);
	out << record.dump();
	out.close();
}

void problem_completion::load(std::string filename, std::ostream& log)
{
	std::ifstream fin(filename);
	if (fin.fail())
	{
		log << "Failed to load problem from " << filename << std::endl;
		// TODO: MessageBox
		return;
	}

	json jsbuf = json::parse(fin);
	fin.close();

	jsbuf["id"].get_to(id);
	jsbuf["content"].get_to(content);
	jsbuf["answer"].get_to(answer);
	jsbuf["complexity"].get_to(complexity);
}
void problem_completion::save(std::string filename)
{
	json jsbuf;
	jsbuf["id"] = id;
	jsbuf["content"] = content;
	jsbuf["answer"] = answer;
	jsbuf["complexity"] = complexity;

	write_problem_json(filename, jsbuf);
}

void problem_choice::load(std::string filename, std::ostream& log)
{
	std::ifstream fin(filename);
	if (fin.fail())
	{
		log << "Failed to load problem from " << filename << std::endl;
		// TODO: MessageBox
		return;
	}

	json jsbuf = json::parse(fin);
	fin.close();

	jsbuf["id"].get_to(id);
	jsbuf["content"].get_to(content);
	jsbuf["answer"].get_to(answer);
	jsbuf["options"].get_to(options);
	jsbuf["complexity"].get_to(complexity);
}
void problem_choice::save(std::string filename)
{
	json jsbuf;
	jsbuf["id"] = id;
	jsbuf["content"] = content;
	jsbuf["answer"] = answer;
	jsbuf["options"] = options;
	jsbuf["complexity"] = complexity;

	write_problem_json(filename, jsbuf);
}

void problem_info::load(std::string filename, std::ostream& log)
{
	std::ifstream fin(filename);
	if (fin.fail())
	{
		log << "Failed to load problem from " << filename << std::endl;
		// TODO: MessageBox
		return;
	}

	json jsbuf = json::parse(fin);
	fin.close();

	jsbuf["id"].get_to(id);
	jsbuf["content"].get_to(content);
	jsbuf["answer"].get_to(answer);
	jsbuf["info"].get_to(info);
	jsbuf["complexity"].get_to(complexity);
}
void problem_info::save(std::string filename)
{
	json jsbuf;
	jsbuf["id"] = id;
	jsbuf["content"] = content;
	jsbuf["answer"] = answer;
	jsbuf["info"] = info;
	jsbuf["complexity"] = complexity;

	write_problem_json(filename, jsbuf);
}

void problem_list::load(std::string filename, std::ostream& log)
{
	std::ifstream fin(filename);
	if (fin.fail())
	{
		log << "Failed to load problem list from " << filename << std::endl;
		// TODO: MessageBox
		return;
	}

	json jsbuf = json::parse(fin);
	fin.close();

	std::vector<std::pair<int, int>> _list;
	jsbuf["choice_problems"].get_to(_list);
	for (const auto& choice : _list)
	{
		choice_problems.push_back(std::make_pair(problem_choice(), choice.second));
		choice_problems.back().first.load("data/problems/choice_" + std::to_string(choice.first) + ".json", log);
	}

	jsbuf["completion_problems"].get_to(_list);
	for (const auto& completion : _list)
	{
		completion_problems.push_back(std::make_pair(problem_completion(), completion.second));
		completion_problems.back().first.load("data/problems/completion_" + std::to_string(completion.first) + ".json", log);
	}

	jsbuf["info_problems"].get_to(_list);
	for (const auto& info : _list)
	{
		info_problems.push_back(std::make_pair(problem_info(), info.second));
		info_problems.back().first.load("data/problems/info_" + std::to_string(info.first) + ".json", log);
	}
}
void problem_list::save(std::string filename)
{
	json jsbuf;
	std::vector<std::pair<int, int>> _list;

	for (const auto& choice : choice_problems)
		_list.push_back(std::make_pair(choice.first.id, choice.second));
	jsbuf["choice_problems"] = _list;
	_list.clear();

	for (const auto& completion : completion_problems)
		_list.push_back(std::make_pair(completion.first.id, completion.second));
	jsbuf["completion_problems"] = _list;
	_list.clear();

	for (const auto& info : info_problems)
		_list.push_back(std::make_pair(info.first.id, info.second));
	jsbuf["info_problems"] = _list;

	write_problem_json(filename, jsbuf);
}

void problem_list::delete_duplicates()
{
	// 删除选择题重复项
	for (int i = 0; i < choice_problems.size(); i++)
		for (int j = i + 1; j < choice_problems.size(); j++)
			if (choice_problems[i].first.content == choice_problems[j].first.content &&
				choice_problems[i].first.options == choice_problems[j].first.options &&
				choice_problems[i].first.answer == choice_problems[j].first.answer)
			{
				choice_problems[i].second |= choice_problems[j].second; // 如果其中一个被使用了则都标记为已使用
				choice_problems.erase(choice_problems.begin() + j);
				j--;
			}
	// 重新编号
	for (int i = 0; i < choice_problems.size(); i++)
		choice_problems[i].first.id = i + 1; // id 从 1 开始

	// 删除填空题重复项
	for (int i = 0; i < completion_problems.size(); i++)
		for (int j = i + 1; j < completion_problems.size(); j++)
			if (completion_problems[i].first.content == completion_problems[j].first.content &&
				completion_problems[i].first.answer == completion_problems[j].first.answer)
			{
				completion_problems[i].second |= completion_problems[j].second; // 如果其中一个被使用了则都标记为已使用
				completion_problems.erase(completion_problems.begin() + j);
				j--;
			}
	// 重新编号
	for (int i = 0; i < completion_problems.size(); i++)
		completion_problems[i].first.id = i + 1; // id 从 1 开始

	// 删除信息题重复项
	for (int i = 0; i < info_problems.size(); i++)
		for (int j = i + 1; j < info_problems.size(); j++)
			if (info_problems[i].first.content == info_problems[j].first.content &&
				info_problems[i].first.info == info_problems[j].first.info &&
				info_problems[i].first.answer == info_problems[j].first.answer)
			{
				info_problems[i].second |= info_problems[j].second; // 如果其中一个被使用了则都标记为已使用
				info_problems.erase(info_problems.begin() + j);
				j--;
			}
	// 重新编号
	for (int i = 0; i < info_problems.size(); i++)
		info_problems[i].first.id = i + 1; // id 从 1 开始
}

void problem_list::convert(std::string filename_in, std::string folder_out, std::ostream& log)
{
	log << "Converting problems from " << filename_in << " to " << folder_out << " ..." << std::endl;

	responder_problem_output_handler new_pl;

	try 
	{
		std::ifstream fin(filename_in);
		if (!fin) // 如果文件打开失败
			throw std::runtime_error("Failed to open input file.");

		int complexity, type;

#undef max
		while (true)
		{
			// 检查是否能成功读取两个整数
			if (!(fin >> complexity >> type))
			{
				if (fin.eof())
					break; // 到达文件末尾时退出
				else 
				{
					log << "Error reading integers (complexity, type). Skipping line." << std::endl;
					fin.clear(); // 清除错误状态
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 跳过当前行
					continue; // 继续下一次循环
				}
			}
			fin.get(); // 读取整数后面的换行符

			if (!(1 <= complexity && complexity <= 3))
			{
				log << "Invalid complexity value: " << complexity << ". Setting to 1." << std::endl;
				complexity = 1;
			}

			std::string temp;
			switch (type)
			{
			case 0: // 选择题
				new_pl.choice_problems.push_back(std::make_pair(problem_choice(), 0));
				std::getline(fin, new_pl.choice_problems.back().first.content);
				for (int i = 0; i < 4; i++)
				{
					std::getline(fin, temp);
					new_pl.choice_problems.back().first.options.push_back(temp);
				}
				std::getline(fin, new_pl.choice_problems.back().first.answer);
				new_pl.choice_problems.back().first.id = new_pl.choice_problems.size(); // id 从 1 开始
				new_pl.choice_problems.back().first.complexity = complexity;

				log << " - converted: type=choice, id=" << new_pl.choice_problems.size() << std::endl;

				break;

			case 1: // 填空题
				new_pl.completion_problems.push_back(std::make_pair(problem_completion(), 0));
				std::getline(fin, new_pl.completion_problems.back().first.content);
				std::getline(fin, new_pl.completion_problems.back().first.answer);
				new_pl.completion_problems.back().first.id = new_pl.completion_problems.size(); // id 从 1 开始
				new_pl.completion_problems.back().first.complexity = complexity;

				log << " - converted: type=completion, id=" << new_pl.completion_problems.size() << std::endl;

				break;

			case 2: // 信息题
				new_pl.info_problems.push_back(std::make_pair(problem_info(), 0));
				std::getline(fin, new_pl.info_problems.back().first.content);
				for (int i = 0; i < 4; i++)
				{
					std::getline(fin, temp);
					new_pl.info_problems.back().first.info.push_back(temp);
				}
				std::getline(fin, new_pl.info_problems.back().first.answer);
				new_pl.info_problems.back().first.id = new_pl.info_problems.size(); // id 从 1 开始
				new_pl.info_problems.back().first.complexity = complexity;

				log << " - converted: type=info, id=" << new_pl.info_problems.size() << std::endl;

				break;

			default:
				throw std::runtime_error("Unknown problem type encountered.");
			}
		}

		fin.close();

		new_pl.delete_duplicates(); // 删除重复题目（根据题目内容和选项/信息判断）

		// 进行目录的删除和创建操作
		RemoveDir(folder_out.c_str());
		CreateDirectoryA(folder_out.c_str(), NULL);
		new_pl.output_to_folder(folder_out);

		log << "Successfully converted problems and saved to " << folder_out << std::endl;
	}
	catch (const std::ios_base::failure& e) {
		log << "I/O error: " << e.what() << std::endl;
	}
	catch (const std::exception& e) {
		log << "An error occurred: " << e.what() << std::endl;
	}
	catch (...) {
		log << "An unknown error occurred." << std::endl;
	}
}

void problem_list::merge(std::string filename_in, std::string folder_out, std::ostream& log)
{
	log << "Merging problems from " << filename_in << " into " << folder_out << " ..." << std::endl;

	responder_problem_output_handler new_pl;
	new_pl.load(folder_out + "problem_list.json", log); // 读入原有题目列表和题目信息，进行合并后覆盖写出

	try
	{
		std::ifstream fin(filename_in);
		if (!fin) // 如果文件打开失败
			throw std::runtime_error("Failed to open input file.");

		int complexity, type;
		while (true)
		{
			// 检查是否能成功读取两个整数
			if (!(fin >> complexity >> type))
			{
				if (fin.eof())
					break; // 到达文件末尾时退出
				else
				{
					log << "Error reading integers (complexity, type). Skipping line." << std::endl;
					fin.clear(); // 清除错误状态
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 跳过当前行
					continue; // 继续下一次循环
				}
			}
			fin.get(); // 读取整数后面的换行符

			if (!(1 <= complexity && complexity <= 3))
			{
				log << "Invalid complexity value: " << complexity << ". Setting to 1." << std::endl;
				complexity = 1;
			}

			std::string temp;
			switch (type)
			{
			case 0: // 选择题
			{
				problem_choice new_problem;
				std::getline(fin, new_problem.content);
				for (int i = 0; i < 4; i++)
				{
					std::getline(fin, temp);
					new_problem.options.push_back(temp);
				}
				std::getline(fin, new_problem.answer);

				// 检查是否与现有题目重复（内容、选项和答案完全相同），如果是则跳过
				bool is_duplicate = false;
				for (auto& existing_problem : new_pl.choice_problems)
				{
					if (existing_problem.first.content == new_problem.content &&
						existing_problem.first.options == new_problem.options &&
						existing_problem.first.answer == new_problem.answer)
					{
						is_duplicate = true;
						log << " - skipped: duplicate choice problem detected." << std::endl;
					}
				}
				if (is_duplicate)
					break;

				new_pl.choice_problems.push_back(std::make_pair(new_problem, 0));
				new_pl.choice_problems.back().first.id = new_pl.choice_problems.size(); // id 接着原有题目的最大id继续编号
				new_pl.choice_problems.back().first.complexity = complexity;

				log << " - converted: type=choice, id=" << new_pl.choice_problems.size() << std::endl;

			}
			break;

			case 1: // 填空题
			{
				problem_completion new_problem;
				std::getline(fin, new_problem.content);
				std::getline(fin, new_problem.answer);

				// 检查是否与现有题目重复（内容、选项和答案完全相同），如果是则跳过
				bool is_duplicate = false;
				for (auto& existing_problem : new_pl.completion_problems)
				{
					if (existing_problem.first.content == new_problem.content &&
						existing_problem.first.answer == new_problem.answer)
					{
						is_duplicate = true;
						log << " - skipped: duplicate completion problem detected." << std::endl;
					}
				}
				if (is_duplicate)
					break;

				new_pl.completion_problems.push_back(std::make_pair(new_problem, 0));
				new_pl.completion_problems.back().first.id = new_pl.completion_problems.size(); // id 接着原有题目的最大id继续编号
				new_pl.completion_problems.back().first.complexity = complexity;

				log << " - converted: type=completion, id=" << new_pl.completion_problems.size() << std::endl;
			}
			break;

			case 2: // 信息题
			{
				problem_info new_problem;
				std::getline(fin, new_problem.content);
				for (int i = 0; i < 4; i++)
				{
					std::getline(fin, temp);
					new_problem.info.push_back(temp);
				}
				std::getline(fin, new_problem.answer);

				// 检查是否与现有题目重复（内容、选项和答案完全相同），如果是则跳过
				bool is_duplicate = false;
				for (auto& existing_problem : new_pl.info_problems)
				{
					if (existing_problem.first.content == new_problem.content &&
						existing_problem.first.info == new_problem.info &&
						existing_problem.first.answer == new_problem.answer)
					{
						is_duplicate = true;
						log << " - skipped: duplicate info problem detected." << std::endl;
					}
				}
				if (is_duplicate)
					break;

				new_pl.info_problems.push_back(std::make_pair(new_problem, 0));
				new_pl.info_problems.back().first.id = new_pl.info_problems.size(); // id 接着原有题目的最大id继续编号
				new_pl.info_problems.back().first.complexity = complexity;

				log << " - converted: type=info, id=" << new_pl.info_problems.size() << std::endl;
			}
			break;

			default:
				throw std::runtime_error("Unknown problem type encountered.");
			}
		}

		fin.close();

		// 进行目录的删除和创建操作
		RemoveDir(folder_out.c_str());
		CreateDirectoryA(folder_out.c_str(), NULL);
		new_pl.output_to_folder(folder_out);

		log << "Successfully merged problems and saved to " << folder_out << std::endl;
	}
	catch (const std::ios_base::failure& e) {
		log << "I/O error: " << e.what() << std::endl;
	}
	catch (const std::exception& e) {
		log << "An error occurred: " << e.what() << std::endl;
	}
	catch (...) {
		log << "An unknown error occurred." << std::endl;
	}
}

void responder_problem_output_handler::output_to_folder(std::string output_folder)
{
	json jsbuf;
	std::vector<std::pair<int, int>> _list;

	for (auto choice : choice_problems)
	{
		choice.first.save(output_folder + "choice_" + std::to_string(choice.first.id) + ".json");
		_list.push_back(std::make_pair(choice.first.id, choice.second));
	}
	jsbuf["choice_problems"] = _list;
	_list.clear();

	for (auto completion : completion_problems)
	{
		completion.first.save(output_folder + "completion_" + std::to_string(completion.first.id) + ".json");
		_list.push_back(std::make_pair(completion.first.id, completion.second));
	}
	jsbuf["completion_problems"] = _list;
	_list.clear();

	for (auto info : info_problems)
	{
		info.first.save(output_folder + "info_" + std::to_string(info.first.id) + ".json");
		_list.push_back(std::make_pair(info.first.id, info.second));
	}
	jsbuf["info_problems"] = _list;

	std::ofstream fout(output_folder + "problem_list.json");
	fout << jsbuf.dump();
	fout.close();
}
