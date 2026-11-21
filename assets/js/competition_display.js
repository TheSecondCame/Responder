// competition_display.js : Responder 比赛展示页面浏览器端代码实现。

// 版权声明：
// Copyright (C) 2023-2026 Science & Technology Association of Chengdu No.7 High School (STA). All Rights Reserved.

/* -- 页面基础控制 -- */
const notyf = new Notyf({
    duration: 2000,
    position: {
        x: 'center',
        y: 'bottom',
    }
});

// 音频预加载
var audioRing = new Audio("/assets/sound/ring.wav");
var audioTimer = new Audio("/assets/sound/timer.wav");

// 显示当前时间
function synchronization() {
    var curDate = new Date();
    if (curDate.getMinutes() < 10)
        document.getElementById("time").innerHTML = "当前时间：" + curDate.getFullYear() + "/" + (curDate.getMonth() + 1) + "/" + curDate.getDate() + " " + curDate.getHours() + ":0" + curDate.getMinutes();
    else
        document.getElementById("time").innerHTML = "当前时间：" + curDate.getFullYear() + "/" + (curDate.getMonth() + 1) + "/" + curDate.getDate() + " " + curDate.getHours() + ":" + curDate.getMinutes();
}
var int = self.setInterval("synchronization() ", 1000);

// 控制页面全屏
function requestFullScreen(element) {
    var de = document.querySelector(element) || document.documentElement;
    if (de.requestFullscreen) {
        de.requestFullscreen();
    } else if (de.mozRequestFullScreen) {
        de.mozRequestFullScreen();
    } else if (de.webkitRequestFullScreen) {
        de.webkitRequestFullScreen();
    }
}
function exitFullscreen(element) {
    var de = document.querySelector(element) || document.documentElement;
    if (de.exitFullscreen) {
        de.exitFullscreen();
    } else if (de.mozCancelFullScreen) {
        de.mozCancelFullScreen();
    } else if (de.webkitCancelFullScreen) {
        de.webkitCancelFullScreen();
    }
}

// 提示型警告弹窗：默认3秒后自动关闭，函数在弹窗关闭后才返回
async function showAutoCloseWarningSwal(warningText, warningTitle) {
    const result = await Swal.fire({
        title: warningTitle || '警告',
        text: warningText,
        icon: 'warning',
        timer: 3000,
        timerProgressBar: true,
        showConfirmButton: false,
        allowOutsideClick: false,
        allowEscapeKey: false,
    });

    return result;
}

// 结束比赛
var isRestoringSession = false;
var isConfirmPending = false;
var isEndingCompetition = false;
var sessionSyncChain = Promise.resolve();

function EndCompetition() {
    Swal.fire({
        title: "退出当前正在进行的比赛？",
        text: "退出当前正在进行的比赛会导致比赛中断。",
        icon: "warning",
        showCancelButton: true,
        confirmButtonText: "确定",
        cancelButtonText: "取消",
    })
        .then((willEnd) => {
            if (willEnd.isConfirmed) {
                exitFullscreen();
                window.onbeforeunload = undefined;

                Swal.fire({
                    title: "退出比赛",
                    text: "要结束比赛还是暂停比赛？",
                    icon: "warning",
                    showCancelButton: true,
                    confirmButtonText: "结束",
                    cancelButtonText: "暂停",
                })
                    .then((isFinished) => {
                        var isFinished = isFinished.isConfirmed ? 1 : 0;
                        isEndingCompetition = true;
                        $.ajax({
                            type: 'post',
                            url: "/main/competition/display/enddisplay?guid=" + competition_guid + "&action=" + isFinished,
                            async: true,
                            dataType: 'text',
                            data: '',
                            success: function (res) {
                                if (res != "success") {
                                    isEndingCompetition = false;
                                    notyf.error("尝试退出比赛时出现意外错误。");
                                    return;
                                }
                                window.location.href = "/main/competition/detail?guid=" + competition_guid;
                            },
                            error: function (xhr, status, error) {
                                isEndingCompetition = false;
                                notyf.error("尝试退出比赛时出错。请检查网络连接并重试。");
                            }
                        });
                    });
            }
        });
}
window.onbeforeunload = function (e) {
    var e = window.event || e;
    e.returnValue = ("离开当前页面会导致正在进行的比赛中断且丢失进度");
}

// 自动计分模块Toast定义
var confirmAnsToast, correctToast, incorrectToast, hintToast, manualhintToast;
document.addEventListener('DOMContentLoaded', function () {
    confirmAnsToast = new bootstrap.Toast(document.getElementById('confirmAns'));
    correctToast = new bootstrap.Toast(document.getElementById('correctAns'));
    incorrectToast = new bootstrap.Toast(document.getElementById('incorrectAns'));
    hintToast = new bootstrap.Toast(document.getElementById('autoScoringHint'));
    manualhintToast = new bootstrap.Toast(document.getElementById('manualScoringHint'));
    restoreSessionOnLoad();
});

// 开始比赛
Swal.fire({
    title: "进入全屏",
    text: "是否自动进入全屏模式并尝试强行适配UI尺寸？",
    icon: "warning",
    showCancelButton: true,
    confirmButtonText: "确定",
    cancelButtonText: "取消",
})
    .then((Fullscreen) => {
        if (Fullscreen.isConfirmed) {
            function bodyScale() {
                var devicewidth = document.documentElement.clientWidth;
                var scale = devicewidth / 1470; // TODO: for 16:9 only, adjust if using 4:3 or 16:10 screens
                document.body.style.zoom = scale;
            }
            window.onload = window.onresize = function () {
                bodyScale();
            };
            requestFullScreen();
        }
        if (typeof has_initial_session === 'undefined' || !has_initial_session) {
            hintToast.show();
        }
    });

/* -- 自动计分模块 -- */
var IsAutoScoring = 1; // 是否开启自动计分
var SelectedTeam = 0; // 当前自动记分选中的队伍，0表示未选中
var autoAddValue = 1, autoSubValue = 1;  // 自动记分的加减分值
var IsEarlyResponderWarning = 1; // 提早抢答时是否进行警告
var EarlyResponderWarningTriggerCount = 3; // 触发自动扣分的提早抢答次数
var EarlyResponderWarningSubValue = 1; // 提早抢答时自动扣分分值

var IsAutoComplexity = 1; // 是否在轮答中控制难度相同
var TargetComplexity = 0; // 当前轮答中题目难度

/* -- 倒计时与抢答轮答模块 -- */
// 倒计时
var Seconds = 15, defaultTime = 15;
function Sec2Time(Second) {
    var Ans = "";
    if (Second < 60)
        Ans = "00:";
    else {
        var Minute = parseInt(Second / 60);
        if (Minute < 10)
            Ans = "0";
        Ans += Minute + ":";
        Second %= 60;
    }
    if (Second < 10)
        Ans += "0";
    Ans += Second;
    return Ans;
}
function Interval_setTime(Time) {
    Seconds = Time;
    document.getElementById("NowTime").innerHTML = Sec2Time(Time);
}

var IntervalMode = 0;
// 0: 正常倒计时
// 1: 倒计时后开始抢答
// 2: 倒计时后开始轮答
var IsResponderFlowRunning = 0;
var IsRoundFlowRunning = 0;

var IsWinnerTakesAll = 1; // 是否开启轮答赢者通吃模式

// 轮、抢答前后倒计时时间
var timeBefore = 3;
var timeAfter = 15;

var IsPaused = 0, IsStarted = 0;
var IntervalID;
function Interval_Start() {
    if (!IsStarted) {
        document.getElementById("IntervalSet").disabled = true;
        document.getElementById("IntervalStop").disabled = false;
        document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-pause-fill'>";

        IntervalID = setInterval(Interval_Counting, 1000);
        IsStarted = 1;
        syncSession({ timer_action: 'start', timer_duration: Seconds, timer_mode: IntervalMode });
    }
    else {
        if (!IsPaused) {
            clearInterval(IntervalID);
            document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-play-fill'>";
            IsPaused = 1;
            syncSession({ timer_action: 'pause', timer_remaining: Seconds, timer_mode: IntervalMode });
        }
        else {
            IntervalID = setInterval(Interval_Counting, 1000);
            document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-pause-fill'>";
            IsPaused = 0;
            syncSession({ timer_action: 'start', timer_duration: Seconds, timer_mode: IntervalMode });
        }
    }
}
function Interval_Stop() {
    clearInterval(IntervalID);
    Interval_setTime(defaultTime);
    if (IsResponderFlowRunning == 1 || IsRoundFlowRunning == 1) {
        Interval_setTime(timeBefore);
    }
    document.getElementById("IntervalStart").disabled = false;
    document.getElementById("IntervalSet").disabled = false;
    document.getElementById("IntervalStop").disabled = true;

    IsPaused = 0;
    IsStarted = 0;
    IntervalMode = 0;
    document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-play-fill'>";
    syncSession({ timer_action: 'stop' });
}
function Interval_Finish() {
    Interval_setTime(defaultTime);
    document.getElementById("IntervalStart").disabled = false;
    document.getElementById("IntervalSet").disabled = false;
}
function Interval_Counting() {
    Seconds--;
    document.getElementById("NowTime").innerHTML = Sec2Time(Seconds);

    if (Seconds == 0) {
        clearInterval(IntervalID);

        IsPaused = 0;
        IsStarted = 0;
        document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-play-fill'>";

        document.getElementById("IntervalStop").disabled = true;

        if (IntervalMode == 0) {
            audioTimer.play();

            if (IsResponderFlowRunning == 1) {
                document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
                document.getElementById("ResponderSwitcher").className = "btn btn-success btn-sm";
                document.getElementById("ResponderSettings").disabled = false;

                document.getElementById("IntervalStart").disabled = false;
                document.getElementById("IntervalSet").disabled = false;
                IsResponderFlowRunning = 0;
            } else if (IsRoundFlowRunning == 1) {
                document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
                document.getElementById("RoundSwitcher").className = "btn btn-success btn-sm";
                document.getElementById("RoundSettings").disabled = false;
                document.getElementById("problemsettings").disabled = false;
                document.getElementById('nextproblem').disabled = false;

                document.getElementById("IntervalStart").disabled = false;
                document.getElementById("IntervalSet").disabled = false;
                IsRoundFlowRunning = 0;
            } else {
                setTimeout("Interval_Finish();", 3000);
            }
            syncSession({ timer_action: 'stop' });
        } else if (IntervalMode == 1) { // 抢答模式自动化
            if ((CurrentProblemType >= 0 ? CurrentProblemType : DefaultType) == 2) {
                if (InfoID == 0) {
                    SwitchResponder();
                }

                ShowNextInfo();

                if (InfoID <= 3) {
                    IntervalMode = 1;
                    Interval_setTime(10);
                    Interval_Start();
                } else {
                    syncSession({ timer_action: 'stop' });
                }
                return;
            }

            document.getElementById("IntervalStart").disabled = false;
            document.getElementById("IntervalSet").disabled = false;
            IntervalMode = 0;

            SwitchResponder();

        } else if (IntervalMode == 2) {
            document.getElementById("IntervalStart").disabled = false;
            document.getElementById("IntervalSet").disabled = false;
            IntervalMode = 0;

            Problem(DefaultType); // TODO: 抽取相同难度题目
        }
    }
}

