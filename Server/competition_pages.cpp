// competition_pages.cpp : Responder 比赛管理页面实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

#include "responder_pages.h"
#include "responder_problem.h"
#include "responder_competition.h"
#include "functions.h"
#include "build.h"
#include <ctime>

extern problem_list pl;
extern problem_list pl_unused;

namespace
{
	std::string safe_json_for_script(const json& j)
	{
		std::string s = j.dump();
		std::string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == '<' && i + 1 < s.size() && s[i + 1] == '/')
			{
				out += "<\\/";
				++i;
			}
			else
			{
				out += s[i];
			}
		}
		return out;
	}

	bool try_parse_int(const std::string& s, int& out)
	{
		try
		{
			size_t idx = 0;
			int val = std::stoi(s, &idx);
			if (idx != s.size())
				return false;
			out = val;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::string render_problem_html(int type, int id, const std::string& p_content, const std::string p_info[5], const std::string& p_ans)
	{
		return R"(
<div id="problemMeta" data-type=")" + std::to_string(type) + R"(" data-id=")" + std::to_string(id) + R"(" hidden></div>
<div class="h2" id="task">)" + p_content + R"(</div>
<div class="h2" id="d1area"><b style="display:inline-block" id="ta"></b><div style="display:inline" id="data1">)" + p_info[1] + R"(</div></div>
<div class="h2" id="d2area"><b style="display:inline-block" id="tb"></b><div style="display:inline" id="data2">)" + p_info[2] + R"(</div></div>
<div class="h2" id="d3area"><b style="display:inline-block" id="tc"></b><div style="display:inline" id="data3">)" + p_info[3] + R"(</div></div>
<div class="h2" id="d4area"><b style="display:inline-block" id="td"></b><div style="display:inline" id="data4">)" + p_info[4] + R"(</div></div>
<br>
<div id="ansarea">
  <p class="h3"><span class="badge bg-success" id="answertitle">答案</span></p> 
  <div class="h2"><b id="answer">)" + p_ans + R"(</b></div>
</div>
)";
	}

	template <typename T>
	const T* find_problem_by_id(const std::vector<std::pair<T, int>>& list, int id)
	{
		if (id >= 1 && static_cast<size_t>(id) <= list.size() && list[id - 1].first.id == id)
			return &list[id - 1].first;
		for (const auto& item : list)
		{
			if (item.first.id == id)
				return &item.first;
		}
		return nullptr;
	}

	bool lookup_and_render_problem(int type, int id, std::string& out_html)
	{
		if (id <= 0)
			return false;
		std::string p_content, p_info[5], p_ans;
		if (type == 0)
		{
			const problem_choice* p = find_problem_by_id(pl.choice_problems, id);
			if (!p) return false;
			p_content = p->content;
			p_ans = p->answer;
			for (size_t i = 0; i < 4 && i < p->options.size(); ++i)
				p_info[i + 1] = p->options[i];
		}
		else if (type == 1)
		{
			const problem_completion* p = find_problem_by_id(pl.completion_problems, id);
			if (!p) return false;
			p_content = p->content;
			p_ans = p->answer;
		}
		else if (type == 2)
		{
			const problem_info* p = find_problem_by_id(pl.info_problems, id);
			if (!p) return false;
			p_content = p->content;
			p_ans = p->answer;
			for (size_t i = 0; i < 4 && i < p->info.size(); ++i)
				p_info[i + 1] = p->info[i];
		}
		else
		{
			return false;
		}

		out_html = render_problem_html(type, id, p_content, p_info, p_ans);
		return true;
	}
}

void new_competition_page::init()
{
    address = "/main/competition/new";
    browser_title = "新建比赛 - Responder | 简单易用的知识竞赛组织系统";
    breadcrumb = "";
    page_title = "新建比赛";

	is_request_login = true;
}
void new_competition_page::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
    content = R"(
<form id="newComForm" method="post" role="form" style="margin-bottom:25px" novalidate>
<div class="row d-flex align-items-stretch">
  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <h3>基本信息</h3>
      <p>在此处填写一些关于比赛的基础信息。</p>
      <div class="form-floating mb-3">
        <input class="form-control" name="name" id="competitionName" autocomplete="off" required>
        <label for="competitionName">比赛名称</label>
      </div>
      <div class="form-floating mb-3">
        <input class="form-control" name="description" id="competitionDescription" autocomplete="off" required>
        <label for="competitionDescription">描述</label>
      </div>
    </div> 
  </div>

  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <div class="row">
        <div class="col-md">
          <h3>参赛队伍</h3>
        </div>
        <div class="col-md" align="right">
          <button class="btn btn-success btn-sm" type="button" id="addTeamBtn"><i class="bi bi-plus-lg"></i> 添加</button>
        </div>
      </div>
      <!-- <p align="center" id="teamHint"> 点击“添加”以开始。</p> -->
      <table class="table table-sm table-hover">
        <thead>
          <tr>
            <th>#</th>
            <th>队伍名称</th>
            <th>按键</th>
          </tr>
        </thead>
        <tbody id="teamContainer">
          <!-- 动态插入行 -->
        </tbody>
      </table>
    </div>
  </div>

  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <h3>题目分配</h3>
      <p>在此处初步分配比赛中使用的题目数量。</p>
      <table class="table table-sm table-hover">
        <thead>
          <tr>
            <th style="width:33%">类型</th>
            <th style="width:33%">可用</th>
            <th style="width:34%">所需</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>选择题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.choice_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="choiceCount" min="0" value="0" required></td>
          </tr>
          <tr>
            <td>填空题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.completion_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="completionCount" min="0" value="0" required></td>
          </tr>
          <tr>
            <td>信息题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.info_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="infoCount" min="0" value="0" required></td>
          </tr>
        </tbody>
      </table>
    </div> 
  </div>
</div>
<div class="d-grid gap-2 d-md-flex justify-content-md-end" style="margin-top:20px">
  <a class="btn btn-lg btn-outline-primary" align="right" href="/main">取消</a>
  <button class="btn btn-lg btn-success" align="right" type="submit">确定</button>
</div>
</form>

