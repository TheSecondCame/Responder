#include <winsock2.h>
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "encoding.h"
#include "responder_request.h"
#include "responder_problem.h"
#include "responder_competition.h"
#include "logging.h"
#include "problem_upload.h"

std::string authentic_token = "CONCURRENCY_TEST_TOKEN_50_CHARS_0123456789ABCDEFGH";

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

static void TestProblemUpload(const fs::path& workdir)
{
    const fs::path source = workdir / L"只读题库.txt";
    const fs::path destination = workdir / "upload.txt";
    const fs::path alias = workdir / "upload-alias.txt";
    const std::string contents(150000, 'Q');
    WriteText(source, contents);
    assert(SetFileAttributesW(source.c_str(), FILE_ATTRIBUTE_READONLY));

    assert(StageProblemUpload(source, destination));
    assert(ReadAllText(source) == contents);
    assert(ReadAllText(destination) == contents);
    assert(GetFileAttributesW(source.c_str()) & FILE_ATTRIBUTE_READONLY);
    assert(!(GetFileAttributesW(destination.c_str()) & FILE_ATTRIBUTE_READONLY));
    assert(fs::remove(destination));

    // A second import must work after cleaning up the read-only source's copy.
    assert(StageProblemUpload(source, destination));
    assert(!StageProblemUpload(destination, destination));
    fs::create_hard_link(destination, alias);
    assert(!StageProblemUpload(alias, destination));
    assert(ReadAllText(destination) == contents);
    assert(ReadAllText(alias) == contents);
    assert(!StageProblemUpload(workdir / "missing.txt", destination));
    assert(ReadAllText(destination) == contents);
    assert(!StageProblemUpload(source, workdir / "missing-directory" / "upload.txt"));

    fs::remove(alias);
    assert(SetFileAttributesW(source.c_str(), FILE_ATTRIBUTE_NORMAL));
    WriteText(source, "ordinary");
    assert(StageProblemUpload(source, destination));
    assert(ReadAllText(destination) == "ordinary");
    assert(SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_READONLY));
    assert(!StageProblemUpload(source, destination));
    assert(ReadAllText(destination) == "ordinary");
    assert(SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_NORMAL));
    WriteText(source, "");
    assert(StageProblemUpload(source, destination));
    assert(ReadAllText(destination).empty());
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

static void TestSessionWriteFailurePreservesPreviousAndRetries(const fs::path& workdir)
{
    fs::create_directories(workdir / "data" / "competitions");
    competition c;
    c.guid = "test-write-fail-guid";
    c.name = "Write Fail Test";
    c.description = "Regression";
    c.teams = { { "Alpha", 'A' }, { "Beta", 'B' } };
    c.team_scores = { 0, 0 };
    c.competition_status = competition::ONGOING;
    c.choice_count = 5;
    c.save();

    assert(competition_session::update(c.guid, c.teams.size(), [](competition_session& s, bool) {
        s.current_type = 0;
        s.current_id = 1;
        s.shown_at = 1000;
    }));

    fs::path session_path = workdir / "data" / "competitions" / c.guid / "session.json";
    fs::path tmp_path = workdir / "data" / "competitions" / c.guid / "session.json.tmp";
    assert(fs::exists(session_path));

    // Mark session.json read-only to force atomic MoveFileExA replacement to fail
    assert(SetFileAttributesA(session_path.string().c_str(), FILE_ATTRIBUTE_READONLY));
    bool second_saved = competition_session::update(c.guid, c.teams.size(), [](competition_session& s, bool) {
        s.current_type = 0;
        s.current_id = 2;
        s.shown_at = 2000;
    });
    assert(!second_saved);
    assert(!fs::exists(tmp_path));

    // Previous session (Q1) must still be intact on disk
    competition_session still_q1;
    assert(still_q1.load(c.guid, c.teams.size()));
    assert(still_q1.current_id == 1);

    // Restore write permission and retry the same question update without losing state
    assert(SetFileAttributesA(session_path.string().c_str(), FILE_ATTRIBUTE_NORMAL));
    assert(competition_session::update(c.guid, c.teams.size(), [](competition_session& s, bool) {
        s.current_type = 0;
        s.current_id = 2;
        s.shown_at = 2001;
    }));

    competition_session after_retry;
    assert(after_retry.load(c.guid, c.teams.size()));
    assert(after_retry.current_id == 2);

    competition_session::clear(c.guid);
}