// 轮答
var RoundAnswering = 0; // 题目是否被展示
var Round_Count = 0, Round_Team = Teams.length, Round_Team_Cnt = 0;
var Team_Problems_Count = 0;
function IsRoundFinished() {
    if (Round_Team == Teams.length) {
        return true;
    }

    for (var i = Round_Team + 1; i <= Teams.length; i++) {
        if (scores[i - 1] != -1) {
            return false;
        }
    }
    return true;
}

var IsRoundDelayed = 0;
function RoundDelay() {
    if (IsRoundDelayed == 1) {
        return;
    }

    if (Round_Team > 1) {
        Round_Team--;
        Round_Team_Cnt--;
    } else {
        Round_Team = Teams.length;
        Round_Count--;
    }
    IsRoundDelayed = 1;
}

// 连续实现倒计时-切换题目-倒计时
function RoundFlow() {
    if (IsRoundFlowRunning == 1) { // 正在进行轮答流程，停止并重置
        Interval_Stop();
        Interval_setTime(timeBefore);
        if (RoundAnswering == 0) { // 题目没有展示
            // 视为本组还未作答
            RoundDelay();
        }

        document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
        document.getElementById("RoundSwitcher").className = "btn btn-success btn-sm";
        document.getElementById("RoundSettings").disabled = false;
        document.getElementById("problemsettings").disabled = false;
        document.getElementById('nextproblem').disabled = false;

        IsRoundFlowRunning = 0;
        syncSession({ timer_action: 'stop' });
        return;
    }

    if (IsRoundDelayed == 0) {
        Team_Problems_Count = 0;
    }

    IsRoundFlowRunning = 1;
    SelectedTeam = 0;
    RoundAnswering = 0;
    IsRoundDelayed = 0;

    // 切换到下一组答题
    if (IsRoundFinished()) {
        Round_Count++;
        Round_Team = 1;
        Round_Team_Cnt = 1;
        if (IsRoundFinished()) {
            Round_Count--;
            notyf.error("所有队伍都被淘汰，无法开始轮答。");

            IsRoundFlowRunning = 0;
            return;
        }
    } else {
        Round_Team++;
        Round_Team_Cnt++;
    }
    while (scores[Round_Team - 1] == -1) {
        Round_Team++;
    }

    SelectedTeam = Round_Team;
    document.getElementById('NowInfo').innerHTML = Teams[SelectedTeam - 1];
    if (IsWinnerTakesAll == 0) {
        document.getElementById('RoundInfo').innerHTML = "第" + Round_Count + "轮，第" + Round_Team_Cnt + "位次";
    } else {
        document.getElementById('RoundInfo').innerHTML = "第" + Round_Team_Cnt + "位次，第" + (Team_Problems_Count + 1) + "题";
    }

    confirmAnsToast.hide();
    correctToast.hide();
    incorrectToast.hide();
    hintToast.hide();
    manualhintToast.hide();

    if (timeBefore != 0) {
        IntervalMode = 2;
        Interval_setTime(timeBefore);
        Interval_Start();
        document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
        document.getElementById("RoundSwitcher").className = "btn btn-danger btn-sm";
        document.getElementById("RoundSettings").disabled = true;
        document.getElementById("problemsettings").disabled = true;
        document.getElementById('nextproblem').disabled = true;
    } else {
        document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
        document.getElementById("RoundSwitcher").className = "btn btn-danger btn-sm";
        document.getElementById("RoundSettings").disabled = true;
        document.getElementById("problemsettings").disabled = true;
        document.getElementById('nextproblem').disabled = true;
        Problem(DefaultType); // TODO: 抽取相同难度题目
    }
}
function buttonSwitchRoundFlow() {
    if (IsRoundFlowRunning == 1) {
        RoundFlow();
    } else {
        buttonNextProblem();
    }
}