<script>
document.addEventListener('DOMContentLoaded', function() {
  const notyf = new Notyf({
    duration: 2000,
    position: {
      x: 'center',
      y: 'bottom',
    }
  });

  const maxTeams = 4;
  let teamCount = 0;

  const addTeamBtn = document.getElementById('addTeamBtn');
  const teamContainer = document.getElementById('teamContainer');
  const teamForm = document.getElementById('teamForm');
  const newComForm = document.getElementById('newComForm');

  // 点击加号按钮时添加表格行
  addTeamBtn.addEventListener('click', function() {
    if (teamCount < maxTeams) {
      teamCount++;

      // 创建新行
      const newRow = document.createElement('tr');

      // 创建文本框
      const newCell = document.createElement('td');
      const teamInput = document.createElement('input');
      teamInput.type = 'text';
      teamInput.name = `teamName${teamCount}`;
      teamInput.classList.add('form-control', 'form-control-sm');
      teamInput.placeholder = `请输入第 ${teamCount} 个队伍名称`;
      teamInput.autocomplete = 'off';
      teamInput.required = true;

      const newCell0 = document.createElement('td');
      newCell0.innerHTML = `<b>${teamCount}</b>`;

      const newCell2 = document.createElement('td');
      let key = 65 + teamCount - 1; // 从 A 开始绑定按键
      newCell2.innerHTML = String.fromCharCode(key);

      // 将文本框放到新行的单元格中
      newCell.appendChild(teamInput);
      newRow.appendChild(newCell0);
      newRow.appendChild(newCell);
      newRow.appendChild(newCell2);

      // 将新行添加到表格中
      teamContainer.appendChild(newRow);

      // 设置焦点到新添加的文本框
      teamInput.focus();
    } else {
      notyf.error('最多只能添加 4 个队伍');
    }
  });
  newComForm.addEventListener('submit', function(event) {
    event.preventDefault(); // 阻止默认的表单提交行为

    // 清除之前的错误状态
    newComForm.classList.remove('was-validated');

    // 执行HTML5验证
    if (newComForm.checkValidity() === false) {
        event.stopPropagation(); // 停止事件传播
    }

    // 添加was-validated类来显示Bootstrap的验证样式
    newComForm.classList.add('was-validated');

    // 如果表单验证通过，提交表单
    if (newComForm.checkValidity() === true) {
        if (teamCount < 2) {
            event.stopPropagation();
            notyf.error('请至少添加 2 个队伍');
            return;
        }
        newComForm.submit();
    }
  });
});
</script>
)";
}

void new_competition_handler::init()
{
    address = "/main/competition/new";

    is_request_login = true;
}
std::string new_competition_handler::generate_response()
{
    int teamCount = 0;
    for (auto& param : params)
        if (param.first.substr(0, 8) == "teamName")
            teamCount++;

    competition c;
	c.guid = competition::generate_guid();
    c.name = params["name"];
	c.description = params["description"];
	c.choice_count = std::stoi(params["choiceCount"]);
	c.completion_count = std::stoi(params["completionCount"]);
	c.info_count = std::stoi(params["infoCount"]);
	c.competition_status = competition::NOT_STARTED;
	for (int i = 1; i <= teamCount; i++)
    {
        std::string teamName = params["teamName" + std::to_string(i)];
        char key = 64 + i; // 从 A 开始绑定按键
        c.teams.push_back(std::make_pair(teamName, key));
    }
    c.save();

    auto comlist = load_competition_list("data/competitions/list.json");
	comlist.push_back(c.guid);
	save_competition_list("data/competitions/list.json", comlist);

    return R"(
<script> window.location.href = "/main/competition/detail?guid=)" + c.guid + R"("; </script>
)";
}

void competition_edit_page::init()
{
    address = "/main/competition/edit";

    is_request_login = true;
}
void competition_edit_page::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return;
    }

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return;
	}
	if (c.competition_status != competition::status::NOT_STARTED)
    {
        // error: TODO
        return;
    }

    browser_title = c.name + " - 编辑比赛 - Responder | 简单易用的知识竞赛组织系统";
    breadcrumb = "";
    page_title = "<b>" + c.name + "</b> - 编辑比赛";

    content = (std::string)R"(
<form id="newComForm" method="post" role="form" style="margin-bottom:25px" novalidate>
<div class="row d-flex align-items-stretch">
  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <h3>基本信息</h3>
      <p>在此处填写一些关于比赛的基础信息。</p>
      <div class="form-floating mb-3">
        <input class="form-control" name="name" id="competitionName" autocomplete="off" value=")" + c.name + R"(" required>
        <label for="competitionName">比赛名称</label>
      </div>
      <div class="form-floating mb-3">
        <input class="form-control" name="description" id="competitionDescription" autocomplete="off" value=")" + c.description + R"(" required>
        <label for="competitionDescription">描述</label>
      </div>
    </div> 
  </div>

  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <div class="row">
        <div class="col-md">
          <h3>参赛队伍</h3>
        </div>
        <div class="col-md" align="right">
          <button class="btn btn-success btn-sm" type="button" id="addTeamBtn"><i class="bi bi-plus-lg"></i> 添加</button>
        </div>
      </div>
      <!-- <p align="center" id="teamHint"> 点击“添加”以开始。</p> -->
      <table class="table table-sm table-hover">
        <thead>
          <tr>
            <th>#</th>
            <th>队伍名称</th>
            <th>按键</th>
          </tr>
        </thead>
        <tbody id="teamContainer">
          <!-- 动态插入行 -->
)";
    for (int i = 1; i <= c.teams.size(); i++)
        content += R"(
<tr><td><b>)" + std::to_string(i) + R"(</b></td><td>
<input type="text" name="teamName)" + std::to_string(i) + R"(" class="form-control form-control-sm" placeholder="请输入第 )" + std::to_string(i) + R"( 个队伍名称" value=")" + c.teams[i - 1].first + R"(" autocomplete="off" required>
</td><td>)" + c.teams[i - 1].second + R"(</td></tr>)";

    content += R"(
        </tbody>
      </table>
    </div>
  </div>

  <div class="col-lg-4 d-flex">
    <div class="portfolio-info w-100">
      <h3>题目分配</h3>
      <p>在此处初步分配比赛中使用的题目数量。</p>
      <table class="table table-sm table-hover">
        <thead>
          <tr>
            <th style="width:33%">类型</th>
            <th style="width:33%">可用</th>
            <th style="width:34%">所需</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>选择题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.choice_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="choiceCount" min="0" value=")" + std::to_string(c.choice_count) + R"(" required></td>
          </tr>
          <tr>
            <td>填空题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.completion_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="completionCount" min="0" value=")" + std::to_string(c.completion_count) + R"(" required></td>
          </tr>
          <tr>
            <td>信息题</td>
            <td>)" + std::to_string(problem_list::count_unused(pl.info_problems)) + R"(</td>
            <td><input type="number" class="form-control form-control-sm" name="infoCount" min="0" value=")" + std::to_string(c.info_count) + R"(" required></td>
          </tr>
        </tbody>
      </table>
    </div> 
  </div>
</div>
<div class="d-grid gap-2 d-md-flex justify-content-md-end" style="margin-top:20px">
  <a class="btn btn-lg btn-outline-primary" align="right" href="/main/competition/detail?guid=)" + params["guid"] + R"(">取消</a>
  <button class="btn btn-lg btn-success" align="right" type="submit">确定</button>
</div>
</form>

<script>
document.addEventListener('DOMContentLoaded', function() {
  const notyf = new Notyf({
    duration: 2000,
    position: {
      x: 'center',
      y: 'bottom',
    }
  });

  const maxTeams = 4;
  let teamCount = )" + std::to_string(c.teams.size()) + R"(;

  const addTeamBtn = document.getElementById('addTeamBtn');
  const teamContainer = document.getElementById('teamContainer');
  const teamForm = document.getElementById('teamForm');
  const newComForm = document.getElementById('newComForm');

  // 点击加号按钮时添加表格行
  addTeamBtn.addEventListener('click', function() {
    if (teamCount < maxTeams) {
      teamCount++;

      // 创建新行
      const newRow = document.createElement('tr');

      // 创建文本框
      const newCell = document.createElement('td');
      const teamInput = document.createElement('input');
      teamInput.type = 'text';
      teamInput.name = `teamName${teamCount}`;
      teamInput.classList.add('form-control', 'form-control-sm');
      teamInput.placeholder = `请输入第 ${teamCount} 个队伍名称`;
      teamInput.autocomplete = 'off';
      teamInput.required = true;

      const newCell0 = document.createElement('td');
      newCell0.innerHTML = `<b>${teamCount}</b>`;

      const newCell2 = document.createElement('td');
      let key = 65 + teamCount - 1; // 从 A 开始绑定按键
      newCell2.innerHTML = String.fromCharCode(key);

      // 将文本框放到新行的单元格中
      newCell.appendChild(teamInput);
      newRow.appendChild(newCell0);
      newRow.appendChild(newCell);
      newRow.appendChild(newCell2);

      // 将新行添加到表格中
      teamContainer.appendChild(newRow);

      // 设置焦点到新添加的文本框
      teamInput.focus();
    } else {
      notyf.error('最多只能添加 4 个队伍');
    }
  });
  newComForm.addEventListener('submit', function(event) {
    event.preventDefault(); // 阻止默认的表单提交行为

    // 清除之前的错误状态
    newComForm.classList.remove('was-validated');

    // 执行HTML5验证
    if (newComForm.checkValidity() === false) {
        event.stopPropagation(); // 停止事件传播
    }

    // 添加was-validated类来显示Bootstrap的验证样式
    newComForm.classList.add('was-validated');

    // 如果表单验证通过，提交表单
    if (newComForm.checkValidity() === true) {
        if (teamCount < 2) {
            event.stopPropagation();
            notyf.error('请至少添加 2 个队伍');
            return;
        }
        newComForm.submit();
    }
  });

});
</script>
)";
}