static void TestPauseFinishSyncRace(const fs::path& workdir)
{
    fs::create_directories(workdir / "data" / "competitions");
    const std::string guid = "test-race-guid";

    for (int iter = 0; iter < 50; ++iter) {
        competition init_c;
        init_c.guid = guid;
        init_c.name = "Race Test";
        init_c.description = "Regression";
        init_c.teams = { { "Alpha", 'A' }, { "Beta", 'B' } };
        init_c.team_scores = { 0, 0 };
        init_c.competition_status = competition::ONGOING;
        init_c.save();
        competition_session::clear(guid);

        std::atomic<bool> start{ false };
        auto target_status = (iter % 2 == 0) ? competition::PAUSED : competition::FINISHED;

        std::thread sync_thread([&]() {
            while (!start.load(std::memory_order_acquire)) {}
            for (int k = 0; k < 5; ++k) {
                auto comp_mtx = get_competition_mutex(guid);
                std::lock_guard<std::mutex> lock(*comp_mtx);
                competition c;
                c.load(guid);
                if (c.competition_status != competition::ONGOING) {
                    break;
                }
                assert(competition_session::update(guid, c.teams.size(), [](competition_session& s, bool) {
                    s.current_type = 0;
                    s.current_id = 1;
                    s.timer_started_at = 12345;
                }));
            }
        });

        std::thread end_thread([&]() {
            while (!start.load(std::memory_order_acquire)) {}
            auto comp_mtx = get_competition_mutex(guid);
            std::lock_guard<std::mutex> lock(*comp_mtx);
            competition c;
            c.load(guid);
            c.competition_status = target_status;
            competition_session::clear(guid);
            c.save();
        });

        start.store(true, std::memory_order_release);
        sync_thread.join();
        end_thread.join();

        competition final_c;
        final_c.load(guid);
        assert(final_c.competition_status == target_status);
        fs::path session_path = workdir / "data" / "competitions" / guid / "session.json";
        assert(!fs::exists(session_path));

        // Post-pause/finish delayed sync under per-competition lock must observe non-ONGOING and leave session absent
        {
            auto comp_mtx = get_competition_mutex(guid);
            std::lock_guard<std::mutex> lock(*comp_mtx);
            competition delayed_c;
            delayed_c.load(guid);
            assert(delayed_c.competition_status != competition::ONGOING);
        }
        assert(!fs::exists(session_path));
    }
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

static void TestEncodingAndFormParsing()
{
    const std::string utf8 = "成都七中 STA 竞赛题库 + 100% & a=b";
    const std::string encoded = encode_uri(utf8);
    assert(decode_uri(encoded, false) == utf8);

    // Payload exceeding the legacy 512000-byte fixed stack/static buffers
    std::string big(700000, 'A');
    big += "%E6%B5%8B%E8%AF%95%2B+space";
    std::string decoded = decode_uri(big, true);
    const std::string suffix = "测试+ space";
    assert(decoded.size() == 700000 + suffix.size());
    assert(decoded.substr(700000) == suffix);

    // Malformed percent-escapes stay literal
    assert(decode_uri("%ZZ%2%G0%", false) == "%ZZ%2%G0%");

    // build_request_context parses >512KB form fields, preserves %2B as '+', and overrides query params
    httplib::Request req;
    req.params.emplace("name", "from_query");
    req.params.emplace("only_query", "qval");
    req.headers.emplace("Content-Type", "Application/X-WWW-Form-Urlencoded; charset=UTF-8");
    const std::string huge_val(600000, 'x');
    req.body = "name=a%2Bb+c&huge=" + huge_val;

    request_context ctx = build_request_context(req, true);
    assert(ctx.get_param("name") == "a+b c");
    assert(ctx.get_param("only_query") == "qval");
    assert(ctx.get_param("huge") == huge_val);

    // Multi-threaded concurrent encode_uri/decode_uri
    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 200; ++i) {
                std::string src = "thread_" + std::to_string(t) + "_题_" + std::to_string(i);
                assert(decode_uri(encode_uri(src)) == src);
            }
        });
    }
    for (auto& th : threads) th.join();
}

static void TestProblemStreamLoggingNoCoutHijack(const fs::path& workdir)
{
    fs::create_directories(workdir / "data" / "problems");
    const fs::path txt = workdir / "stream_log_batch.txt";
    WriteText(txt,
        "1 1\n"
        "Stream log test question\n"
        "42\n");

    std::ostringstream cout_canary;
    std::streambuf* orig_cout = std::cout.rdbuf(cout_canary.rdbuf());

    std::ostringstream op_log;
    problem_list::convert(txt.string(), "data/problems/", op_log);
    assert(std::cout.rdbuf() == cout_canary.rdbuf());
    assert(cout_canary.str().empty());
    assert(op_log.str().find("Converting problems from") != std::string::npos);
    assert(op_log.str().find("Successfully converted problems") != std::string::npos);

    const fs::path txt_merge = workdir / "stream_log_merge.txt";
    WriteText(txt_merge,
        "2 1\n"
        "Stream log merge question\n"
        "99\n");

    std::ostringstream merge_log;
    problem_list::merge(txt_merge.string(), "data/problems/", merge_log);
    assert(std::cout.rdbuf() == cout_canary.rdbuf());
    assert(cout_canary.str().empty());
    assert(merge_log.str().find("Merging problems from") != std::string::npos);
    assert(merge_log.str().find("Successfully merged problems") != std::string::npos);

    std::cout.rdbuf(orig_cout);
}