// 抢答
var Responding = 0;
var EarlyResponding = 0; // 是否处于提早抢答状态
var EarlyWarningCount = new Array(Teams.length).fill(0); // 每队提早抢答次数
// 连续实现倒计时-抢答-倒计时
function ResponderFlow() {
    EarlyResponding = 0;

    if (IsResponderFlowRunning == 1) { // 正在进行抢答流程，停止并重置
        if (Responding == 1) { // 处于抢答阶段
            SwitchResponder();
            Interval_setTime(timeBefore);
            SelectedTeam = 0;
        }
        else { // 处于倒计时阶段
            Interval_Stop();
            document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
            document.getElementById("ResponderSwitcher").className = "btn btn-success btn-sm";
            document.getElementById("ResponderSettings").disabled = false;
        }

        IsResponderFlowRunning = 0;
        syncSession({ timer_action: 'stop' });
        return;
    }

    SelectedTeam = 0;
    IsResponderFlowRunning = 1;
    if (timeBefore != 0) {
        document.getElementById('NowInfo').innerHTML = "等待开始";
        IntervalMode = 1;
        Interval_setTime(timeBefore);
        EarlyResponding = 1;
        Interval_Start();
        document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
        document.getElementById("ResponderSwitcher").className = "btn btn-danger btn-sm";
        document.getElementById("ResponderSettings").disabled = true;
    } else {
        SwitchResponder();
    }
}
// 切换抢答状态
function SwitchResponder() {
    SelectedTeam = 0;
    EarlyResponding = 0;

    if (Responding == 1) {
        document.getElementById('NowInfo').innerHTML = "等待开始";
        document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
        document.getElementById("ResponderSwitcher").className = "btn btn-success btn-sm";
        document.getElementById("ResponderSettings").disabled = false;
    }
    else {
        document.getElementById('NowInfo').innerHTML = "抢答中...";
        document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
        document.getElementById("ResponderSwitcher").className = "btn btn-danger btn-sm";
        document.getElementById("ResponderSettings").disabled = true;
    }

    Responding = !Responding;
    syncSession({ timer_action: 'stop' });
}
// 键盘事件监听抢答
document.onkeydown = async function () {

    if (IsEarlyResponderWarning == 1 && IsAutoScoring == 1
        && IsResponderFlowRunning == 1 && EarlyResponding == 1) {

        var EarlyTeam = 0;

        var keynum, keychar;
        keynum = window.event ? event.keyCode : event.which;

        if (65 <= keynum && keynum <= 64 + Teams.length) {
            // SelectedTeam = keynum - 64;
            for (var i = 0; i < Teams.length; i++) {
                if (keynum - 65 == team_keys[i]) {
                    EarlyTeam = i + 1;
                }
            }
        }
        if (EarlyTeam == 0) return;

        ResponderFlow();

        EarlyWarningCount[EarlyTeam - 1]++;

        var warningInfo = "队伍 " + Teams[EarlyTeam - 1] + " 提早抢答 "
            + EarlyWarningCount[EarlyTeam - 1] + 
            (EarlyResponderWarningTriggerCount == 0 || EarlyResponderWarningSubValue == 0 ? "" : 
                "/" + EarlyResponderWarningTriggerCount) + " 次。";

        if (EarlyResponderWarningTriggerCount != 0 && EarlyResponderWarningSubValue != 0) {
            if (EarlyWarningCount[EarlyTeam - 1] >= EarlyResponderWarningTriggerCount) {
                EarlyWarningCount[EarlyTeam - 1] %= EarlyResponderWarningTriggerCount;
                warningInfo += "已自动扣除 " + EarlyResponderWarningSubValue + " 分。";
                score_add_custom(EarlyTeam - 1, -EarlyResponderWarningSubValue);
            } else {
                warningInfo += "请不要提早抢答，否则将扣分处理。";
            }
        }
        syncSession();

        await showAutoCloseWarningSwal(warningInfo, "提早抢答警告");

        ResponderFlow();

        return;
    }

    if (Responding == 0) return;

    var keynum, keychar;
    keynum = window.event ? event.keyCode : event.which;

    // Default: ABCD
    if (65 <= keynum && keynum <= 64 + Teams.length) {
        // SelectedTeam = keynum - 64;
        for (var i = 0; i < Teams.length; i++) {
            if (keynum - 65 == team_keys[i]) {
                SelectedTeam = i + 1;
            }
        }
    }
    if (SelectedTeam == 0) return;

    if (scores[SelectedTeam - 1] == -1) {
        SelectedTeam = 0;
        return; // 已被淘汰队伍无法抢答
    }

    Responding = 0;
    document.getElementById('NowInfo').innerHTML = Teams[SelectedTeam - 1];

    audioRing.play();

    if (timeAfter != 0) {
        Interval_Stop();
        IntervalMode = 0;
        Interval_setTime(timeAfter);
        Interval_Start();
    } else {
        document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
        document.getElementById("ResponderSwitcher").className = "btn btn-success btn-sm";
        document.getElementById("ResponderSettings").disabled = false;
        IsResponderFlowRunning = 0;
        syncSession({ timer_action: 'stop' });
    }
}
// 切换抢答和计时模式
var ResponderTimerMode = 1;
// 0: 计时/轮答模式，1: 抢答模式
function buttonSwitchTimerMode() {
    // 弹出确认框提示切换模式会重置当前流程
    if (IsAutoScoring == 1 && ResponderTimerMode == 0
        && (IsRoundFlowRunning == 1 || IsRoundFinished() == false)) {
        Swal.fire({
            title: "切换到抢答模式",
            text: "本轮轮答还未结束，切换至抢答将强制结束本轮轮答，是否继续？",
            icon: "warning",
            showCancelButton: true,
            confirmButtonText: "确定",
            cancelButtonText: "取消",
        })
            .then((switchMode) => {
                if (switchMode.isConfirmed) {
                    if (IsRoundFlowRunning == 1) {
                        RoundFlow();
                    }
                    Round_Team = Teams.length; // 直接跳过本轮剩余队伍

                    SwitchTimerMode(!ResponderTimerMode);
                    syncSession({ timer_action: 'stop' });
                }
            });
        return;
    }

    SwitchTimerMode(!ResponderTimerMode);
    syncSession({ timer_action: 'stop' });
}
function SwitchTimerMode(ID) {
    SelectedTeam = 0;

    if (ID == 0) { // 切换到倒计时，若开启自动计分则切换到轮答模式
        if (IsResponderFlowRunning == 1) ResponderFlow();
        document.getElementById('NowInfo').innerHTML = "等待开始";

        if (IsAutoScoring == 1) {
            document.getElementById('timertitle').innerHTML = "轮答";
            document.getElementById('RoundInfo').innerHTML = "切换下一题以自动开始轮答";

            document.getElementById('RoundInfo').style.display = "block";
            document.getElementById('Responder1').style.display = "grid";
            document.getElementById('Timer1').className = "quiz-timer-main";

            document.getElementById('ResponderControls').style.display = "none";
            document.getElementById('TimerControls').style.display = "none";
            document.getElementById('RoundControls').style.display = "block";

            IsRoundDelayed = 1; // 防止弃用题目导致轮答流程出现问题

            if (IsWinnerTakesAll == 1) {
                sortTeamsByScoresDesc();
            } else {
                sortTeamsByKeyAsc();
            }
        } else {
            document.getElementById('timertitle').innerHTML = "倒计时";

            document.getElementById('RoundInfo').style.display = "none";
            document.getElementById('Responder1').style.display = "none";
            document.getElementById('Timer1').className = "quiz-timer-only-main";

            document.getElementById('ResponderControls').style.display = "none";
            document.getElementById('RoundControls').style.display = "none";
            document.getElementById('TimerControls').style.display = "block";

            Interval_setTime(defaultTime);
            sortTeamsByKeyAsc();
        }
        document.getElementById('changetimermode').innerHTML = "切换抢答";
    }
    else { // 切换到抢答模式
        Interval_Stop();
        if (IsRoundFlowRunning == 1) RoundFlow();
        document.getElementById('NowInfo').innerHTML = "等待开始";

        document.getElementById('timertitle').innerHTML = "抢答";

        if (IsAutoScoring == 1) {
            document.getElementById('changetimermode').innerHTML = "切换轮答";
        } else {
            document.getElementById('changetimermode').innerHTML = "切换计时";
        }

        document.getElementById('RoundInfo').style.display = "none";
        document.getElementById('Responder1').style.display = "grid";
        document.getElementById('Timer1').className = "quiz-timer-main";

        document.getElementById('ResponderControls').style.display = "block";
        document.getElementById('RoundControls').style.display = "none";
        document.getElementById('TimerControls').style.display = "none";

        Interval_setTime(timeBefore);
        sortTeamsByKeyAsc();
    }
    ResponderTimerMode = !ResponderTimerMode;
}
// 设置倒计时时间
function showSecondInput() {
    Swal.fire({
        title: '设置倒计时（秒）',
        input: 'number',
        inputLabel: '请输入秒数',
        inputAttributes: {
            min: 1,
            step: 1
        },
        inputValue: defaultTime,
        showCancelButton: true,
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        inputValidator: (value) => {
            if (!value || value <= 0) {
                return '请输入大于 0 的秒数';
            }
        }
    }).then((result) => {
        if (result.isConfirmed) {
            const seconds = parseInt(result.value, 10);
            defaultTime = seconds;
            Interval_setTime(seconds);
            syncSession();
        }
    });
}
// 设置抢答选项
function SetResponder() {
    Swal.fire({
        title: '抢答/轮答设置',
        html: `
<div class="container-fluid text-start">
    <div class="row g-3">
        <div class="col-12 col-md-6 border-end">
            <div class="mb-3">
                <label class="form-label fw-semibold">抢答/轮答前倒计时</label>
                <div class="input-group">
                    <input id="swal-time-before" type="number" class="form-control" min="0" step="1"
                        placeholder="填0则无该倒计时" value="${timeBefore}">
                    <span class="input-group-text">秒</span>
                </div>
            </div>

            <div class="mb-2">
                <label class="form-label fw-semibold">抢答/轮答后答题时间</label>
                <div class="input-group">
                    <input id="swal-time-after" type="number" class="form-control" min="0" step="1"
                        placeholder="填0则无该倒计时" value="${timeAfter}">
                    <span class="input-group-text">秒</span>
                </div>
            </div>

            <div class="form-check form-switch mb-3">
                <input class="form-check-input" type="checkbox" id="swal-early-responder-warning" ${IsEarlyResponderWarning == 1 ? 'checked' : ''} ${IsAutoScoring == 1 ? '' : 'disabled'}>
                <label class="form-check-label fw-semibold" for="swal-early-responder-warning">提早抢答时警告</label>
            </div>

            <div class="mb-3">
                <label class="form-label fw-semibold">触发自动扣分警告次数</label>
                <div class="input-group">
                    <input id="swal-early-responder-warning-count" type="number" class="form-control" min="0" step="1"
                        placeholder="填0则不自动扣分"  
                        value="${EarlyResponderWarningTriggerCount}" ${IsAutoScoring == 1 && IsEarlyResponderWarning == 1 ? '' : 'disabled'}>
                    <span class="input-group-text">次</span>
                </div>
            </div>

            <div class="mb-2">
                <label class="form-label fw-semibold">提早抢答扣分分值</label>
                <div class="input-group">
                    <input id="swal-early-responder-warning-sub" type="number" class="form-control" min="0" step="1"
                        placeholder="填0则不自动扣分"  
                        value="${EarlyResponderWarningSubValue}" ${IsAutoScoring == 1 && IsEarlyResponderWarning == 1 ? '' : 'disabled'}>
                    <span class="input-group-text">分</span>
                </div>
            </div>
        </div>

        <div class="col-12 col-md-6 ps-md-3">
            <div class="form-check form-switch mb-3">
                <input class="form-check-input" type="checkbox" id="swal-auto-scoring" ${IsAutoScoring == 1 ? 'checked'
                : ''}>
                <label class="form-check-label fw-semibold" for="swal-auto-scoring">开启自动计分</label>
            </div>

            <div class="form-check form-switch mb-3">
                <input class="form-check-input" type="checkbox" id="swal-auto-complexity" ${IsAutoComplexity == 1 ? 'checked'
                : ''} ${!IsRoundFinished() ? 'disabled' : ''}>
                <label class="form-check-label fw-semibold" for="swal-auto-complexity">同一轮轮答分配相同难度的题目</label>
            </div>

            <div class="form-check form-switch mb-3">
                <input class="form-check-input" type="checkbox" id="swal-winner-takes-all" ${IsWinnerTakesAll == 1 ? 'checked'
                : ''} ${!IsRoundFinished() ? 'disabled' : ''}>
                <label class="form-check-label fw-semibold" for="swal-winner-takes-all">轮答使用赢者通吃模式</label>
            </div>

            <div class="mb-3">
                <label class="form-label fw-semibold">自动加分分值</label>
                <div class="input-group">
                    <input id="swal-auto-add" type="number" class="form-control" min="0" step="1"
                        value="${autoAddValue}">
                    <span class="input-group-text">分</span>
                </div>
            </div>

            <div class="mb-2">
                <label class="form-label fw-semibold">自动扣分分值</label>
                <div class="input-group">
                    <input id="swal-auto-sub" type="number" class="form-control" min="0" step="1"
                        value="${autoSubValue}">
                    <span class="input-group-text">分</span>
                </div>
            </div>
        </div>
    </div>
</div>
    `,
        focusConfirm: false,
        showCancelButton: true,
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        didOpen: () => {
            const autoScoringToggle = document.getElementById('swal-auto-scoring');
            const earlyResponderWarningToggle = document.getElementById('swal-early-responder-warning');
            const autoComplexityToggle = document.getElementById('swal-auto-complexity');
            const autoAddInput = document.getElementById('swal-auto-add');
            const autoSubInput = document.getElementById('swal-auto-sub');
            const earlyResponderWarningCountInput = document.getElementById('swal-early-responder-warning-count');
            const earlyResponderWarningSubInput = document.getElementById('swal-early-responder-warning-sub');

            function syncEarlyResponderWarningInputs() {
                const autoScoringDisabled = !autoScoringToggle.checked;
                const earlyWarningEnabled = autoScoringToggle.checked && earlyResponderWarningToggle.checked;

                earlyResponderWarningToggle.disabled = autoScoringDisabled;
                earlyResponderWarningCountInput.disabled = !earlyWarningEnabled;
                earlyResponderWarningSubInput.disabled = !earlyWarningEnabled;
            }

            function syncAutoScoringInputs() {
                const disabled = !autoScoringToggle.checked;
                autoComplexityToggle.disabled = disabled;
                document.getElementById('swal-winner-takes-all').disabled = disabled;
                autoAddInput.disabled = disabled;
                autoSubInput.disabled = disabled;
                syncEarlyResponderWarningInputs();
            }

            autoScoringToggle.addEventListener('change', syncAutoScoringInputs);
            earlyResponderWarningToggle.addEventListener('change', syncEarlyResponderWarningInputs);
            syncAutoScoringInputs();
        },

        preConfirm: () => {
            const before = document.getElementById('swal-time-before').value;
            const after = document.getElementById('swal-time-after').value;
            const autoScoringEnabled = document.getElementById('swal-auto-scoring').checked;
            const earlyResponderWarningEnabled = document.getElementById('swal-early-responder-warning').checked;
            const autoComplexityEnabled = document.getElementById('swal-auto-complexity').checked;
            const winnerTakesAllEnabled = document.getElementById('swal-winner-takes-all').checked;
            const addValue = document.getElementById('swal-auto-add').value;
            const subValue = document.getElementById('swal-auto-sub').value;
            const earlyResponderWarningCount = document.getElementById('swal-early-responder-warning-count').value;
            const earlyResponderWarningSubValue = document.getElementById('swal-early-responder-warning-sub').value;

            if (before === '' || Number(before) < 0) {
                Swal.showValidationMessage('请输入有效的“抢答/轮答前倒计时”');
                return false;
            }
            if (after === '' || Number(after) < 0) {
                Swal.showValidationMessage('请输入有效的“抢答/轮答后答题时间”');
                return false;
            }
            if (autoScoringEnabled && (addValue === '' || Number(addValue) < 0)) {
                Swal.showValidationMessage('请输入有效的“自动加分分值”');
                return false;
            }
            if (autoScoringEnabled && (subValue === '' || Number(subValue) < 0)) {
                Swal.showValidationMessage('请输入有效的“自动扣分分值”');
                return false;
            }
            if (autoScoringEnabled && earlyResponderWarningEnabled && (earlyResponderWarningCount === '' || Number(earlyResponderWarningCount) < 0)) {
                Swal.showValidationMessage('请输入有效的“触发自动扣分抢答次数”');
                return false;
            }
            if (autoScoringEnabled && earlyResponderWarningEnabled && (earlyResponderWarningSubValue === '' || Number(earlyResponderWarningSubValue) < 0)) {
                Swal.showValidationMessage('请输入有效的“提早抢答扣分分值”');
                return false;
            }

            return {
                before: parseInt(before, 10),
                after: parseInt(after, 10),
                autoScoring: autoScoringEnabled ? 1 : 0,
                earlyResponderWarning: earlyResponderWarningEnabled && autoScoringEnabled ? 1 : 0,
                autoComplexity: autoComplexityEnabled ? 1 : 0,
                winnerTakesAll: winnerTakesAllEnabled ? 1 : 0,
                addValue: parseInt(addValue, 10),
                subValue: parseInt(subValue, 10),
                earlyResponderWarningCount: parseInt(earlyResponderWarningCount, 10),
                earlyResponderWarningSubValue: parseInt(earlyResponderWarningSubValue, 10)
            };
        }
    }).then((result) => {
        if (result.isConfirmed) {
            const { before, after, autoScoring, earlyResponderWarning, autoComplexity, winnerTakesAll, addValue, subValue, earlyResponderWarningCount, earlyResponderWarningSubValue } = result.value;

            if (IsAutoScoring != autoScoring
                && IsAutoScoring == 1 && ResponderTimerMode == 0
                && (IsRoundFlowRunning == 1 || IsRoundFinished() == false)) {
                Swal.fire({
                    title: '切换自动计分设置',
                    text: '本轮轮答还未结束，关闭自动计分将强制结束本轮轮答，是否继续？',
                    icon: 'warning',
                    showCancelButton: true,
                    confirmButtonText: '确定',
                    cancelButtonText: '取消',
                }).then((switchAutoScoring) => {
                    if (!switchAutoScoring.isConfirmed) {
                        return;
                    }

                    if (IsRoundFlowRunning == 1) {
                        RoundFlow();
                    }
                    Round_Team = Teams.length; // 直接跳过本轮剩余队伍

                    timeBefore = before;
                    timeAfter = after;
                    IsAutoScoring = autoScoring;
                    IsEarlyResponderWarning = earlyResponderWarning;
                    IsAutoComplexity = autoComplexity;
                    IsWinnerTakesAll = winnerTakesAll;

                    if (IsAutoScoring == 1) {
                        autoAddValue = addValue;
                        autoSubValue = subValue;
                        EarlyResponderWarningTriggerCount = earlyResponderWarningCount;
                        EarlyResponderWarningSubValue = earlyResponderWarningSubValue;
                    }
                    SwitchTimerMode(ResponderTimerMode); // 刷新界面
                    ResponderTimerMode = !ResponderTimerMode;
                    confirmAnsToast.hide();
                    correctToast.hide();
                    incorrectToast.hide();
                    hintToast.hide();
                    manualhintToast.show();
                    syncSession({ timer_action: 'stop' });
                });
                return;
            }

            timeBefore = before;
            timeAfter = after;
            IsEarlyResponderWarning = earlyResponderWarning;
            IsAutoComplexity = autoComplexity;
            IsWinnerTakesAll = winnerTakesAll;

            if (ResponderTimerMode == 0) {
                if (IsWinnerTakesAll == 1) {
                    sortTeamsByScoresDesc();
                } else {
                    sortTeamsByKeyAsc();
                }
            }

            if (IsAutoScoring != autoScoring) {
                IsAutoScoring = autoScoring;
                SwitchTimerMode(ResponderTimerMode); // 刷新界面
                ResponderTimerMode = !ResponderTimerMode;
                confirmAnsToast.hide();
                correctToast.hide();
                incorrectToast.hide();
                if (IsAutoScoring == 0) {
                    hintToast.hide();
                    manualhintToast.show();
                } else {
                    manualhintToast.hide();
                    hintToast.show();
                }
            }
            if (IsAutoScoring == 1) {
                autoAddValue = addValue;
                autoSubValue = subValue;
                EarlyResponderWarningTriggerCount = earlyResponderWarningCount;
                EarlyResponderWarningSubValue = earlyResponderWarningSubValue;
            }
            Interval_setTime(timeBefore);
            syncSession();
        }
    });
}
// 重置所有控件
function resetResponderAndTimer() {
    SelectedTeam = 0;
    Responding = 0;
    IsResponderFlowRunning = 0;

    document.getElementById('NowInfo').innerHTML = "等待开始";
    document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
    document.getElementById("ResponderSwitcher").className = "btn btn-success btn-sm";
    document.getElementById("ResponderSettings").disabled = false;

    Interval_Stop();
    if (ResponderTimerMode == 1) {
        Interval_setTime(timeBefore);
    }
}