void competition_edit_handler::init()
{
    address = "/main/competition/edit";

    is_request_login = true;
}
std::string competition_edit_handler::generate_response()
{
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return "";
    }

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return "";
    }
    if (c.competition_status != competition::status::NOT_STARTED)
    {
        // error: TODO
        return "";
    }

    c.teams.clear();

    int teamCount = 0;
    for (auto& param : params)
        if (param.first.substr(0, 8) == "teamName")
            teamCount++;

    c.name = params["name"];
    c.description = params["description"];
    c.choice_count = std::stoi(params["choiceCount"]);
    c.completion_count = std::stoi(params["completionCount"]);
    c.info_count = std::stoi(params["infoCount"]);
    for (int i = 1; i <= teamCount; i++)
    {
        std::string teamName = params["teamName" + std::to_string(i)];
        char key = 64 + i; // 从 A 开始绑定按键
        c.teams.push_back(std::make_pair(teamName, key));
    }
    c.save();

    return R"(
<script> window.location.href = "/main/competition/detail?guid=)" + c.guid + R"("; </script>
)";
}

void competition_delete_handler::init()
{
    address = "/main/competition/delete";

    is_request_login = true;
}
std::string competition_delete_handler::generate_response()
{
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return "";
    }

    auto comp_mutex = get_competition_mutex(params["guid"]);
    std::lock_guard<std::mutex> comp_lock(*comp_mutex);

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return "";
    }
    if (c.competition_status == competition::status::ONGOING)
    {
        // error: TODO
        return "";
    }

    competition_session::clear(c.guid);
    RemoveDir(("data/competitions/" + c.guid).c_str());

	auto comlist = load_competition_list("data/competitions/list.json");
    for (auto it = comlist.begin(); it != comlist.end(); it++)
    {
        if (*it == c.guid)
        {
            comlist.erase(it);
            break;
        }
	}
	save_competition_list("data/competitions/list.json", comlist);

    return "Yes";
}

namespace competition_result_info
{
    struct competition_result
    {
        std::string team_name;
        int score;
        int ranking;
    };
    int cmp(const competition_result& a, const competition_result& b)
    {
        if (a.score != b.score)
        {
            return a.score > b.score; // 分数高的排前面
        }
        return a.team_name.compare(b.team_name); // 分数相同按队伍名称字典序排序
    }
    typedef std::vector<competition_result> teamlist;
    void get_ranking(const competition& c, teamlist& result)
    {
        for (int i = 0; i < c.teams.size(); i++)
            result.push_back({c.teams[i].first, c.team_scores[i], -1});
        std::sort(result.begin(), result.end(), cmp);
        for (int i = 0; i < result.size(); i++)
            result[i].ranking = (i == 0) ? 1 : ((result[i].score == result[i - 1].score) ? result[i - 1].ranking : (i + 1));
	}
}

void competition_detail_page::init()
{
    address = "/main/competition/detail";

    is_request_login = true;
}
void competition_detail_page::generate_page()
{
	std::lock_guard bank_lock(problem_bank_mutex);
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return;
    }

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return;
    }

    browser_title = c.name + " - 比赛详情 - Responder | 简单易用的知识竞赛组织系统";
    breadcrumb = "";
    page_title = "<b>" + c.name + "</b> - 比赛详情";

    std::string status, result, problems;
    switch (c.competition_status)
    {
    case competition::status::FINISHED:
        status = R"(<span class="badge text-bg-danger">已结束</span>)";
        break;
    case competition::status::NOT_STARTED:
        status = R"(<span class="badge text-bg-success">未开赛</span>)";
        break;
    case competition::status::ONGOING:
        status = R"(<span class="badge text-bg-primary">进行中</span>)";
        break;
    case competition::status::PAUSED:
        status = R"(<span class="badge text-bg-warning">暂停中</span>)";
        break;
    }

    if (c.competition_status == competition::status::FINISHED)
    {
		competition_result_info::teamlist ranking;
		competition_result_info::get_ranking(c, ranking);

        result = R"(
              <table class="table table-hover">
          		<tr>
            	  <th>名次</th>
            	  <th>参赛队伍</th>
            	  <th>分数</th>
          		</tr>)";

        for (auto team : ranking)
            result += R"(
          		<tr>
            	  <td><b>)" + ((team.score == -1) ? "--" : "#" + std::to_string(team.ranking)) + R"(</b></td>
            	  <td>)" + team.team_name + R"(</td>
            	  <td><b style="color:#dc3545;">)" + ((team.score == -1) ? "淘汰" : std::to_string(team.score)) + R"(</b></td>
          		</tr>)";

        result += R"(
        	  </table>
)";
    }
    else
    {
        result = R"(
			  <ul>
			    <li>比赛还未结束，暂无结果。</li>
			  </ul>
)";
    }

    problems = R"(
				<table class="table table-hover">
          		<tr>
            	  <th>题型</th>
            	  <th>预计使用数量</th>
            	  <th>实际使用数量</th>
            	  <th>题库剩余数量</th>
          		</tr>
          		<tr>
            	  <td>选择题</td>
            	  <td>)" + std::to_string(c.choice_count) + R"(</td>
            	  <td>)" + std::to_string(c.problems.choice_problems.size()) + R"(</td>
            	  <td>)" + std::to_string(problem_list::count_unused(pl.choice_problems)) + R"(</td>
          		</tr>
				<tr>
            	  <td>填空题</td>
            	  <td>)" + std::to_string(c.completion_count) + R"(</td>
            	  <td>)" + std::to_string(c.problems.completion_problems.size()) + R"(</td>
            	  <td>)" + std::to_string(problem_list::count_unused(pl.completion_problems)) + R"(</td>
          		</tr>
				<tr>
            	  <td>信息题</td>
            	  <td>)" + std::to_string(c.info_count) + R"(</td>
            	  <td>)" + std::to_string(c.problems.info_problems.size()) + R"(</td>
            	  <td>)" + std::to_string(problem_list::count_unused(pl.info_problems)) + R"(</td>
          		</tr>
        		</table>
)";

    content = R"(
        <div class="row gy-4">
          <div class="col-lg-8">

              <div class="row d-flex align-items-stretch">

                <div class="col-lg d-flex">
  			      <div class="portfolio-info w-100">
                    <h3>基本信息</h3>
                    <ul>
                      <li><strong>名称：</strong>)" + c.name + R"(</li>
                      <li><strong>描述：</strong>)" + c.description + R"(</li>
                      <li><strong>状态：</strong>)" + status + R"(</li>
                    </ul>
                  </div> 
                </div>
                &nbsp;
                <div class="col-lg d-flex">
  			      <div class="portfolio-info w-100">
                    <h3>参赛队伍</h3>
)";
    content += R"(
      <table class="table table-sm table-hover">
        <thead>
          <tr>
            <th>#</th>
            <th>队伍名称</th>
            <th>按键</th>
          </tr>
        </thead>
        <tbody id="teamContainer">
)";
    for (int i = 1; i <= c.teams.size(); i++)
        content += R"(
