// encoding.h : Responder URI 编解码工具的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RES_ENCODING_H
#define RES_ENCODING_H

#include <string>

// 无状态、无固定尺寸缓冲的 URI 编解码工具（支持任意长度 UTF-8 字符串）
std::string encode_uri(const std::string& s);
std::string decode_uri(const std::string& s, bool plus_as_space = false);

#endif // RES_ENCODING_H
