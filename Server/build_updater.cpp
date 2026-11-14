// BuildUpdater.cpp : 更新Build信息。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include <iostream>
#include <fstream>
#include <Windows.h>
using namespace std;
int main()
{
	char strModule[256];
	GetModuleFileNameA(NULL, strModule, 256);
	std::string a;
	a.assign(strModule);
	a.append("\\..\\");
	SetCurrentDirectoryA(a.c_str());

	string temp;
	int build;
	ifstream fin("build.h");
	fin >> temp >> temp >> build;
	fin.close();

	ofstream fout("build.h");
	fout << "// Build " << build + 1;
	fout << R"(
// build.h : 定义程序Build信息

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef _RES_BUILD_
#define _RES_BUILD_

)";
	fout << "#define PROGRAM_BUILD " << build + 1 << endl;
	fout << "#define PROGRAM_BUILD_S \"" << build + 1 << "\"" << endl;
	fout << endl;
	fout << "#endif" << endl;
	fout.close();
	return 0;
}