<tr>
<td><b>)" + std::to_string(i) + R"(</b></td>
<td>)" + c.teams[i - 1].first + R"(</td>
<td>)" + c.teams[i - 1].second + R"(</td>
</tr>)";

    content += R"(
        </tbody>
      </table>
)";
    content += R"(
                  </div> 
                </div>

              </div>

			<br>
			<div class="portfolio-info">
              <h3>题目信息</h3>
              <ul>
)" + problems + R"(
              </ul>
            </div>
          </div>

          <div class="col-lg-4">
            <div class="portfolio-info">
              <h3>操作</h3>
              <ul>
				<input class="btn btn-success my-2 my-sm-1" type="button" value="开始比赛" onclick="window.location.href='/main/competition/display?guid=)" + params["guid"] + R"(' ")" + ((c.competition_status == competition::status::NOT_STARTED || c.competition_status == competition::status::PAUSED) ? "" : " disabled=\"disabled\"") + R"(>
				<input class="btn btn-outline-primary my-2 my-sm-1" type="button" value="编辑" onclick="window.location.href='/main/competition/edit?guid=)" + params["guid"] + R"(' ")" + ((c.competition_status != competition::status::NOT_STARTED) ? " disabled=\"disabled\"" : "") + R"(>
				<input class="btn btn-outline-danger my-2 my-sm-1" type="button" value="删除" onclick="Delete() ")" + ((c.competition_status != competition::status::ONGOING) ? "" : " disabled=\"disabled\"") + R"(>
              </ul>
            </div>
			  <br>
			  <div class="portfolio-info">
              <h3>比赛结果</h3>
)" + result + R"(
              </div>
          </div>

        </div>

<script>
    function Delete(){
        swal({
          title: "删除比赛",
          text: "确实要删除 )" + c.name + R"( 吗？此操作不可逆！",
          icon: "warning",
          buttons: true,
          dangerMode: true,
          buttons: ["取消", "确定"],
        })
        .then((Delete) => {
          if (Delete) {
            $.ajax({
                type:'post', 
                url:'/main/competition/delete?guid=)" + c.guid + R"(',
                async:false,
                dataType:'text',
                data:'',
                success:function(res) {
                    window.location.href = "/main";
                }
            })
          }
        });
    }
</script>
)";
}

void competition_display_page::init()
{
    address = "/main/competition/display"; // ?ID={GUID}
	is_full_custom = true;

    is_request_login = true;
}
void competition_display_page::generate_page()
{
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return;
    }

    competition c;
    competition_session session;
    bool has_session = false;
    std::string initial_problem_html;
    {
        std::lock_guard<std::mutex> bank_lock(problem_bank_mutex);
        auto comp_mutex = get_competition_mutex(params["guid"]);
        std::lock_guard<std::mutex> comp_lock(*comp_mutex);

        c.load(params["guid"]);

        if (c.teams.size() == 0)
        {
            // error: TODO
            return;
        }

        c.competition_status = competition::ONGOING;
        c.save();

        has_session = session.load(c.guid, c.teams.size());
        if (has_session && session.current_id > 0 && session.current_type >= 0 && session.current_type <= 2)
        {
            lookup_and_render_problem(session.current_type, session.current_id, initial_problem_html);
        }
    }
	std::int64_t server_now = static_cast<std::int64_t>(std::time(nullptr));

    content = R"(