/* -- 题目显示模块 -- */
var ProblemID = 0, InfoID = 0, DefaultType = 0;
var CurrentProblemType = -1;
var LastProblemType = (typeof last_problem_type !== 'undefined') ? last_problem_type : -1;
var LastProblemID = (typeof last_problem_id !== 'undefined') ? last_problem_id : 0;
var CurrentProblemID = 0;
var pendingUnsavedDraw = false;
var IsDiscarded = 0;
var IsShowAnswer = 1;

function updateReshowButton() {
    var btn = document.getElementById('reshowproblem');
    if (btn) {
        btn.disabled = !(LastProblemID > 0 && LastProblemType >= 0 && LastProblemType <= 2);
    }
}

function parseSessionSaveFailure(res) {
    var prefix = "fail : session_save_failed|";
    if (typeof res !== 'string' || res.indexOf(prefix) !== 0) {
        return false;
    }
    var parts = res.substring(prefix.length).split("|");
    var failedType = parseInt(parts[0], 10);
    var failedId = parseInt(parts[1], 10);
    if (!isNaN(failedType) && failedType >= 0 && failedType <= 2 && !isNaN(failedId) && failedId > 0) {
        LastProblemType = failedType;
        LastProblemID = failedId;
        updateReshowButton();
    }
    return true;
}

