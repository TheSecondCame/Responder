#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "responder_problem.h"
#include "responder_competition.h"
#include "logging.h"

namespace fs = std::filesystem;

static std::string ReadAllText(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    assert(in.is_open());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void WriteText(const fs::path& p, const std::string& content)
{
    std::ofstream out(p, std::ios::binary);
    assert(out.is_open());
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
}

static void TestProblemListLifecycle(const fs::path& workdir)
{
    fs::create_directories(workdir / "data" / "problems");
    const std::string inputTxt =
        "2 0\n"
        "HTTP 状态码 404 表示什么？\n"
        "请求成功\n"
        "未找到资源\n"
        "服务器内部错误\n"
        "重定向\n"
        "B\n"
        "\n"
        "9 0\n"
        "HTTP 状态码 404 表示什么？\n"
        "请求成功\n"
        "未找到资源\n"
        "服务器内部错误\n"
        "重定向\n"
        "B\n"
        "\n"
        "3 1\n"
        "TCP 三次握手中第二次握手包含哪两个标志位？\n"
        "SYN 和 ACK\n"
        "\n"
        "1 2\n"
        "请根据线索猜协议\n"
        "工作在应用层\n"
        "默认端口 80\n"
        "无状态\n"
        "超文本传输\n"
        "HTTP\n";

    fs::path txt1 = workdir / "batch1.txt";
    WriteText(txt1, inputTxt);

    problem_list::convert(txt1.string(), "data/problems/");

    problem_list loaded;
    loaded.load("data/problems/problem_list.json");
    assert(loaded.choice_problems.size() == 1);
    assert(loaded.completion_problems.size() == 1);
    assert(loaded.info_problems.size() == 1);
    assert(loaded.choice_problems[0].first.id == 1);
    assert(loaded.choice_problems[0].first.complexity == 2);
    assert(loaded.choice_problems[0].first.answer == "B");
    assert(loaded.completion_problems[0].first.complexity == 3);
    assert(loaded.info_problems[0].first.info.size() == 4);
    assert(problem_list::count_unused(loaded.choice_problems) == 1);

    // Mark the choice problem as used and persist index before merge
    loaded.choice_problems[0].second = 1;
    loaded.save("data/problems/problem_list.json");

    const std::string mergeTxt =
        "1 0\n"
        "HTTP 状态码 404 表示什么？\n"
        "请求成功\n"
        "未找到资源\n"
        "服务器内部错误\n"
        "重定向\n"
        "B\n"
        "\n"
        "1 1\n"
        "HTTPS 默认端口是？\n"
        "443\n";
    fs::path txt2 = workdir / "batch2.txt";
    WriteText(txt2, mergeTxt);

    problem_list::merge(txt2.string(), "data/problems/");

    problem_list merged;
    merged.load("data/problems/problem_list.json");
    assert(merged.choice_problems.size() == 1);
    assert(merged.choice_problems[0].second == 1);
    assert(merged.completion_problems.size() == 2);
    assert(merged.completion_problems[1].first.id == 2);
    assert(merged.completion_problems[1].first.answer == "443");
    assert(problem_list::count_unused(merged.choice_problems) == 0);
    assert(problem_list::count_unused(merged.completion_problems) == 2);

    // remove_used keeps only unused problems
    problem_list unused = merged;
    unused.remove_used();
    assert(unused.choice_problems.empty());
    assert(unused.completion_problems.size() == 2);
    assert(unused.info_problems.size() == 1);
}

static void TestDeleteDuplicatesPreservesUsed()
{
    problem_list pl;
    problem_completion p1;
    p1.id = 1; p1.content = "Q1"; p1.answer = "A1"; p1.complexity = 1;
    problem_completion p2;
    p2.id = 2; p2.content = "Q1"; p2.answer = "A1"; p2.complexity = 1;
    problem_completion p3;
    p3.id = 3; p3.content = "Q2"; p3.answer = "A2"; p3.complexity = 2;
    pl.completion_problems.push_back({ p1, 0 });
    pl.completion_problems.push_back({ p2, 1 });
    pl.completion_problems.push_back({ p3, 0 });

    pl.delete_duplicates();
    assert(pl.completion_problems.size() == 2);
    assert(pl.completion_problems[0].first.id == 1);
    assert(pl.completion_problems[0].second == 1);
    assert(pl.completion_problems[1].first.id == 2);
    assert(pl.completion_problems[1].second == 0);
}

static void TestCompetitionSessionLifecycle(const fs::path& workdir)
{
    fs::create_directories(workdir / "data" / "competitions");
    competition c;
    c.guid = "test-session-guid";
    c.name = "Session Test";
    c.description = "Regression";
    c.teams = { { "Alpha", 'A' }, { "Beta", 'B' }, { "Gamma", 'C' } };
    c.team_scores = { 0, 0, 0 };
    c.competition_status = competition::ONGOING;
    c.choice_count = 5;
    c.save();

    competition loaded_legacy;
    loaded_legacy.load(c.guid);
    assert(loaded_legacy.last_problem_type == -1);
    assert(loaded_legacy.last_problem_id == 0);

    c.last_problem_type = 0;
    c.last_problem_id = 123;
    c.save();

    competition loaded_comp;
    loaded_comp.load(c.guid);
    assert(loaded_comp.last_problem_type == 0);
    assert(loaded_comp.last_problem_id == 123);

    competition_session empty_session;
    assert(!empty_session.load(c.guid, c.teams.size()));
    assert(empty_session.round_team == 3);
    assert(empty_session.team_order == std::vector<int>({ 0, 1, 2 }));

    assert(competition_session::update(c.guid, c.teams.size(), [](competition_session& s, bool existed) {
        assert(!existed);
        s.current_type = 0;
        s.current_id = 123;
        s.shown_at = 1763670000;
        s.timer_started_at = 1763670012;
        s.timer_duration = 15;
        s.timer_mode = 0;
        json client_state = {
            { "round_team", 2 },
            { "round_team_cnt", 2 },
            { "round_count", 15 },
            { "mode", "round" },
            { "auto_scoring", true },
            { "problem_number", 7 },
            { "info_id", 2 },
            { "show_answer", true },
            { "team_order", { 2, 0, 1 } }
        };
        s.apply_client_state(client_state, 3);
    }));

    competition_session restored;
    assert(restored.load(c.guid, c.teams.size()));
    assert(restored.current_type == 0);
    assert(restored.current_id == 123);
    assert(restored.shown_at == 1763670000);
    assert(restored.round_team == 2);
    assert(restored.round_count == 15);
    assert(restored.mode == "round");
    assert(restored.timer_started_at == 1763670012);
    assert(restored.timer_duration == 15);
    assert(restored.auto_scoring == true);
    assert(restored.problem_number == 7);
    assert(restored.info_id == 2);
    assert(restored.show_answer == true);
    assert(restored.team_order == std::vector<int>({ 2, 0, 1 }));

    // apply_client_state must not overwrite server-owned current_id or accept malformed team_order
    json bad_client = {
        { "current_id", 0 },
        { "team_order", { 99, 0, 1 } },
        { "round_team", 1 }
    };
    restored.apply_client_state(bad_client, c.teams.size());
    assert(restored.current_id == 123);
    assert(restored.round_team == 1);
    assert(restored.team_order == std::vector<int>({ 0, 1, 2 }));

    competition_session::clear(c.guid);
    competition_session after_clear;
    assert(!after_clear.load(c.guid, c.teams.size()));
}

static void TestLogging()
{
    std::streambuf* orig_cout = std::cout.rdbuf();
    std::streambuf* orig_cerr = std::cerr.rdbuf();

    logging::clear();
    logging::setup(1);
    std::cout << "regression-log-line-1" << std::endl;
    std::cout << "regression-log-line-2" << std::endl;

    auto drained = logging::drain();
    assert(drained.size() == 2);
    assert(drained[0] == "regression-log-line-1");
    assert(drained[1] == "regression-log-line-2");
    assert(logging::drain().empty());
    assert(logging::snapshot().size() == 2);

    std::string logFile = ReadAllText("server.log");
    assert(logFile.find("regression-log-line-1") != std::string::npos);
    assert(logFile.find("regression-log-line-2") != std::string::npos);

    std::cout.rdbuf(orig_cout);
    std::cerr.rdbuf(orig_cerr);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    fs::path orig_cwd = fs::current_path();
    fs::path temp_dir = fs::temp_directory_path() / ("responder_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);
    fs::current_path(temp_dir);

    TestProblemListLifecycle(temp_dir);
    TestDeleteDuplicatesPreservesUsed();
    TestCompetitionSessionLifecycle(temp_dir);
    TestLogging();

    fs::current_path(orig_cwd);
    std::error_code ec;
    fs::remove_all(temp_dir, ec);

    std::cout << "All regression tests passed." << std::endl;
    return 0;
}