<!DOCTYPE html>
<html lang="zh">

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta content="width=device-width, initial-scale=1.0" name="viewport">

  <title>)" + c.name + R"( - 比赛进行中 - Responder | 简单易用的知识竞赛组织系统</title>
  <link rel="icon" href="/assets/favicon.ico" type="image/x-icon">

  <!-- Google Fonts -->
  <link href="/assets/fonts/fonts.css" rel="stylesheet">

  <!-- Vendor CSS Files -->
  <link href="/assets/vendor/aos/aos.css" rel="stylesheet">
  <link href="/assets/vendor/bootstrap/css/bootstrap.min.css" rel="stylesheet">
  <link href="/assets/vendor/bootstrap-icons/bootstrap-icons.css" rel="stylesheet">
  <link href="/assets/vendor/swiper/swiper-bundle.min.css" rel="stylesheet">
  <link href="/assets/vendor/notyf/notyf.min.css" rel="stylesheet">

  <!-- Template Main CSS File -->
  <link href="/assets/css/style.css" rel="stylesheet">
  <link href="/assets/css/competition_display.css?)" + PROGRAM_BUILD_S + R"(" rel="stylesheet">

  <!-- Addons JS File -->
  <script src="/assets/js/jquery-3.6.3.min.js"></script>
  <script src="/assets/js/sweetalert2.js"></script>

</head>

<body>

  <!-- ======= Top Bar ======= -->
  <section id="topbar" class="d-flex align-items-center">
    <div class="container d-flex justify-content-center justify-content-md-between">
      <div class="contact-info d-flex align-items-center">
        <i class="=d-flex align-items-center">成都七中科学技术协会</i>
      </div>
      <div class="social-links d-none d-md-flex align-items-center">
        <div class="=d-flex align-items-center" id="time"></div>
      </div>
    </div>
  </section>

  <main id="main">

    <!-- ======= Breadcrumbs ======= -->
    <section class="breadcrumbs">
      <div class="container">

        <div class="d-flex justify-content-between align-items-center">
          <h2><b>)" + c.name + R"(</b></h2>
          <ol>
            <li><a class="btn btn-danger my-2 my-sm-0" onclick="EndCompetition() " title="">退出比赛</a></li>
          </ol>
        </div>
      </div>
    </section><!-- End Breadcrumbs -->

    <!-- ======= Portfolio Details Section ======= -->
    <section id="portfolio-details" class="portfolio-details">

    <div class="container quiz-container user-select-none">
      <div class="row gy-3">

        <!-- 左侧题目 -->
        <div class="col-lg-8">
          <div class="portfolio-info quiz-card">

            <div class="quiz-header">
              <h3>题目</h3>
              <div class="d-flex flex-wrap align-items-center gap-1">
                <button class="btn btn-outline-secondary btn-sm" onclick="reshowLastProblem() " id="reshowproblem" disabled>重新显示上一题</button>
                <button class="btn btn-primary btn-sm" onclick="ShowNextInfo() " id="nextinfo" disabled>下一信息</button>
                <button class="btn btn-danger btn-sm" onclick="ShowAnswer() " id="showanswer" disabled>显示答案</button>

                <div class="btn-group">
                  <button class="btn btn-success btn-sm" id="nextproblem" onclick="buttonNextProblem() ">下一选择题</button>
                  <button class="btn btn-success btn-sm" id="problemsettings" data-bs-toggle="modal" data-bs-target="#modalQuestionSettings"><i class="bi bi-gear-fill"></i></button>
                </div>
              </div>
            </div>

            <div aria-live="polite" aria-atomic="true" class="position-relative">
              <div class="toast-container top-0 end-0 p-3" style="z-index:1000">

                <!-- Then put toasts within -->
                <div class="toast" id="confirmAns" role="alert" aria-live="assertive" aria-atomic="true" data-bs-autohide="false">
                  <div class="toast-header">
                    <strong class="me-auto">评价作答</strong>
                    <small class="text-body-secondary">自动计分模块</small>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                  </div>
                  <div class="toast-body">
                    队伍 <b id="teamName_t1">Team</b> 是否答对了问题？
                    <div class="mt-2 pt-2 border-top">
                      <button type="button" class="btn btn-success btn-sm" onclick="autoScoring_Correct() ">是</button>
                      <button type="button" class="btn btn-danger btn-sm" onclick="autoScoring_Incorrect() ">否</button>
                    </div>
                  </div>
                </div>

                <div class="toast" id="correctAns" role="alert" aria-live="assertive" aria-atomic="true">
                  <div class="toast-header">
                    <strong class="me-auto">评价作答</strong>
                    <small class="text-body-secondary">自动计分模块</small>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                  </div>
                  <div class="toast-body">
                    队伍 <b id="teamName_t2">Team</b> 作答<b class="text-success">正确</b><span id="correctScoreChange">，已自动加分</span>
                  </div>
                </div>

                <div class="toast" id="incorrectAns" role="alert" aria-live="assertive" aria-atomic="true">
                  <div class="toast-header">
                    <strong class="me-auto">评价作答</strong>
                    <small class="text-body-secondary">自动计分模块</small>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                  </div>
                  <div class="toast-body">
                    队伍 <b id="teamName_t3">Team</b> 作答<b class="text-danger">错误</b><span id="incorrectScoreChange">，已自动扣分</span>
                  </div>
                </div>

                <div class="toast" id="autoScoringHint" role="alert" aria-live="assertive" aria-atomic="true">
                  <div class="toast-header">
                    <strong class="me-auto">提示</strong>
                    <small class="text-body-secondary">自动计分模块</small>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                  </div>
                  <div class="toast-body">
                    <b>自动计分模块已经<span class="text-success">开启</span></b><br>可在抢答/轮答设置页调整相关设置。
                  </div>
                </div>
                <div class="toast" id="manualScoringHint" role="alert" aria-live="assertive" aria-atomic="true">
                  <div class="toast-header">
                    <strong class="me-auto">提示</strong>
                    <small class="text-body-secondary">自动计分模块</small>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                  </div>
                  <div class="toast-body">
                    <b>自动计分模块已经<span class="text-danger">关闭</span></b><br>可在抢答设置页重新开启。
                  </div>
                </div>

              </div>
            </div>


            <div id="problems" class="quiz-problem">

              <div id="problemarea" style="display:none">

                <p class="h3">
                  <span class="badge bg-primary" id="problemtitle"></span>
                </p>

                <div id="dymproblemarea">

                  <div class="quiz-question" id="task"></div>

                  <div class="quiz-option" id="d1area">
                    <b id="ta"></b> <span id="data1"></span>
                  </div>

                  <div class="quiz-option" id="d2area">
                    <b id="tb"></b> <span id="data2"></span>
                  </div>

                  <div class="quiz-option" id="d3area">
                    <b id="tc"></b> <span id="data3"></span>
                  </div>

                  <div class="quiz-option" id="d4area">
                    <b id="td"></b> <span id="data4"></span>
                  </div>

                  <div id="ansarea" style="margin-top:15px;">
                    <p class="h3">
                      <span class="badge bg-success" id="answertitle">答案</span>
                    </p>
                    <div class="h2"><b id="answer"></b></div>
                  </div>

                </div>
              </div>

              <div id="waitingarea" class="quiz-center">
                <h1>比赛即将开始，请耐心等待</h1>
                <p>使用右侧“切换题目”开始</p>
              </div>

            </div>
          </div>
        </div>

        <!-- 右侧 -->
        <div class="col-lg-4">

          <div class="portfolio-info quiz-card">
            <div class="quiz-header">
              <h3 id="timertitle">抢答</h3>
              <button class="btn btn-success btn-sm" id="changetimermode" onclick="buttonSwitchTimerMode() ">切换轮答</button>
            </div>

            <div class="quiz-panel" style="height:165px">

              <div>

                <div class="quiz-responder-text" id="Responder1">
                  <span id="NowInfo">等待开始</span>
                  <p id="RoundInfo" style="display:none">切换下一题以自动开始轮答</p>
                </div>

                <div class="quiz-timer-main" id="Timer1">
                  <span id="NowTime">00:03</span>
                </div>

                <div class="quiz-panel-controls" id="ResponderControls">
                  <button class="btn btn-success btn-sm" id="ResponderSwitcher" onclick="ResponderFlow() ">
                    <i class="bi bi-play-fill"></i>
                  </button>
                  <button class="btn btn-primary btn-sm" id="ResponderSettings" onclick="SetResponder() ">
                    <i class="bi bi-gear-fill"></i>
                  </button>
                </div>

                <div class="quiz-panel-controls" id="RoundControls" style="display:none">
                  <button class="btn btn-success btn-sm" id="RoundSwitcher" onclick="buttonSwitchRoundFlow() ">
                    <i class="bi bi-play-fill"></i>
                  </button>
                  <button class="btn btn-primary btn-sm" id="RoundSettings" onclick="SetResponder() ">
                    <i class="bi bi-gear-fill"></i>
                  </button>
                </div>

                <div class="quiz-panel-controls" id="TimerControls" style="display:none">
                  <button class="btn btn-success btn-sm" id="IntervalStart" onclick="Interval_Start() ">
                    <i class="bi bi-play-fill"></i>
                  </button>
                  <button class="btn btn-primary btn-sm" id="IntervalSet" onclick="showSecondInput() ">
                    <i class="bi bi-gear-fill"></i>
                  </button>
                  <button class="btn btn-danger btn-sm" id="IntervalStop" onclick="Interval_Stop() " disabled>
                    <i class="bi bi-stop-fill"></i>
                  </button>
                </div>

              </div>
            </div>
          </div>

          <br>

          <!-- 比分 -->
          <div class="portfolio-info quiz-card quiz-score-card">

            <table class="table table-sm table-borderless" style="margin-bottom:0px">
              <thead>
                <tr>
                  <th>参赛队伍</th>
                  <th>分数</th>
                  <th>操作</th>
                </tr>
              </thead>
              <tbody>
)";

