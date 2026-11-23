// encoding.cpp : Responder URI 编解码工具的实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "encoding.h"

namespace
{
	int hex_value(char ch)
	{
		if (ch >= '0' && ch <= '9')
			return ch - '0';
		if (ch >= 'A' && ch <= 'F')
			return ch - 'A' + 10;
		if (ch >= 'a' && ch <= 'f')
			return ch - 'a' + 10;
		return -1;
	}

	bool is_uri_unreserved_or_reserved(unsigned char c)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
			return true;
		switch (c)
		{
		case '-': case '_': case '.': case '~':
		case '!': case '#': case '$': case '&':
		case '\'': case '(': case ')': case '*':
		case '+': case ',': case '/': case ':':
		case ';': case '=': case '?': case '@':
			return true;
		default:
			return false;
		}
	}
}

std::string encode_uri(const std::string& s)
{
	static constexpr char kHexDigits[] = "0123456789ABCDEF";
	std::string out;
	out.reserve(s.size());

	for (unsigned char c : s)
	{
		if (is_uri_unreserved_or_reserved(c))
		{
			out.push_back(static_cast<char>(c));
		}
		else
		{
			out.push_back('%');
			out.push_back(kHexDigits[(c >> 4) & 0x0F]);
			out.push_back(kHexDigits[c & 0x0F]);
		}
	}
	return out;
}

std::string decode_uri(const std::string& s, bool plus_as_space)
{
	std::string out;
	out.reserve(s.size());

	for (size_t i = 0; i < s.size(); ++i)
	{
		const char ch = s[i];
		if (plus_as_space && ch == '+')
		{
			out.push_back(' ');
		}
		else if (ch == '%' && i + 2 < s.size())
		{
			const int hi = hex_value(s[i + 1]);
			const int lo = hex_value(s[i + 2]);
			if (hi >= 0 && lo >= 0)
			{
				out.push_back(static_cast<char>((hi << 4) | lo));
				i += 2;
			}
			else
			{
				out.push_back(ch);
			}
		}
		else
		{
			out.push_back(ch);
		}
	}
	return out;
}
