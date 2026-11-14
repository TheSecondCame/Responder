// functions.h : Responder 复用函数的定义。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#ifndef RES_FUNCTIONS
#define RES_FUNCTIONS

#include <Windows.h>
#include <string>
#include <random>

bool RemoveDir(const char* szFileDir);
void CopyDir(const char* szSrcDir, const char* szDstDir);
std::string get_ip_list();

class RandomGenerator {
public:
    static RandomGenerator& getInstance() {
        static RandomGenerator instance;
        return instance;
    }

    int getInt(int min, int max) {
        std::uniform_int_distribution<> dis(min, max);
        return dis(gen);
    }

    double getDouble(double min, double max) {
        std::uniform_real_distribution<> dis(min, max);
        return dis(gen);
    }

private:
    RandomGenerator() : gen(rd()) {}
    std::random_device rd;
    std::mt19937 gen;
};

#endif