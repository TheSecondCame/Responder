// responder_competition.h : Responder 比赛类的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_competition.h"
#include <winsock2.h>
#include <Windows.h>
#include <objbase.h>

#pragma comment(lib, "ole32.lib")

std::vector<std::string> load_competition_list(std::string filename)
{
	std::ifstream fin(filename);
	if (fin.fail())
	{
		std::cout << "Failed to load competition list from " << filename << std::endl;
		// TODO: MessageBox
		return std::vector<std::string>();
	}

	json jsbuf = json::parse(fin);
	fin.close();

	std::vector<std::string> guid_list;
	jsbuf["competitions"].get_to(guid_list);

	return guid_list;
}

void save_competition_list(std::string filename, const std::vector<std::string>& guid_list)
{
	json jsbuf;
	jsbuf["competitions"] = guid_list;
	
	std::ofstream fout(filename);
	fout << jsbuf.dump();
	fout.close();
}

void competition::load(std::string _guid)
{
	guid = _guid;
	std::string filename = "data/competitions/" + guid + "/competition.json";

	std::ifstream fin(filename);
	if (fin.fail())
	{
		std::cout << "Failed to load competition from " << filename << std::endl;
		// TODO: MessageBox
		return;
	}

	json jsbuf = json::parse(fin);
	fin.close();

	jsbuf["name"].get_to(name);
	jsbuf["description"].get_to(description);
	jsbuf["teams"].get_to(teams);
	jsbuf["competition_status"].get_to(competition_status);
	jsbuf["choice_count"].get_to(choice_count);
	jsbuf["completion_count"].get_to(completion_count);
	jsbuf["info_count"].get_to(info_count);

	problems.load("data/competitions/" + guid + "/problems.json");

	if (competition_status >= status::ONGOING)
	{
		fin.open("data/competitions/" + guid + "/scores.json");
		if (!fin.fail())
		{
			int temp = 0;
			for (int i = 1; i <= teams.size(); i++)
			{
				fin >> temp;
				team_scores.push_back(temp);
			}
			fin.close();
			return;
		}
		fin.close();
	}

	for (int i = 1; i <= teams.size(); i++)
		team_scores.push_back(0);
}

void competition::save()
{
	if (guid == "")
		return;

	CreateDirectoryA(("data/competitions/" + guid + "/").c_str(), NULL);

	json jsbuf;
	jsbuf["name"] = name;
	jsbuf["description"] = description;
	jsbuf["teams"] = teams;
	jsbuf["competition_status"] = competition_status;
	jsbuf["choice_count"] = choice_count;
	jsbuf["completion_count"] = completion_count;
	jsbuf["info_count"] = info_count;

	problems.save("data/competitions/" + guid + "/problems.json");

	std::ofstream fout("data/competitions/" + guid + "/competition.json");
	fout << jsbuf.dump();
	fout.close();
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