#define ID std::to_string(i)
    for (int i = 0; i < c.teams.size(); i++)
        content += R"(
                <tr>
                  <td class="quiz-team-name">)" + c.teams[i].first + R"(</td>
                  <td class="quiz-team-score text-center" id="teamscore)" + ID + R"(" style="color: #dc3545;">)" + std::to_string(c.team_scores[i]) + R"(</td>
                  <td class="quiz-team-actions">
                    <div class="btn-group">
                      <button class="btn btn-success btn-sm" id="btn1)" + ID + R"(" onclick="score_add()" + ID + R"() "><i class="bi bi-plus-lg"></i></button>
                      <button class="btn btn-danger btn-sm" id="btn2)" + ID + R"(" onclick="score_sub()" + ID + R"() "><i class="bi bi-dash-lg"></i></button>
                      <button class="btn btn-secondary btn-sm" onclick="score_edit()" + ID + R"() "><i class="bi bi-three-dots"></i></button>
                    </div>
                  </td>
                </tr>
)";
#undef ID
    content += R"(
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>

    </section><!-- End Portfolio Details Section -->

  </main><!-- End #main -->

  <div class="modal fade" tabindex="-1" data-bs-backdrop="static" role="dialog" id="modalQuestionSettings">
    <div class="modal-dialog modal-dialog-centered" role="document">
      <div class="modal-content rounded-4 shadow">
        <div class="modal-header p-5 pb-4 border-bottom-0">
          <h1 class="mb-0 fs-4">题目设置</h1>
          <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
        </div>

        <div class="modal-body p-5 pt-0">
          <div class="mb-3">
            <label class="form-label">下一题的题目类型</label>
            <p class="form-text text-danger" id="questionTypeError" style="display:none">本轮轮答还未结束，无法切换题目类型。</p>
            <div class="d-flex gap-4">
              <div class="form-check">
                <input class="form-check-input" type="radio" name="questionType" id="radioChoice" checked>
                <label class="form-check-label" for="radioChoice">选择题</label>
              </div>
              <div class="form-check">
                <input class="form-check-input" type="radio" name="questionType" id="radioFillIn">
                <label class="form-check-label" for="radioFillIn">填空题</label>
              </div>
              <div class="form-check">
                <input class="form-check-input" type="radio" name="questionType" id="radioInfo">
                <label class="form-check-label" for="radioInfo">信息题</label>
              </div>
            </div>
          </div>

          <div class="d-flex mb-3 gap-1">
            <button type="button" class="btn btn-secondary btn-sm w-48" id="resetCurrentQuestion">重置题号</button>
            <button type="button" class="btn btn-danger btn-sm w-48" id="discardCurrentQuestion">弃用当前展示的题目</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- ======= Footer ======= -->
  <footer id="footer">
	<center>
	  <br>
      Copyright &copy; 2023-2026 <strong><span>Science &amp; Technology Association of Chengdu No.7 High School (STA)</span></strong>. All Rights Reserved.
	</center>
  </footer><!-- End Footer -->

  <script>
    var competition_guid = ")" + c.guid + R"(";
    var Teams = )";

    std::string TeamsInfo;
	TeamsInfo = "[";
	for (auto& team : c.teams)
		TeamsInfo += "\"" + team.first + "\",";
	TeamsInfo[TeamsInfo.size() - 1] = ']';
    TeamsInfo += ";";

    content += TeamsInfo + R"(
    var scores = )";

    TeamsInfo = "[";
    for (auto& team : c.team_scores)
        TeamsInfo += std::to_string(team) + ",";
    TeamsInfo[TeamsInfo.size() - 1] = ']';
    TeamsInfo += ";";

    content += TeamsInfo + R"(
    var team_keys = )";

    TeamsInfo = "[";
    for (int i = 0; i < c.teams.size(); i++)
        TeamsInfo += std::to_string(i) + ",";
    TeamsInfo[TeamsInfo.size() - 1] = ']';
    TeamsInfo += ";";

    content += TeamsInfo + R"(
    var has_initial_session = )" + std::string(has_session ? "true" : "false") + R"(;
    var initial_session = )" + safe_json_for_script(session.to_json()) + R"(;
    var server_now = )" + std::to_string(server_now) + R"(;
    var initial_problem_html = )" + safe_json_for_script(json(initial_problem_html)) + R"(;
    var last_problem_type = )" + std::to_string(c.last_problem_type) + R"(;
    var last_problem_id = )" + std::to_string(c.last_problem_id) + R"(;
  </script>

  <!-- Vendor JS Files -->
  <script src="/assets/vendor/aos/aos.js"></script>
  <script src="/assets/vendor/bootstrap/js/bootstrap.bundle.min.js"></script>
  <script src="/assets/vendor/isotope-layout/isotope.pkgd.min.js"></script>
  <script src="/assets/vendor/swiper/swiper-bundle.min.js"></script>
  <script src="/assets/vendor/waypoints/noframework.waypoints.js"></script>
  <script src="/assets/vendor/notyf/notyf.min.js"></script>

  <!-- Template Main JS File -->
  <script src="/assets/js/main.js"></script>
  <script src="/assets/js/competition_display.js?)" + PROGRAM_BUILD_S + R"("></script>

</body>

</html>

)";
}

void competition_display_scores_handler::init()
{
    address = "/main/competition/display/scores";

    is_request_login = true;
}
std::string competition_display_scores_handler::generate_response()
{
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return "fail : guid not found";
    }

    auto comp_mutex = get_competition_mutex(params["guid"]);
    std::lock_guard<std::mutex> comp_lock(*comp_mutex);

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return "fail : guid not existed";
    }

    if (c.competition_status != competition::status::ONGOING)
    {
        // error: TODO
        return "fail : competition isn't ongoing";
    }

    if (params.find("team") == params.end()
        || params.find("score") == params.end())
    {
        // error: TODO
        return "fail : team or score not found";
    }

    int team_idx = -1;
    int new_score = 0;
    if (!try_parse_int(params["team"], team_idx) || team_idx < 0 || static_cast<size_t>(team_idx) >= c.team_scores.size()
        || !try_parse_int(params["score"], new_score))
    {
        return "fail : invalid team or score";
    }

	c.team_scores[team_idx] = new_score;

	std::ofstream fout("data/competitions/" + c.guid + "/scores.json");

	if (fout.fail())
    {
        // error: TODO
        return "fail : can't write scores.json";
    }

	for (auto score : c.team_scores)
        fout << score << std::endl;
    fout.close();

    return "success";
}

