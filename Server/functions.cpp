// functions.cpp : Responder 复用函数的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "functions.h"
#include <iostream>
#include <winsock2.h>

// get_ip_list 使用 Winsock，这里显式链接，避免使用方遗漏 ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

bool RemoveDir(const char* szFileDir)
{
	if (!szFileDir || szFileDir[0] == '\0')
		return false;
	std::string strDir = szFileDir;
	if (strDir.back() != '\\' && strDir.back() != '/')
		strDir += '\\';
	WIN32_FIND_DATAA wfd;
	HANDLE hFind = FindFirstFileA((strDir + "*.*").c_str(), &wfd);
	if (hFind == INVALID_HANDLE_VALUE)
		return false;
	do
	{
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(wfd.cFileName, ".") != 0 &&
				strcmp(wfd.cFileName, "..") != 0)
				RemoveDir((strDir + wfd.cFileName).c_str());
		}
		else
		{
			DeleteFileA((strDir + wfd.cFileName).c_str());
		}
	} while (FindNextFileA(hFind, &wfd));
	FindClose(hFind);
	return RemoveDirectoryA(szFileDir) != 0;
}

void CopyDir(const char* szSrcDir, const char* szDstDir)
{
	std::string strSrcDir = szSrcDir;
	std::string strDstDir = szDstDir;
	if (strSrcDir.at(strSrcDir.length() - 1) != '\\')
		strSrcDir += '\\';
	if (strDstDir.at(strDstDir.length() - 1) != '\\')
		strDstDir += '\\';
	WIN32_FIND_DATAA wfd;
	HANDLE hFind = FindFirstFileA((strSrcDir + "*.*").c_str(), &wfd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(wfd.cFileName, ".") != 0 &&
				strcmp(wfd.cFileName, "..") != 0)
			{
				CreateDirectoryA((strDstDir + wfd.cFileName).c_str(), NULL);
				CopyDir((strSrcDir + wfd.cFileName).c_str(), (strDstDir + wfd.cFileName).c_str());
			}
		}
		else
		{
			CopyFileA((strSrcDir + wfd.cFileName).c_str(), (strDstDir + wfd.cFileName).c_str(), FALSE);
		}
	} while (FindNextFileA(hFind, &wfd));
	FindClose(hFind);
}

std::string get_ip_list()
{
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR)
	{
		std::cerr << "Error: " << WSAGetLastError() << " when getting local host name." << std::endl;
		return "";
	}
	struct hostent* phe = gethostbyname(ac);
	if (phe == 0)
	{
		std::cerr << "Error: Yow! Bad host lookup." << std::endl;
		return "";
	}
	std::string ip_list;
	for (int i = 0; phe->h_addr_list[i] != 0; ++i)
	{
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		ip_list += inet_ntoa(addr);
		ip_list += "\n";
	}
	return ip_list;
}