function applyRenderedProblem(html, type, opts) {
    opts = opts || {};
    var area = document.getElementById('dymproblemarea');
    area.style.display = "none";
    area.innerHTML = html;

    var meta = document.getElementById('problemMeta');
    if (meta) {
        var metaType = parseInt(meta.getAttribute('data-type'), 10);
        var metaId = parseInt(meta.getAttribute('data-id'), 10);
        if ((type === undefined || type === null || type < 0) && !isNaN(metaType)) {
            type = metaType;
        }
        if (!isNaN(metaType) && metaType >= 0 && metaType <= 2) {
            LastProblemType = metaType;
        }
        if (!isNaN(metaId) && metaId > 0) {
            LastProblemID = metaId;
            CurrentProblemID = metaId;
        }
    }
    if (type !== undefined && type !== null && type >= 0 && type <= 2) {
        CurrentProblemType = type;
        LastProblemType = type;
    }
    updateReshowButton();

    var showAnswer = !!opts.showAnswer;
    IsShowAnswer = showAnswer ? 1 : 0;
    IsDiscarded = opts.discarded ? 1 : 0;
    document.getElementById('ansarea').style.display = showAnswer ? "block" : "none";
    document.getElementById('showanswer').disabled = showAnswer;

    if (typeof opts.problemNumber === 'number') {
        ProblemID = opts.problemNumber;
    }
    document.getElementById('problemtitle').innerHTML = "第" + ProblemID + "题";

    if (CurrentProblemType != 0) {
        document.getElementById('d1area').style.display = "none";
        document.getElementById('d2area').style.display = "none";
        document.getElementById('d3area').style.display = "none";
        document.getElementById('d4area').style.display = "none";
    } else {
        document.getElementById('d1area').style.display = "block";
        document.getElementById('d2area').style.display = "block";
        document.getElementById('d3area').style.display = "block";
        document.getElementById('d4area').style.display = "block";
    }

    if (CurrentProblemType != 2) {
        document.getElementById('ta').innerHTML = "A.&nbsp";
        document.getElementById('tb').innerHTML = "B.&nbsp";
        document.getElementById('tc').innerHTML = "C.&nbsp";
        document.getElementById('td').innerHTML = "D.&nbsp";
        document.getElementById('nextinfo').disabled = true;
    } else {
        document.getElementById('ta').innerHTML = "信息1&nbsp";
        document.getElementById('tb').innerHTML = "信息2&nbsp";
        document.getElementById('tc').innerHTML = "信息3&nbsp";
        document.getElementById('td').innerHTML = "信息4&nbsp";

        var restoredInfoId = (typeof opts.infoId === 'number') ? Math.max(0, Math.min(4, opts.infoId)) : 0;
        if (showAnswer) {
            restoredInfoId = 4;
        }
        InfoID = restoredInfoId;
        for (var k = 1; k <= restoredInfoId; k++) {
            document.getElementById('d' + k + 'area').style.display = "block";
        }
        document.getElementById('nextinfo').disabled = (InfoID >= 4 || showAnswer);
    }

    area.style.display = "block";
    document.getElementById('waitingarea').style.display = "none";
    document.getElementById('problemarea').style.display = "block";
}

function reshowLastProblem() {
    var query = "/main/competition/display/problem?guid=" + encodeURIComponent(competition_guid) + "&reshow=1";
    if (LastProblemType >= 0 && LastProblemID > 0) {
        query += "&type=" + LastProblemType + "&id=" + LastProblemID;
    }
    $.ajax({
        type: 'post',
        url: query,
        async: true,
        dataType: 'text',
        data: '',
        success: function (res) {
            if (res.substring(0, 4) == "fail") {
                if (parseSessionSaveFailure(res)) {
                    notyf.error("会话快照保存失败，请检查磁盘或权限后再次点击「重新显示上一题」重试（不会消耗新题）。");
                } else {
                    notyf.error("没有可重新显示的上一题。");
                }
                return;
            }
            var wasWaiting = (document.getElementById('problemarea').style.display === "none");
            var isSwitchingToUnsaved = pendingUnsavedDraw || (LastProblemID > 0 && (LastProblemID !== CurrentProblemID || LastProblemType !== CurrentProblemType));
            if (wasWaiting || isSwitchingToUnsaved) {
                var nextNum = pendingUnsavedDraw ? (ProblemID + 1) : Math.max(1, ProblemID);
                pendingUnsavedDraw = false;
                applyRenderedProblem(res, null, {
                    problemNumber: nextNum,
                    infoId: 0,
                    showAnswer: false,
                    discarded: false
                });
                isConfirmPending = false;
                syncSession();
            } else {
                applyRenderedProblem(res, null, {
                    problemNumber: Math.max(1, ProblemID),
                    infoId: InfoID,
                    showAnswer: IsShowAnswer == 1,
                    discarded: IsDiscarded == 1
                });
            }
            notyf.success("已重新显示上一题（未消耗新题）");
        },
        error: function () {
            notyf.error("尝试重新显示上一题时出错。请检查网络连接并重试。");
        }
    });
}

function RequestTargetComplexity(Type) {
    var ans;
    $.ajax({
        type: 'post',
        url: "/main/competition/display/problem?requestcomplexity=1&guid=" + competition_guid + "&type=" + Type,
        async: false,
        dataType: 'text',
        data: '',
        success: function (res) {
            ans = res;
        },
        error: function (xhr, status, error) {
            ans = "0";
        }
    })
    return ans;
}
function Problem(Type) { // Type: 0-选择题，1-填空题，2-信息题
    if (IsAutoScoring == 1 && IsAutoComplexity == 1
        && IsRoundFlowRunning == 1
        && Round_Team_Cnt == 1
        && (IsWinnerTakesAll == 1 ? Team_Problems_Count == 0 : true)) {
        TargetComplexity = RequestTargetComplexity(Type);
    }

    var prospectiveState = collectSessionState();
    prospectiveState.problem_number = ProblemID + 1;
    if (IsAutoScoring == 1 && IsRoundFlowRunning == 1 && IsWinnerTakesAll == 1) {
        prospectiveState.team_problems_count = Team_Problems_Count + 1;
    }
    if (IsAutoScoring == 1 && ResponderTimerMode == 0 && IsRoundFlowRunning == 1) {
        prospectiveState.round_answering = true;
    }
    prospectiveState.info_id = 0;
    prospectiveState.show_answer = false;
    prospectiveState.discarded = false;
    prospectiveState.confirm_pending = false;

    $.ajax({
        type: 'post',
        url: "/main/competition/display/problem?guid=" + competition_guid + "&type=" + Type
            + ((IsAutoScoring == 1 && IsAutoComplexity == 1 && IsRoundFlowRunning == 1
                && (IsWinnerTakesAll == 1 ? Team_Problems_Count == 0 : true))
                ? "&complexity=" + TargetComplexity : ""),
        async: true,
        dataType: 'text',
        data: { state: JSON.stringify(prospectiveState) },
        success: function (res) {
            if (res.substring(0, 4) == "fail") {
                if (parseSessionSaveFailure(res)) {
                    pendingUnsavedDraw = true;
                    notyf.error("题目已抽取但会话快照保存失败，请勿重复抽题；请排查后点击「重新显示上一题」重试。");
                } else {
                    notyf.error("尝试拉取题目时出错。请检查是否剩余足够的题目。");
                }
                if (IsAutoScoring == 1 && IsRoundFlowRunning == 1) {
                    RoundFlow();
                }
            } else {
                pendingUnsavedDraw = false;
                if (IsAutoScoring == 1 && IsRoundFlowRunning == 0) {
                    resetResponderAndTimer();
                }
                confirmAnsToast.hide();
                correctToast.hide();
                incorrectToast.hide();
                hintToast.hide();
                manualhintToast.hide();

                if (IsAutoScoring == 1 && IsRoundFlowRunning == 1 && IsWinnerTakesAll == 1) {
                    Team_Problems_Count++;
                }

                isConfirmPending = false;
                applyRenderedProblem(res, Type, {
                    problemNumber: ProblemID + 1,
                    infoId: 0,
                    showAnswer: false,
                    discarded: false
                });

                if (IsAutoScoring == 1) {
                    if (ResponderTimerMode == 1) { // 抢答
                        ResponderFlow();
                    } else {                       // 轮答
                        if (IsRoundFlowRunning == 0)
                            return;
                        RoundAnswering = 1;
                        if (timeAfter != 0) {
                            IntervalMode = 0;
                            Interval_setTime(timeAfter);
                            Interval_Start();
                        } else {
                            document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-play-fill'></i>";
                            document.getElementById("RoundSwitcher").className = "btn btn-success btn-sm";
                            document.getElementById("RoundSettings").disabled = false;
                            document.getElementById("problemsettings").disabled = false;
                            document.getElementById('nextproblem').disabled = false;
                            IsRoundFlowRunning = 0;
                            syncSession({ timer_action: 'stop' });
                        }
                    }
                } else {
                    syncSession();
                }
            }

            return;
        },
        error: function (xhr, status, error) {
            notyf.error("尝试拉取题目时出错。请检查网络连接并重试。");
            if (IsAutoScoring == 1 && IsRoundFlowRunning == 1) {
                RoundFlow();
            }

        }
    });
}

function buttonNextProblem() {
    if (IsShowAnswer == 0) {
        Swal.fire({
            title: "切换下一题",
            text: "当前题目还未展示答案，是否继续？",
            icon: "warning",
            showCancelButton: true,
            confirmButtonText: "确定",
            cancelButtonText: "取消",
        })
            .then((nextProblem) => {
                if (nextProblem.isConfirmed) {
                    if (ResponderTimerMode == 0 && IsAutoScoring == 1) { // 计时模式下如果开启自动记分则切换到轮答模式
                        RoundFlow();
                    } else {
                        Problem(DefaultType);
                    }
                }
            });
    } else {
        if (ResponderTimerMode == 0 && IsAutoScoring == 1) { // 计时模式下如果开启自动记分则切换到轮答模式
            RoundFlow();
        } else {
            Problem(DefaultType);
        }
    }
}

function ShowAnswer() {
    // TODO: 如果还没被抢答/还未展示任何信息，显示确认框

    if (IsResponderFlowRunning == 1 && IsAutoScoring == 1) {
        ResponderFlow();
    }
    if (IsRoundFlowRunning == 1 && IsAutoScoring == 1) {
        RoundFlow();
    }

    if ((CurrentProblemType >= 0 ? CurrentProblemType : DefaultType) == 2) {
        while (InfoID < 4) {
            ShowNextInfo(); // 展示所有剩余信息
        }
    }

    IsShowAnswer = 1;
    document.getElementById('ansarea').style.display = "block";
    document.getElementById('showanswer').disabled = true;

    isConfirmPending = false;
    if (IsAutoScoring == 1) {
        if (SelectedTeam != 0) {
            if (IsDiscarded == 1) {
                syncSession();
                return;
            }

            // TODO：判断题目类型
            document.getElementById('teamName_t1').innerHTML
                = document.getElementById('teamName_t2').innerHTML
                = document.getElementById('teamName_t3').innerHTML
                = Teams[SelectedTeam - 1];

            if (autoAddValue > 0) {
                document.getElementById('correctScoreChange').style.display = "inline-block";
            } else {
                document.getElementById('correctScoreChange').style.display = "none";
            }
            if (autoSubValue > 0) {
                document.getElementById('incorrectScoreChange').style.display = "inline-block";
            } else {
                document.getElementById('incorrectScoreChange').style.display = "none";
            }

            isConfirmPending = true;
            confirmAnsToast.show();
        }
    }
    syncSession();
}
function ShowNextInfo() {
    InfoID++;
    document.getElementById('d' + InfoID + 'area').style.display = "block";
    if (InfoID == 4)
        document.getElementById('nextinfo').disabled = true;
    syncSession();
}