void competition_display_problem_handler::init()
{
    address = "/main/competition/display/problem";

    is_request_login = true;
}
std::string competition_display_problem_handler::generate_response()
{
	std::lock_guard<std::mutex> bank_lock(problem_bank_mutex);
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return "fail : guid not found";
    }

    auto comp_mutex = get_competition_mutex(params["guid"]);
    std::lock_guard<std::mutex> comp_lock(*comp_mutex);

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return "fail : guid not existed";
    }

    if (c.competition_status != competition::status::ONGOING)
    {
        // error: TODO
        return "fail : competition isn't ongoing";
    }

    if (params.find("reshow") != params.end() && params["reshow"] == "1")
    {
        competition_session session;
        bool has_session = session.load(c.guid, c.teams.size());

        int target_type = -1;
        int target_id = 0;
        if (params.find("type") != params.end() && params.find("id") != params.end())
        {
            int parsed_type = -1, parsed_id = 0;
            if (try_parse_int(params["type"], parsed_type) && try_parse_int(params["id"], parsed_id))
            {
                target_type = parsed_type;
                target_id = parsed_id;
            }
        }
        if (target_id <= 0 && has_session && session.current_id > 0)
        {
            target_type = session.current_type;
            target_id = session.current_id;
        }
        if (target_id <= 0 && c.last_problem_id > 0)
        {
            target_type = c.last_problem_type;
            target_id = c.last_problem_id;
        }

        std::string reshow_html;
        if (!lookup_and_render_problem(target_type, target_id, reshow_html))
            return "fail : no previous problem";

        bool saved = competition_session::update(c.guid, c.teams.size(), [&](competition_session& s, bool) {
            bool switching_problem = (s.current_type != target_type || s.current_id != target_id);
            if (params.find("state") != params.end())
            {
                json client_state = json::parse(params["state"], nullptr, false);
                if (client_state.is_object())
                    s.apply_client_state(client_state, c.teams.size());
            }
            if (switching_problem)
            {
                s.current_type = target_type;
                s.current_id = target_id;
                s.shown_at = static_cast<std::int64_t>(std::time(nullptr));
                s.info_id = 0;
                s.show_answer = false;
                s.discarded = false;
                s.confirm_pending = false;
            }
            if (s.shown_at <= 0)
                s.shown_at = static_cast<std::int64_t>(std::time(nullptr));
            if (s.problem_number <= 0)
                s.problem_number = 1;
        });

        if (!saved)
            return "fail : session_save_failed|" + std::to_string(target_type) + "|" + std::to_string(target_id);

        return reshow_html;
    }

    if (params.find("type") == params.end())
    {
        // error: TODO
        return "fail : type not found";
    }

    int req_type = -1;
    if (!try_parse_int(params["type"], req_type) || req_type < 0 || req_type > 2)
        return "fail : invalid type";

    if (params.find("requestcomplexity") != params.end()) // 用于轮答，请求一种数量足够的难度，失败则返回0; 参数只用于标记，忽略其值
    {
		int spare_count[4]; // 1-3分别对应难度1-3

        // 加载可用题目数量
        switch (req_type) // Type: 0-选择题，1-填空题，2-信息题
        {
        case 0:
            spare_count[1] = problem_list::count_unused(pl.choice_problems, 1);
            spare_count[2] = problem_list::count_unused(pl.choice_problems, 2);
            spare_count[3] = problem_list::count_unused(pl.choice_problems, 3);
            break;
        case 1:
            spare_count[1] = problem_list::count_unused(pl.completion_problems, 1);
            spare_count[2] = problem_list::count_unused(pl.completion_problems, 2);
            spare_count[3] = problem_list::count_unused(pl.completion_problems, 3);
            break;
        case 2:
            spare_count[1] = problem_list::count_unused(pl.info_problems, 1);
            spare_count[2] = problem_list::count_unused(pl.info_problems, 2);
            spare_count[3] = problem_list::count_unused(pl.info_problems, 3);
            break;
        }

        if (spare_count[1] < c.teams.size()
            && spare_count[2] < c.teams.size()
            && spare_count[3] < c.teams.size())
        {
			// 每种难度题目数量都不足以支持轮答
            return "0";
        }

        int target_complexity = 0;
        do
        {
			target_complexity = RandomGenerator::getInstance().getInt(1, 3);
        } while (spare_count[target_complexity] < c.teams.size());

		return std::to_string(target_complexity); // 成功找到一种难度，返回该难度
    }

    int requested_complexity = 0; // 默认为0：不限
    if (params.find("complexity") != params.end()) // 用于轮答，以前请求了固定的难度
    {
		if (!try_parse_int(params["complexity"], requested_complexity) || requested_complexity < 0 || requested_complexity > 3)
            requested_complexity = 0;
    }
    
    std::string p_content, p_info[5], p_ans;
    int total_id = 0, random_id = 0, total = 0;
    
    switch (req_type) // Type: 0-选择题，1-填空题，2-信息题
    {
	case 0: // 选择题
        if (requested_complexity != 0)
        {
			std::vector<std::pair<problem_choice, int>> filtered_problems; // first: 题目信息，second: pl_unused题库中的下标
            for (int i = 0; i < pl_unused.choice_problems.size(); i++)
            {
                if (pl_unused.choice_problems[i].first.complexity == requested_complexity)
                    filtered_problems.push_back(std::make_pair(pl_unused.choice_problems[i].first, i));
			}

			if (filtered_problems.size() == 0)
            {
                // return "fail : no unused choice problems with requested complexity";
				requested_complexity = 0; // 降级为不限难度
            }
            else
            {
                random_id = RandomGenerator::getInstance().getInt(0, filtered_problems.size() - 1);
                random_id = filtered_problems[random_id].second; // 在 pl_unused.choice_problems 中的下标
            }
        }
        if (requested_complexity == 0)
        {
            total = problem_list::count_unused(pl.choice_problems);
            if (total == 0)
                return "fail : no unused choice problems";
            random_id = RandomGenerator::getInstance().getInt(0, total - 1);
        }
        
        // 保存第 random_id 个未使用的题目信息
        p_content = pl_unused.choice_problems[random_id].first.content;
		p_ans = pl_unused.choice_problems[random_id].first.answer;
        for (int i = 0; i < 4; i++)
			p_info[i + 1] = pl_unused.choice_problems[random_id].first.options[i];

        // 标记已使用
		total_id = pl_unused.choice_problems[random_id].first.id; // 主题库中文件名id与题目信息中id一致，与pl中vector列表的下标相差1
		c.problems.choice_problems.push_back(std::make_pair(pl_unused.choice_problems[random_id].first, 1)); // 加入比赛题目列表，标记为已使用
		c.problems.save("data/competitions/" + c.guid + "/problems.json");

        pl_unused.choice_problems.erase(pl_unused.choice_problems.begin() + random_id);

        pl.choice_problems[total_id - 1].second = 1; // 在主题库中标记为已使用
        pl.save("data/problems/problem_list.json");

        break;

    case 1: // 填空题
        if (requested_complexity != 0)
        {
            std::vector<std::pair<problem_completion, int>> filtered_problems; // first: 题目信息，second: pl_unused题库中的下标
            for (int i = 0; i < pl_unused.completion_problems.size(); i++)
            {
                if (pl_unused.completion_problems[i].first.complexity == requested_complexity)
                    filtered_problems.push_back(std::make_pair(pl_unused.completion_problems[i].first, i));
            }

            if (filtered_problems.size() == 0)
            {
                // return "fail : no unused choice problems with requested complexity";
                requested_complexity = 0; // 降级为不限难度
            }
            else
            {
                random_id = RandomGenerator::getInstance().getInt(0, filtered_problems.size() - 1);
                random_id = filtered_problems[random_id].second; // 在 pl_unused.completion_problems 中的下标
            }
        }
        if (requested_complexity == 0)
        {
            total = problem_list::count_unused(pl.completion_problems);
            if (total == 0)
                return "fail : no unused completion problems";
            random_id = RandomGenerator::getInstance().getInt(0, total - 1);
        }

        // 保存第 random_id 个未使用的题目信息
        p_content = pl_unused.completion_problems[random_id].first.content;
        p_ans = pl_unused.completion_problems[random_id].first.answer;

        // 标记已使用
        total_id = pl_unused.completion_problems[random_id].first.id; // 主题库中文件名id与题目信息中id一致，与pl中vector列表的下标相差1
        c.problems.completion_problems.push_back(std::make_pair(pl_unused.completion_problems[random_id].first, 1)); // 加入比赛题目列表，标记为已使用
        c.problems.save("data/competitions/" + c.guid + "/problems.json");

        pl_unused.completion_problems.erase(pl_unused.completion_problems.begin() + random_id);

        pl.completion_problems[total_id - 1].second = 1; // 在主题库中标记为已使用
        pl.save("data/problems/problem_list.json");

        break;

    case 2: // 信息题
        if (requested_complexity != 0)
        {
            std::vector<std::pair<problem_info, int>> filtered_problems; // first: 题目信息，second: pl_unused题库中的下标
            for (int i = 0; i < pl_unused.info_problems.size(); i++)
            {
                if (pl_unused.info_problems[i].first.complexity == requested_complexity)
                    filtered_problems.push_back(std::make_pair(pl_unused.info_problems[i].first, i));
            }

            if (filtered_problems.size() == 0)
            {
                // return "fail : no unused choice problems with requested complexity";
                requested_complexity = 0; // 降级为不限难度
            }
            else
            {
                random_id = RandomGenerator::getInstance().getInt(0, filtered_problems.size() - 1);
                random_id = filtered_problems[random_id].second; // 在 pl_unused.info_problems 中的下标
            }
        }
        if (requested_complexity == 0)
        {
            total = problem_list::count_unused(pl.info_problems);
            if (total == 0)
                return "fail : no unused info problems";
            random_id = RandomGenerator::getInstance().getInt(0, total - 1);
        }

        // 保存第 random_id 个未使用的题目信息
        p_content = pl_unused.info_problems[random_id].first.content;
        p_ans = pl_unused.info_problems[random_id].first.answer;
        for (int i = 0; i < 4; i++)
            p_info[i + 1] = pl_unused.info_problems[random_id].first.info[i];

        // 标记已使用
        total_id = pl_unused.info_problems[random_id].first.id; // 主题库中文件名id与题目信息中id一致，与pl中vector列表的下标相差1
        c.problems.info_problems.push_back(std::make_pair(pl_unused.info_problems[random_id].first, 1)); // 加入比赛题目列表，标记为已使用
        c.problems.save("data/competitions/" + c.guid + "/problems.json");

        pl_unused.info_problems.erase(pl_unused.info_problems.begin() + random_id);

        pl.info_problems[total_id - 1].second = 1; // 在主题库中标记为已使用
        pl.save("data/problems/problem_list.json");

        break;
    }

    c.last_problem_type = req_type;
    c.last_problem_id = total_id;
    c.save();

    bool saved = competition_session::update(c.guid, c.teams.size(), [&](competition_session& s, bool) {
        bool has_client_state = false;
        if (params.find("state") != params.end())
        {
            json client_state = json::parse(params["state"], nullptr, false);
            if (client_state.is_object())
            {
                s.apply_client_state(client_state, c.teams.size());
                has_client_state = true;
            }
        }
        if (!has_client_state)
        {
            s.problem_number += 1;
            s.default_type = req_type;
        }
        s.current_type = req_type;
        s.current_id = total_id;
        s.shown_at = static_cast<std::int64_t>(std::time(nullptr));
        s.info_id = 0;
        s.show_answer = false;
        s.discarded = false;
        s.confirm_pending = false;
    });

    if (!saved)
        return "fail : session_save_failed|" + std::to_string(req_type) + "|" + std::to_string(total_id);

    return render_problem_html(req_type, total_id, p_content, p_info, p_ans);
}