static void TestConcurrentRequestContextRouting()
{
    class TestEchoPage : public responder_page
    {
    protected:
        void init() override
        {
            address = "/test/page";
            browser_title = "default";
            page_title = "default";
            is_request_login = true;
            is_index_page = true;
        }
        void generate_page(const request_context& ctx, page_context& page) override
        {
            const std::string guid = ctx.get_param("guid");
            const std::string team = ctx.get_param("team");
            std::this_thread::yield();
            page.browser_title = "BT_" + guid;
            page.page_title = "PT_" + guid;
            page.content = "PAGE_PAYLOAD:" + guid + ":" + team;
        }
    };

    class TestEchoPost : public responder_post_request
    {
    protected:
        void init() override
        {
            address = "/test/post";
            is_request_login = true;
        }
        std::string generate_response(const request_context& ctx) override
        {
            const std::string guid = ctx.get_param("guid");
            const std::string team = ctx.get_param("team");
            const std::string score = ctx.get_param("score");
            std::this_thread::yield();
            return "POST_PAYLOAD:" + guid + ":" + team + ":" + score;
        }
    };

    TestEchoPage echo_page;
    TestEchoPost echo_post;

    httplib::Server svr;
    echo_page.set_handler(svr);
    echo_post.set_handler(svr);

    int port = svr.bind_to_any_port("127.0.0.1");
    assert(port > 0);
    std::thread server_thread([&]() { svr.listen_after_bind(); });
    svr.wait_until_ready();

    // Suppress verbose per-request std::cout noise during 200-request stress test
    std::ostringstream sink;
    std::streambuf* orig_cout = std::cout.rdbuf(sink.rdbuf());

    const int kWorkers = 20;
    const int kPerWorker = 5; // 100 GET + 100 POST concurrent requests on the same static handler instances
    std::atomic<int> failures{ 0 };
    std::vector<std::thread> clients;
    for (int w = 0; w < kWorkers; ++w)
    {
        clients.emplace_back([&, w]() {
            httplib::Client cli("127.0.0.1", port);
            cli.set_connection_timeout(5, 0);
            cli.set_read_timeout(5, 0);
            httplib::Headers hdrs = { { "Cookie", "token=" + authentic_token } };

            for (int k = 0; k < kPerWorker; ++k)
            {
                const int id = w * kPerWorker + k;
                const std::string guid = "guid-" + std::to_string(id);
                const std::string team = "team-" + std::to_string(id % 4);
                const std::string score = std::to_string(1000 + id);

                auto r1 = cli.Get(("/test/page?guid=" + guid + "&team=" + team).c_str(), hdrs);
                if (!r1 || r1->status != 200)
                {
                    ++failures;
                    continue;
                }
                if (r1->body.find("<title>BT_" + guid + "</title>") == std::string::npos ||
                    r1->body.find("PAGE_PAYLOAD:" + guid + ":" + team) == std::string::npos)
                {
                    ++failures;
                }

                auto r2 = cli.Post("/test/post", hdrs,
                    "guid=" + guid + "&team=" + team + "&score=" + score,
                    "application/x-www-form-urlencoded");
                if (!r2 || r2->status != 200 ||
                    r2->body != ("POST_PAYLOAD:" + guid + ":" + team + ":" + score))
                {
                    ++failures;
                }
            }
        });
    }
    for (auto& t : clients) t.join();

    std::cout.rdbuf(orig_cout);
    svr.stop();
    server_thread.join();

    assert(failures.load() == 0);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    fs::path orig_cwd = fs::current_path();
    fs::path temp_dir = fs::temp_directory_path() / ("responder_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);
    fs::current_path(temp_dir);

    TestProblemUpload(temp_dir);
    TestProblemListLifecycle(temp_dir);
    TestDeleteDuplicatesPreservesUsed();
    TestCompetitionSessionLifecycle(temp_dir);
    TestSessionWriteFailurePreservesPreviousAndRetries(temp_dir);
    TestPauseFinishSyncRace(temp_dir);
    TestLogging();
    TestEncodingAndFormParsing();
    TestProblemStreamLoggingNoCoutHijack(temp_dir);
    TestConcurrentRequestContextRouting();

    fs::current_path(orig_cwd);
    std::error_code ec;
    fs::remove_all(temp_dir, ec);

    std::cout << "All regression tests passed." << std::endl;
    return 0;
}