document.getElementById('resetCurrentQuestion').addEventListener('click', function () {
    Swal.fire({
        title: "重置题号",
        text: "确实要重置当前的题号从第1题开始重新计算吗？此操作不可逆。",
        icon: "warning",
        showCancelButton: true,
        confirmButtonText: "确定",
        cancelButtonText: "取消",
    })
        .then((reset) => {
            if (reset.isConfirmed) {
                ProblemID = 0;
                syncSession();
                notyf.success("成功重置了题号，接下来的题目将重新从第1题开始编号。");
            }
        });
});
document.getElementById('discardCurrentQuestion').addEventListener('click', function () {
    if (IsDiscarded == 1) {
        notyf.error("当前题目已经被弃用。");
        return;
    }

    if (IsAutoScoring == 1 && ResponderTimerMode == 0 && IsRoundDelayed == 1) {
        notyf.error("已经进入了本轮下一位次答题，上一位次的题目无法再弃用。");
        return;
    }

    Swal.fire({
        title: "弃用当前题目",
        text: `确实要弃用当前显示的题目吗？这将使本题不计入当前题号${IsAutoScoring == 1 && ResponderTimerMode == 0 ? '，且下一题将仍由当前队伍作答' : ''}。`,
        icon: "warning",
        showCancelButton: true,
        confirmButtonText: "确定",
        cancelButtonText: "取消",
    })
        .then((discard) => {
            if (discard.isConfirmed) {
                if (ProblemID > 0)
                    ProblemID--;
                if (IsAutoScoring == 1 && ResponderTimerMode == 0 && IsWinnerTakesAll == 0) {
                    if (Team_Problems_Count > 0) {
                        Team_Problems_Count--;
                    }
                }
                IsDiscarded = 1;
                IsShowAnswer = 1; // 标记为可以直接切换下一题
                isConfirmPending = false;
                confirmAnsToast.hide();

                if (IsAutoScoring == 1 && ResponderTimerMode == 0) {
                    RoundDelay();
                    syncSession();
                    notyf.success("成功弃用了当前题目，且下一题将仍由当前队伍作答。");
                } else {
                    syncSession();
                    notyf.success("成功弃用了当前题目。");
                }
            }
        });
});
document.querySelectorAll('input[name="questionType"]').forEach((radio) => {
    radio.addEventListener('change', function () {
        if (IsAutoScoring == 1 && ResponderTimerMode == 0
            && IsRoundFinished() == false) {
            notyf.error("本轮轮答还未结束，无法切换题目类型。");
            return;
        }

        if (document.getElementById('radioChoice').checked) {
            DefaultType = 0;
            document.getElementById('nextproblem').innerHTML = "下一选择题";
        } else if (document.getElementById('radioFillIn').checked) {
            DefaultType = 1;
            document.getElementById('nextproblem').innerHTML = "下一填空题";
        } else if (document.getElementById('radioInfo').checked) {
            DefaultType = 2;
            document.getElementById('nextproblem').innerHTML = "下一信息题";
        }
        syncSession();
    });
});
$(document).ready(function () {
    // 监听 modal 弹出事件
    $('#modalQuestionSettings').on('show.bs.modal', function () {
        if (IsAutoScoring == 1 && ResponderTimerMode == 0
            && IsRoundFinished() == false) {
            document.getElementById('radioChoice').disabled = true;
            document.getElementById('radioFillIn').disabled = true;
            document.getElementById('radioInfo').disabled = true;
            document.getElementById('questionTypeError').style.display = "block";
        } else {
            document.getElementById('radioChoice').disabled = false;
            document.getElementById('radioFillIn').disabled = false;
            document.getElementById('radioInfo').disabled = false;
            document.getElementById('questionTypeError').style.display = "none";
        }
    });
});


/* -- 队伍记分模块 -- */
function postScoreUpdate(Team, Score) {
    $.ajax({
        type: 'post',
        url: "/main/competition/display/scores?guid=" + competition_guid + "&team=" + team_keys[Team] + "&score=" + Score,
        async: true,
        dataType: 'text',
        data: '',
        success: function (res) {
            if (res != "success") {
                notyf.error("尝试更新分数时出错，服务器未能保存最新的分数。");
            }
            return;
        },
        error: function (xhr, status, error) {
            notyf.error("尝试更新分数时出错，服务器未能保存最新的分数。请检查网络连接并重试。");
        }
    });
}
function score_add_custom(Team, Value) {
    if (scores[Team] == -1) return; // 已淘汰队伍无法加分

    if (scores[Team] + Value < 0) {
        Value = -scores[Team]; // 防止分数变为负数
    }

    scores[Team] += Value;
    document.getElementById("teamscore" + Team).innerHTML = scores[Team];
    if (scores[Team] != 0) {
        document.getElementById('btn2' + Team).disabled = false;
    } else if (scores[Team] == 0) {
        document.getElementById('btn2' + Team).disabled = true;
    }
    // post message to server to update score
    postScoreUpdate(Team, scores[Team]);
}
function score_add(Team) {
    score_add_custom(Team, 1);
}
function score_sub(Team) {
    if (scores[Team] > 0) {
        score_add_custom(Team, -1);
    }
}
function setteambuttons(Team) {
    if (scores[Team] == 0) {
        document.getElementById('btn1' + Team).disabled = false;
        document.getElementById('btn2' + Team).disabled = true;
    } else if (scores[Team] == -1) {
        document.getElementById('btn1' + Team).disabled = true;
        document.getElementById('btn2' + Team).disabled = true;
        document.getElementById("teamscore" + Team).innerHTML = '-';
    } else {
        document.getElementById('btn1' + Team).disabled = false;
        document.getElementById('btn2' + Team).disabled = false;
    }
}
for (let i = 0; i < Teams.length; i++) {
    setteambuttons(i);
}
function score_edit(Team) {
    Swal.fire({
        title: `${Teams[Team]} 分数设置`,
        html: `
    <div class="form-floating">
        <input id="team-score" type="number" class="form-control" value="${scores[Team] == -1 ? 0 : scores[Team]}" min="0" ${scores[Team] == -1 ? 'disabled' : ''}>
        <label for="team-score">队伍分数</label>
    </div>

    <div class="form-check" style="text-align:left; margin-top: 10px">
        <input type="checkbox" class="form-check-input" id="eliminated-toggle" ${scores[Team] == -1 ? 'checked' : ''}>
        <label class="form-check-label" for="eliminated-toggle">队伍已淘汰</label>
    </div>
    `,
        showCancelButton: true,
        confirmButtonText: '确定',
        cancelButtonText: '取消',

        preConfirm: () => {
            const scoreInput = document.getElementById('team-score');
            const isEliminated = document.getElementById('eliminated-toggle').checked;

            let score = parseInt(scoreInput.value, 10);

            // 如果队伍已淘汰，设置分数为 -1，并禁用输入框
            if (isEliminated) {
                score = -1;
                scoreInput.value = 0;
                scoreInput.disabled = true;
            } else {
                scoreInput.disabled = false;
            }

            return {
                score: score,
                eliminated: isEliminated
            };
        },

        willOpen: () => {
            // 监听开关切换，控制输入框禁用/启用
            const scoreInput = document.getElementById('team-score');
            const toggle = document.getElementById('eliminated-toggle');

            toggle.addEventListener('change', () => {
                if (toggle.checked) {
                    scoreInput.value = 0;
                    scoreInput.disabled = true;
                } else {
                    scoreInput.disabled = false;
                }
            });
        }
    }).then((result) => {
        if (result.isConfirmed) {
            const { score, eliminated } = result.value;

            scores[Team] = score;
            setteambuttons(Team);
            document.getElementById("teamscore" + Team).innerHTML = eliminated ? '-' : score;
            // post message to server to update score
            postScoreUpdate(Team, scores[Team]);
        }
    });
}

function autoScoring_Correct() {
    confirmAnsToast.hide();
    isConfirmPending = false;

    if (IsAutoScoring == 0) return;
    if (SelectedTeam == 0) return;

    score_add_custom(SelectedTeam - 1, autoAddValue);

    if (ResponderTimerMode == 0 && IsWinnerTakesAll == 1) {
        // TODO: 后续题目结束难度控制
        RoundDelay();
    }

    syncSession();
    correctToast.show();
}
function autoScoring_Incorrect() {
    confirmAnsToast.hide();
    isConfirmPending = false;

    if (IsAutoScoring == 0) return;
    if (SelectedTeam == 0) return;

    score_add_custom(SelectedTeam - 1, -autoSubValue);
    syncSession();
    incorrectToast.show();
}

/* -- 队伍排序模块 -- */
function refreshTeamTableDisplay() {
    const teamNameCells = document.querySelectorAll('.quiz-team-name');
    const teamScoreCells = document.querySelectorAll('.quiz-team-score');

    for (let i = 0; i < Teams.length; i++) {
        if (teamNameCells[i]) {
            teamNameCells[i].innerHTML = Teams[i];
        }
        if (teamScoreCells[i]) {
            teamScoreCells[i].innerHTML = scores[i] == -1 ? '-' : scores[i];
        }
        setteambuttons(i);
    }
}
/**
 * 根据scores从大到小排序，当scores相同时按team_keys从小到大排序
 * 直接修改全局变量 Teams、scores、team_keys、EarlyWarningCount
 */