void competition_display_session_handler::init()
{
    address = "/main/competition/display/session";

    is_request_login = true;
}
std::string competition_display_session_handler::generate_response()
{
    if (params.find("guid") == params.end())
        return "fail : guid not found";

    auto comp_mutex = get_competition_mutex(params["guid"]);
    std::lock_guard<std::mutex> comp_lock(*comp_mutex);

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
        return "fail : guid not existed";

    if (c.competition_status != competition::status::ONGOING)
        return "fail : competition isn't ongoing";

    bool saved = competition_session::update(c.guid, c.teams.size(), [&](competition_session& s, bool) {
        if (params.find("state") != params.end())
        {
            json client_state = json::parse(params["state"], nullptr, false);
            if (client_state.is_object())
                s.apply_client_state(client_state, c.teams.size());
        }

        if (params.find("timer_action") != params.end())
        {
            const std::string& action = params["timer_action"];
            int parsed = 0;
            if (params.find("timer_mode") != params.end() && try_parse_int(params["timer_mode"], parsed))
                s.timer_mode = std::clamp(parsed, 0, 2);

            if (action == "start")
            {
                if (params.find("timer_duration") != params.end() && try_parse_int(params["timer_duration"], parsed))
                    s.timer_duration = std::max(0, parsed);
                s.timer_started_at = static_cast<std::int64_t>(std::time(nullptr));
                s.timer_paused = false;
                s.timer_remaining = 0;
            }
            else if (action == "pause")
            {
                if (params.find("timer_remaining") != params.end() && try_parse_int(params["timer_remaining"], parsed))
                    s.timer_remaining = std::max(0, parsed);
                s.timer_started_at = 0;
                s.timer_paused = true;
            }
            else if (action == "stop")
            {
                s.timer_started_at = 0;
                s.timer_paused = false;
                s.timer_remaining = 0;
                s.timer_duration = 0;
                s.timer_mode = 0;
            }
        }
    });

    return saved ? "success" : "fail : can't write session.json";
}

void competition_enddisplay_handler::init()
{
    address = "/main/competition/display/enddisplay";

    is_request_login = true;
}
std::string competition_enddisplay_handler::generate_response()
{
    if (params.find("guid") == params.end())
    {
        // error: TODO
        return "fail : guid not found";
    }

    auto comp_mutex = get_competition_mutex(params["guid"]);
    std::lock_guard<std::mutex> comp_lock(*comp_mutex);

    competition c;
    c.load(params["guid"]);

    if (c.teams.size() == 0)
    {
        // error: TODO
        return "fail : guid not existed";
    }

    if (c.competition_status != competition::status::ONGOING)
    {
        // error: TODO
        return "fail : competition isn't ongoing";
    }

    if (params.find("action") == params.end())
    {
        // error: TODO
        return "fail : action not found";
    }

    int action_val = -1;
    if (!try_parse_int(params["action"], action_val) || (action_val != 0 && action_val != 1))
        return "fail : invalid action";

    switch (action_val)
    {
    case 0:
        c.competition_status = competition::status::PAUSED;
        competition_session::clear(c.guid);
        break;

    case 1:
        c.competition_status = competition::status::FINISHED;
        competition_session::clear(c.guid);
        break;
    }
    c.save();

    return "success";
}