function sortTeamsByScoresDesc() {
    // 创建包含索引的对象数组
    let indices = [];
    for (let i = 0; i < scores.length; i++) {
        indices.push({
            index: i,
            score: scores[i],
            key: team_keys[i],
            earlyWarningCount: EarlyWarningCount[i]
        });
    }

    // 排序：首先按分数从大到小，其次按team_keys从小到大
    indices.sort((a, b) => {
        if (b.score !== a.score) {
            return b.score - a.score;  // 分数从大到小
        }
        return a.key - b.key;  // team_keys从小到大
    });

    // 根据排序结果更新全局数组
    let newTeams = [];
    let newScores = [];
    let newTeamKeys = [];
    let newEarlyWarningCount = [];

    for (let obj of indices) {
        newTeams.push(Teams[obj.index]);
        newScores.push(scores[obj.index]);
        newTeamKeys.push(team_keys[obj.index]);
        newEarlyWarningCount.push(EarlyWarningCount[obj.index]);
    }

    // 更新全局数组
    Teams.length = 0;
    Teams.push(...newTeams);
    scores.length = 0;
    scores.push(...newScores);
    team_keys.length = 0;
    team_keys.push(...newTeamKeys);
    EarlyWarningCount.length = 0;
    EarlyWarningCount.push(...newEarlyWarningCount);

    refreshTeamTableDisplay();
}

/* -- 队伍按 team_keys 升序排序模块 -- */
/**
 * 根据 team_keys 从小到大排序，当 team_keys 相同时按 scores 从大到小排序
 * 直接修改全局变量 Teams、scores、team_keys、EarlyWarningCount
 */
function sortTeamsByKeyAsc() {
    // 创建包含索引的对象数组
    let indices = [];
    for (let i = 0; i < team_keys.length; i++) {
        indices.push({
            index: i,
            key: team_keys[i],
            score: scores[i],
            earlyWarningCount: EarlyWarningCount[i]
        });
    }

    // 排序：首先按team_keys从小到大，其次按scores从大到小
    indices.sort((a, b) => {
        if (a.key !== b.key) {
            return a.key - b.key; // team_keys从小到大
        }
        return b.score - a.score; // scores从大到小
    });

    // 根据排序结果更新全局数组
    let newTeams = [];
    let newScores = [];
    let newTeamKeys = [];
    let newEarlyWarningCount = [];

    for (let obj of indices) {
        newTeams.push(Teams[obj.index]);
        newScores.push(scores[obj.index]);
        newTeamKeys.push(team_keys[obj.index]);
        newEarlyWarningCount.push(EarlyWarningCount[obj.index]);
    }

    // 更新全局数组
    Teams.length = 0;
    Teams.push(...newTeams);
    scores.length = 0;
    scores.push(...newScores);
    team_keys.length = 0;
    team_keys.push(...newTeamKeys);
    EarlyWarningCount.length = 0;
    EarlyWarningCount.push(...newEarlyWarningCount);

    refreshTeamTableDisplay();
}

/* -- 会话快照持久化与恢复模块 -- */
function getCurrentModeString() {
    if (ResponderTimerMode == 1) {
        return "responder";
    }
    return IsAutoScoring == 1 ? "round" : "timer";
}

function collectSessionState() {
    return {
        round_team: Round_Team,
        round_team_cnt: Round_Team_Cnt,
        round_count: Round_Count,
        team_problems_count: Team_Problems_Count,
        round_answering: RoundAnswering == 1,
        round_delayed: IsRoundDelayed == 1,
        round_flow_running: IsRoundFlowRunning == 1,
        responder_flow_running: IsResponderFlowRunning == 1,
        responding: Responding == 1,
        early_responding: EarlyResponding == 1,
        selected_team: SelectedTeam,
        mode: getCurrentModeString(),
        auto_scoring: IsAutoScoring == 1,
        auto_complexity: IsAutoComplexity == 1,
        winner_takes_all: IsWinnerTakesAll == 1,
        early_responder_warning: IsEarlyResponderWarning == 1,
        early_warning_trigger_count: EarlyResponderWarningTriggerCount,
        early_warning_sub_value: EarlyResponderWarningSubValue,
        early_warning_counts: EarlyWarningCount.slice(),
        auto_add_value: autoAddValue,
        auto_sub_value: autoSubValue,
        time_before: timeBefore,
        time_after: timeAfter,
        default_time: defaultTime,
        target_complexity: Number(TargetComplexity) || 0,
        problem_number: ProblemID,
        info_id: InfoID,
        show_answer: IsShowAnswer == 1,
        discarded: IsDiscarded == 1,
        confirm_pending: isConfirmPending,
        default_type: DefaultType,
        team_order: team_keys.slice()
    };
}

function syncSession(extraParams) {
    if (isRestoringSession || isEndingCompetition) {
        return;
    }
    var payload = {
        guid: competition_guid,
        state: JSON.stringify(collectSessionState())
    };
    if (extraParams) {
        Object.assign(payload, extraParams);
    }
    sessionSyncChain = sessionSyncChain.then(function () {
        return $.ajax({
            type: 'post',
            url: "/main/competition/display/session",
            data: payload,
            dataType: 'text'
        }).catch(function () {
            // 忽略单次同步失败，避免阻塞后续状态同步
        });
    });
}

function applyTeamOrder(order) {
    if (!Array.isArray(order) || order.length !== team_keys.length) {
        return;
    }
    var keyToIndex = {};
    for (var i = 0; i < team_keys.length; i++) {
        keyToIndex[team_keys[i]] = i;
    }
    var newTeams = [];
    var newScores = [];
    var newTeamKeys = [];
    var newEarly = [];
    for (var j = 0; j < order.length; j++) {
        var idx = keyToIndex[order[j]];
        if (idx === undefined) {
            return;
        }
        newTeams.push(Teams[idx]);
        newScores.push(scores[idx]);
        newTeamKeys.push(team_keys[idx]);
        newEarly.push(EarlyWarningCount[idx]);
    }
    Teams.length = 0;
    Teams.push(...newTeams);
    scores.length = 0;
    scores.push(...newScores);
    team_keys.length = 0;
    team_keys.push(...newTeamKeys);
    EarlyWarningCount.length = 0;
    EarlyWarningCount.push(...newEarly);
    refreshTeamTableDisplay();
}

function restoreSessionOnLoad() {
    updateReshowButton();
    if (typeof has_initial_session === 'undefined' || !has_initial_session || typeof initial_session !== 'object' || initial_session === null) {
        return;
    }

    var s = initial_session;
    isRestoringSession = true;
    try {
        timeBefore = (typeof s.time_before === 'number') ? s.time_before : timeBefore;
        timeAfter = (typeof s.time_after === 'number') ? s.time_after : timeAfter;
        defaultTime = (typeof s.default_time === 'number' && s.default_time > 0) ? s.default_time : defaultTime;
        IsAutoScoring = s.auto_scoring ? 1 : 0;
        IsAutoComplexity = s.auto_complexity ? 1 : 0;
        IsWinnerTakesAll = s.winner_takes_all ? 1 : 0;
        IsEarlyResponderWarning = s.early_responder_warning ? 1 : 0;
        EarlyResponderWarningTriggerCount = (typeof s.early_warning_trigger_count === 'number') ? s.early_warning_trigger_count : EarlyResponderWarningTriggerCount;
        EarlyResponderWarningSubValue = (typeof s.early_warning_sub_value === 'number') ? s.early_warning_sub_value : EarlyResponderWarningSubValue;
        autoAddValue = (typeof s.auto_add_value === 'number') ? s.auto_add_value : autoAddValue;
        autoSubValue = (typeof s.auto_sub_value === 'number') ? s.auto_sub_value : autoSubValue;
        TargetComplexity = (typeof s.target_complexity === 'number') ? s.target_complexity : 0;

        DefaultType = (typeof s.default_type === 'number' && s.default_type >= 0 && s.default_type <= 2) ? s.default_type : 0;
        if (DefaultType === 0) {
            document.getElementById('radioChoice').checked = true;
            document.getElementById('nextproblem').innerHTML = "下一选择题";
        } else if (DefaultType === 1) {
            document.getElementById('radioFillIn').checked = true;
            document.getElementById('nextproblem').innerHTML = "下一填空题";
        } else if (DefaultType === 2) {
            document.getElementById('radioInfo').checked = true;
            document.getElementById('nextproblem').innerHTML = "下一信息题";
        }

        ProblemID = (typeof s.problem_number === 'number') ? s.problem_number : 0;
        InfoID = (typeof s.info_id === 'number') ? s.info_id : 0;
        IsShowAnswer = s.show_answer ? 1 : 0;
        IsDiscarded = s.discarded ? 1 : 0;

        Round_Count = (typeof s.round_count === 'number') ? s.round_count : 0;
        Round_Team = (typeof s.round_team === 'number') ? s.round_team : Teams.length;
        Round_Team_Cnt = (typeof s.round_team_cnt === 'number') ? s.round_team_cnt : 0;
        Team_Problems_Count = (typeof s.team_problems_count === 'number') ? s.team_problems_count : 0;
        RoundAnswering = s.round_answering ? 1 : 0;
        IsRoundDelayed = s.round_delayed ? 1 : 0;
        SelectedTeam = (typeof s.selected_team === 'number') ? s.selected_team : 0;

        if (Array.isArray(s.early_warning_counts) && s.early_warning_counts.length === Teams.length) {
            EarlyWarningCount = s.early_warning_counts.slice();
        }
        applyTeamOrder(s.team_order);

        var mode = (s.mode === "round" || s.mode === "timer" || s.mode === "responder") ? s.mode : "responder";
        ResponderTimerMode = (mode === "responder") ? 1 : 0;

        if (mode === "responder") {
            document.getElementById('timertitle').innerHTML = "抢答";
            document.getElementById('changetimermode').innerHTML = IsAutoScoring == 1 ? "切换轮答" : "切换计时";
            document.getElementById('RoundInfo').style.display = "none";
            document.getElementById('Responder1').style.display = "grid";
            document.getElementById('Timer1').className = "quiz-timer-main";
            document.getElementById('ResponderControls').style.display = "block";
            document.getElementById('RoundControls').style.display = "none";
            document.getElementById('TimerControls').style.display = "none";

            if (s.responding) {
                document.getElementById('NowInfo').innerHTML = "抢答中...";
            } else if (SelectedTeam > 0 && SelectedTeam <= Teams.length) {
                document.getElementById('NowInfo').innerHTML = Teams[SelectedTeam - 1];
            } else {
                document.getElementById('NowInfo').innerHTML = "等待开始";
            }
        } else if (mode === "round") {
            document.getElementById('timertitle').innerHTML = "轮答";
            document.getElementById('changetimermode').innerHTML = "切换抢答";
            document.getElementById('RoundInfo').style.display = "block";
            document.getElementById('Responder1').style.display = "grid";
            document.getElementById('Timer1').className = "quiz-timer-main";
            document.getElementById('ResponderControls').style.display = "none";
            document.getElementById('TimerControls').style.display = "none";
            document.getElementById('RoundControls').style.display = "block";

            if (SelectedTeam > 0 && SelectedTeam <= Teams.length) {
                document.getElementById('NowInfo').innerHTML = Teams[SelectedTeam - 1];
                if (IsWinnerTakesAll == 0) {
                    document.getElementById('RoundInfo').innerHTML = "第" + Round_Count + "轮，第" + Round_Team_Cnt + "位次";
                } else {
                    var qOrd = RoundAnswering ? Math.max(1, Team_Problems_Count) : (Team_Problems_Count + 1);
                    document.getElementById('RoundInfo').innerHTML = "第" + Round_Team_Cnt + "位次，第" + qOrd + "题";
                }
            } else {
                document.getElementById('NowInfo').innerHTML = "等待开始";
                document.getElementById('RoundInfo').innerHTML = "切换下一题以自动开始轮答";
            }
        } else {
            document.getElementById('timertitle').innerHTML = "倒计时";
            document.getElementById('changetimermode').innerHTML = "切换抢答";
            document.getElementById('RoundInfo').style.display = "none";
            document.getElementById('Responder1').style.display = "none";
            document.getElementById('Timer1').className = "quiz-timer-only-main";
            document.getElementById('ResponderControls').style.display = "none";
            document.getElementById('RoundControls').style.display = "none";
            document.getElementById('TimerControls').style.display = "block";
        }

        if (typeof initial_problem_html === 'string' && initial_problem_html.length > 0 && s.current_id > 0) {
            applyRenderedProblem(initial_problem_html, s.current_type, {
                problemNumber: ProblemID,
                infoId: InfoID,
                showAnswer: IsShowAnswer == 1,
                discarded: IsDiscarded == 1
            });

            if (s.confirm_pending && IsAutoScoring == 1 && SelectedTeam > 0 && SelectedTeam <= Teams.length && IsDiscarded == 0) {
                document.getElementById('teamName_t1').innerHTML
                    = document.getElementById('teamName_t2').innerHTML
                    = document.getElementById('teamName_t3').innerHTML
                    = Teams[SelectedTeam - 1];
                document.getElementById('correctScoreChange').style.display = (autoAddValue > 0) ? "inline-block" : "none";
                document.getElementById('incorrectScoreChange').style.display = (autoSubValue > 0) ? "inline-block" : "none";
                isConfirmPending = true;
                confirmAnsToast.show();
            }
        }

        var hasUnsavedLatestDraw = false;
        if (typeof last_problem_type === 'number' && last_problem_type >= 0 && last_problem_type <= 2
            && typeof last_problem_id === 'number' && last_problem_id > 0
            && (last_problem_id !== s.current_id || last_problem_type !== s.current_type)) {
            LastProblemType = last_problem_type;
            LastProblemID = last_problem_id;
            pendingUnsavedDraw = true;
            hasUnsavedLatestDraw = true;
            updateReshowButton();
        }

        var nowSec = (typeof server_now === 'number' && server_now > 0) ? server_now : Math.floor(Date.now() / 1000);
        if (s.timer_started_at > 0 && !s.timer_paused) {
            var elapsed = Math.max(0, nowSec - s.timer_started_at);
            var remaining = (s.timer_duration || 0) - elapsed;
            if (remaining > 0) {
                IsResponderFlowRunning = s.responder_flow_running ? 1 : 0;
                IsRoundFlowRunning = s.round_flow_running ? 1 : 0;
                EarlyResponding = s.early_responding ? 1 : 0;
                Responding = 0;

                if (IsResponderFlowRunning == 1) {
                    document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
                    document.getElementById("ResponderSwitcher").className = "btn btn-danger btn-sm";
                    document.getElementById("ResponderSettings").disabled = true;
                }
                if (IsRoundFlowRunning == 1) {
                    document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
                    document.getElementById("RoundSwitcher").className = "btn btn-danger btn-sm";
                    document.getElementById("RoundSettings").disabled = true;
                    document.getElementById("problemsettings").disabled = true;
                    document.getElementById('nextproblem').disabled = true;
                }

                IntervalMode = (typeof s.timer_mode === 'number') ? s.timer_mode : 0;
                Interval_setTime(remaining);
                document.getElementById("IntervalSet").disabled = true;
                document.getElementById("IntervalStop").disabled = false;
                document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-pause-fill'>";
                IsStarted = 1;
                IsPaused = 0;
                IntervalID = setInterval(Interval_Counting, 1000);
                notyf.success("已恢复当前题目与计时（剩余 " + remaining + " 秒）");
            } else {
                IsResponderFlowRunning = 0;
                IsRoundFlowRunning = 0;
                Responding = 0;
                EarlyResponding = 0;
                IsStarted = 0;
                IsPaused = 0;
                IntervalMode = 0;
                if (mode === "round" && s.round_flow_running && RoundAnswering == 0) {
                    RoundDelay();
                }
                Interval_setTime(mode === "timer" ? defaultTime : timeBefore);
                notyf.error("计时已中断，可重新开始");
            }
        } else if (s.timer_paused && s.timer_remaining > 0) {
            IsResponderFlowRunning = s.responder_flow_running ? 1 : 0;
            IsRoundFlowRunning = s.round_flow_running ? 1 : 0;
            EarlyResponding = s.early_responding ? 1 : 0;
            Responding = 0;

            if (IsResponderFlowRunning == 1) {
                document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
                document.getElementById("ResponderSwitcher").className = "btn btn-danger btn-sm";
                document.getElementById("ResponderSettings").disabled = true;
            }
            if (IsRoundFlowRunning == 1) {
                document.getElementById('RoundSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
                document.getElementById("RoundSwitcher").className = "btn btn-danger btn-sm";
                document.getElementById("RoundSettings").disabled = true;
                document.getElementById("problemsettings").disabled = true;
                document.getElementById('nextproblem').disabled = true;
            }

            IntervalMode = (typeof s.timer_mode === 'number') ? s.timer_mode : 0;
            Interval_setTime(s.timer_remaining);
            document.getElementById("IntervalSet").disabled = true;
            document.getElementById("IntervalStop").disabled = false;
            document.getElementById('IntervalStart').innerHTML = "<i class='bi bi-play-fill'>";
            IsStarted = 1;
            IsPaused = 1;
            notyf.success("已恢复当前状态（计时已暂停，可点击继续或重新开始）");
        } else if (mode === "responder" && s.responding) {
            IsResponderFlowRunning = 1;
            Responding = 1;
            EarlyResponding = 0;
            document.getElementById('ResponderSwitcher').innerHTML = "<i class='bi bi-stop-fill'></i>";
            document.getElementById("ResponderSwitcher").className = "btn btn-danger btn-sm";
            document.getElementById("ResponderSettings").disabled = true;
            Interval_setTime(timeBefore);
            notyf.success("已恢复当前题目与抢答状态");
        } else {
            Interval_setTime(mode === "timer" ? defaultTime : timeBefore);
            if (s.round_flow_running || s.responder_flow_running) {
                if (mode === "round" && RoundAnswering == 0) {
                    RoundDelay();
                }
                notyf.error("计时已中断，可重新开始");
            } else if (s.current_id > 0) {
                notyf.success("已恢复当前题目与轮次进度");
            }
        }
    } finally {
        isRestoringSession = false;
    }

    if (hasUnsavedLatestDraw) {
        notyf.error("检测到上一道已抽取的题目未能写入快照，可点击「重新显示上一题」重试。");
    }

    // 若计时在刷新前已过期中断，将清理后的非运行态同步回服务端
    if ((s.timer_started_at > 0 && !s.timer_paused && (s.timer_duration - Math.max(0, ((typeof server_now === 'number' && server_now > 0) ? server_now : Math.floor(Date.now() / 1000)) - s.timer_started_at)) <= 0)
        || ((s.round_flow_running || s.responder_flow_running) && !(s.timer_started_at > 0 && !s.timer_paused) && !(s.timer_paused && s.timer_remaining > 0) && !(mode === "responder" && s.responding))) {
        syncSession({ timer_action: 'stop' });
    }
}

window.addEventListener('pagehide', function () {
    if (isEndingCompetition || !navigator.sendBeacon) {
        return;
    }
    var params = new URLSearchParams();
    params.set('guid', competition_guid);
    params.set('state', JSON.stringify(collectSessionState()));
    if (IsStarted && IsPaused) {
        params.set('timer_action', 'pause');
        params.set('timer_remaining', String(Seconds));
        params.set('timer_mode', String(IntervalMode));
    }
    navigator.sendBeacon('/main/competition/display/session', params);
});