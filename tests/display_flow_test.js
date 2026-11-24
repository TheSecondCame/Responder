const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function createHarness(initialGlobals = {}) {
    const elements = new Map();
    const timers = new Map();
    let nextTimerId = 1;
    const requests = [];
    const notyfErrors = [];
    const notyfSuccesses = [];

    function makeElement(id) {
        const listeners = {};
        const el = {
            id,
            innerHTML: '',
            value: '',
            checked: false,
            disabled: false,
            className: '',
            style: { display: 'block', zoom: '' },
            attributes: {},
            addEventListener(type, fn) {
                if (!listeners[type]) listeners[type] = [];
                listeners[type].push(fn);
            },
            dispatchEvent(type) {
                for (const fn of listeners[type] || []) fn({});
            },
            getAttribute(name) {
                return Object.prototype.hasOwnProperty.call(this.attributes, name) ? this.attributes[name] : null;
            },
            setAttribute(name, val) {
                this.attributes[name] = String(val);
            }
        };
        if (id === 'problemarea') {
            el.style.display = 'none';
        }
        return el;
    }

    function getEl(id) {
        if (!elements.has(id)) {
            elements.set(id, makeElement(id));
        }
        return elements.get(id);
    }

    // Seed radios and key elements
    getEl('radioChoice').checked = true;
    getEl('reshowproblem').disabled = true;

    const teamNameCells = [makeElement('tn0'), makeElement('tn1'), makeElement('tn2')];
    const teamScoreCells = [makeElement('ts0'), makeElement('ts1'), makeElement('ts2')];
    const docListeners = {};
    const winListeners = {};
    const readyCbs = [];

    const document = {
        documentElement: { clientWidth: 1470 },
        body: { style: {} },
        getElementById(id) {
            if (id === 'problemMeta') {
                return elements.get('problemMeta') || null;
            }
            return getEl(id);
        },
        querySelector() {
            return document.documentElement;
        },
        querySelectorAll(selector) {
            if (selector === '.quiz-team-name') return teamNameCells;
            if (selector === '.quiz-team-score') return teamScoreCells;
            if (selector === 'input[name="questionType"]') return [getEl('radioChoice'), getEl('radioFillIn'), getEl('radioInfo')];
            return [];
        },
        addEventListener(type, fn) {
            if (!docListeners[type]) docListeners[type] = [];
            docListeners[type].push(fn);
        },
        onkeydown: null
    };

    const window = {
        document,
        onbeforeunload: null,
        addEventListener(type, fn) {
            if (!winListeners[type]) winListeners[type] = [];
            winListeners[type].push(fn);
        }
    };

    const context = {
        window,
        document,
        navigator: {},
        console,
        Math,
        Number,
        parseInt,
        isNaN,
        String,
        Array,
        Object,
        JSON,
        Promise,
        URLSearchParams,
        encodeURIComponent,
        setTimeout(fn) { return nextTimerId++; },
        clearTimeout() {},
        setInterval(fn, ms) {
            const id = nextTimerId++;
            timers.set(id, fn);
            return id;
        },
        clearInterval(id) {
            timers.delete(id);
        },
        Audio: function () { return { play() {} }; },
        Notyf: function () {
            return {
                error(msg) { notyfErrors.push(msg); },
                success(msg) { notyfSuccesses.push(msg); }
            };
        },
        bootstrap: {
            Toast: function (el) {
                return { el, shown: false, show() { this.shown = true; }, hide() { this.shown = false; } };
            }
        },
        Swal: {
            fire() { return Promise.resolve({ isConfirmed: false }); }
        },
        jQuery: null,
        $: null,
        competition_guid: 'test-guid',
        Teams: ['Alpha', 'Beta', 'Gamma'],
        scores: [0, 0, 0],
        team_keys: [0, 1, 2],
        last_problem_type: -1,
        last_problem_id: 0,
        has_initial_session: false,
        initial_session: null,
        initial_problem_html: '',
        server_now: Math.floor(Date.now() / 1000),
        ...initialGlobals
    };
    context.self = context;
    context.window = context;
    context.addEventListener = window.addEventListener;

    let ajaxHandler = () => '';
    function $(arg) {
        if (arg === document) {
            return { ready(cb) { readyCbs.push(cb); } };
        }
        return { on() {} };
    }
    $.ajax = function (opts) {
        requests.push({ url: opts.url, data: opts.data, type: opts.type });
        let resolved;
        try {
            const res = ajaxHandler(opts);
            resolved = res;
            if (typeof opts.success === 'function') {
                opts.success(res);
            }
        } catch (err) {
            if (typeof opts.error === 'function') {
                opts.error({}, 'error', err);
            }
            return Promise.reject(err);
        }
        return Promise.resolve(resolved);
    };
    context.$ = $;
    context.jQuery = $;

    // Intercept innerHTML assignment on dymproblemarea so problemMeta gets parsed
    const dymArea = getEl('dymproblemarea');
    let dymHtml = '';
    Object.defineProperty(dymArea, 'innerHTML', {
        get() { return dymHtml; },
        set(val) {
            dymHtml = val;
            const m = /id="problemMeta" data-type="(\d+)" data-id="(\d+)"/.exec(val);
            if (m) {
                const meta = makeElement('problemMeta');
                meta.setAttribute('data-type', m[1]);
                meta.setAttribute('data-id', m[2]);
                elements.set('problemMeta', meta);
            } else {
                elements.delete('problemMeta');
            }
        }
    });

    vm.createContext(context);
    const jsCode = fs.readFileSync(path.join(__dirname, '..', 'assets', 'js', 'competition_display.js'), 'utf8');
    vm.runInContext(jsCode, context, { filename: 'competition_display.js' });

    for (const fn of docListeners['DOMContentLoaded'] || []) {
        fn();
    }

    return {
        context,
        getEl,
        requests,
        notyfErrors,
        notyfSuccesses,
        timers,
        setAjaxHandler(fn) { ajaxHandler = fn; },
        tickTimerOnce() {
            const fns = Array.from(timers.values());
            for (const fn of fns) {
                if (typeof fn === 'function') fn();
            }
        }
    };
}

function makeProblemHtml(id, type = 0) {
    return `<div id="problemMeta" data-type="${type}" data-id="${id}"></div><div>Question ${id}</div>`;
}

// Scenario 1: Auto-round mode: draw Q1 -> Q2 snapshot write fail -> empty-body reshow retry -> grade -> next team
async function testAutoRoundFailureRecoveryAndNextTeam() {
    const h = createHarness();
    const ctx = h.context;

    // Configure auto-round mode: WinnerTakesAll = 1, timeBefore = 0, timeAfter = 15
    ctx.IsAutoScoring = 1;
    ctx.ResponderTimerMode = 0;
    ctx.IsWinnerTakesAll = 1;
    ctx.timeBefore = 0;
    ctx.timeAfter = 15;

    let consumedCount = 0;
    let sessionWriteFail = false;
    let lastDrawnId = 0;
    const syncedPayloads = [];
    const scorePosts = [];

    h.setAjaxHandler((opts) => {
        if (opts.url.startsWith('/main/competition/display/problem')) {
            if (opts.url.includes('requestcomplexity=1')) {
                return '1';
            }
            if (opts.url.includes('reshow=1')) {
                assert.strictEqual(opts.data, '', 'Browser reshowLastProblem must send empty body');
                if (sessionWriteFail) {
                    return `fail : session_save_failed|0|${lastDrawnId}`;
                }
                return makeProblemHtml(lastDrawnId, 0);
            }
            consumedCount++;
            lastDrawnId = consumedCount;
            if (sessionWriteFail) {
                return `fail : session_save_failed|0|${lastDrawnId}`;
            }
            return makeProblemHtml(lastDrawnId, 0);
        }
        if (opts.url === '/main/competition/display/session') {
            syncedPayloads.push({
                ...opts.data,
                parsedState: opts.data.state ? JSON.parse(opts.data.state) : null
            });
            return 'success';
        }
        if (opts.url.startsWith('/main/competition/display/scores')) {
            scorePosts.push(opts.url);
            return 'success';
        }
        return 'success';
    });

    // Step 1: Team 1 (Alpha) draws Q1 normally
    ctx.buttonNextProblem();
    assert.strictEqual(consumedCount, 1);
    assert.strictEqual(ctx.Round_Team, 1);
    assert.strictEqual(ctx.Round_Team_Cnt, 1);
    assert.strictEqual(ctx.Round_Count, 1);
    assert.strictEqual(ctx.SelectedTeam, 1);
    assert.strictEqual(ctx.ProblemID, 1);
    assert.strictEqual(ctx.Team_Problems_Count, 1);
    assert.strictEqual(ctx.RoundAnswering, 1);
    assert.strictEqual(ctx.IsStarted, 1);
    assert.strictEqual(ctx.Seconds, 15);

    // Team 1 finishes answering (wrong, so turn passes to Team 2)
    ctx.ShowAnswer();
    assert.strictEqual(ctx.IsRoundFlowRunning, 0);
    assert.strictEqual(ctx.IsRoundDelayed, 0);
    ctx.autoScoring_Incorrect();

    // Step 2: Next turn (Team 2 Beta), simulate session.json write failure on Q2
    sessionWriteFail = true;
    ctx.buttonNextProblem();

    assert.strictEqual(consumedCount, 2, 'Server drew and consumed Q2');
    assert.strictEqual(lastDrawnId, 2);
    // Crucial assertions: Team 2 must NOT be rolled back to Team 1!
    assert.strictEqual(ctx.Round_Team, 2, 'Round_Team must stay on Team 2 after session_save_failed');
    assert.strictEqual(ctx.Round_Team_Cnt, 2, 'Round_Team_Cnt must stay at 2');
    assert.strictEqual(ctx.Round_Count, 1);
    assert.strictEqual(ctx.SelectedTeam, 2);
    assert.strictEqual(ctx.IsRoundDelayed, 0, 'RoundDelay must NOT run on session_save_failed');
    assert.strictEqual(ctx.IsRoundFlowRunning, 1, 'Round flow must stay active awaiting reshow recovery');
    assert.strictEqual(ctx.RoundAnswering, 0);
    assert.strictEqual(ctx.ProblemID, 1, 'ProblemID must not advance until Q2 is actually rendered');
    assert.strictEqual(ctx.Team_Problems_Count, 0, 'Team_Problems_Count must not increment before Q2 renders');
    assert.strictEqual(h.getEl('reshowproblem').disabled, false, '#reshowproblem must be enabled for retry');
    assert.strictEqual(h.getEl('nextproblem').disabled, true, '#nextproblem must remain disabled');

    // Retrying while disk is still read-only keeps state intact
    ctx.reshowLastProblem();
    assert.strictEqual(consumedCount, 2);
    assert.strictEqual(ctx.Round_Team, 2);
    assert.strictEqual(ctx.ProblemID, 1);
    assert.strictEqual(ctx.IsRoundFlowRunning, 1);

    // Step 3: Disk permissions restored -> click「重新显示上一题」
    sessionWriteFail = false;
    syncedPayloads.length = 0;
    ctx.reshowLastProblem();
    await ctx.sessionSyncChain;

    assert.strictEqual(consumedCount, 2, 'Retry must NOT consume another problem from bank');
    assert.strictEqual(ctx.ProblemID, 2, 'ProblemID must increment to 2');
    assert.strictEqual(h.getEl('problemtitle').innerHTML, '第2题');
    assert.strictEqual(ctx.Team_Problems_Count, 1, 'Winner-takes-all Team_Problems_Count must increment once');
    assert.strictEqual(ctx.RoundAnswering, 1, 'RoundAnswering must become 1');
    assert.strictEqual(ctx.IsRoundFlowRunning, 1);
    assert.strictEqual(ctx.Round_Team, 2);
    assert.strictEqual(ctx.SelectedTeam, 2);
    assert.strictEqual(ctx.IsStarted, 1, 'Answering timer must start after retry succeeds');
    assert.strictEqual(ctx.IntervalMode, 0);
    assert.strictEqual(ctx.Seconds, 15);
    assert.ok(syncedPayloads.some((p) => p.timer_action === 'start'
        && p.parsedState.round_team === 2
        && p.parsedState.problem_number === 2
        && p.parsedState.round_answering === true
        && p.parsedState.team_problems_count === 1),
        'syncSession must persist recovered round_team=2, problem_number=2, round_answering=true, timer=start');

    // Clicking reshow a second time must be a pure idempotent re-render (no double increment!)
    const syncCountBeforeExtraReshow = syncedPayloads.length;
    ctx.reshowLastProblem();
    assert.strictEqual(ctx.ProblemID, 2);
    assert.strictEqual(ctx.Team_Problems_Count, 1);
    assert.strictEqual(syncedPayloads.length, syncCountBeforeExtraReshow);

    // Step 4: Team 2 answers -> ShowAnswer & grade
    ctx.ShowAnswer();
    assert.strictEqual(ctx.IsRoundFlowRunning, 0);
    assert.strictEqual(ctx.IsRoundDelayed, 0);
    ctx.autoScoring_Incorrect();
    assert.strictEqual(scorePosts.length, 2);

    // Step 5: Next team (Team 3 Gamma) draws Q3 — must advance cleanly to Team 3, NOT repeat Team 2
    ctx.buttonNextProblem();
    assert.strictEqual(consumedCount, 3);
    assert.strictEqual(ctx.Round_Team, 3, 'Must advance cleanly to Team 3 after Team 2');
    assert.strictEqual(ctx.Round_Team_Cnt, 3);
    assert.strictEqual(ctx.SelectedTeam, 3);
    assert.strictEqual(ctx.ProblemID, 3);
    assert.strictEqual(ctx.RoundAnswering, 1);
}

// Scenario 2: Auto-round with timeBefore > 0 + F5 refresh between failure and retry
async function testAutoRoundF5RefreshBeforeRetry() {
    const now = Math.floor(Date.now() / 1000);
    const h = createHarness({
        server_now: now,
        last_problem_type: 0,
        last_problem_id: 2,
        has_initial_session: true,
        initial_problem_html: makeProblemHtml(1, 0),
        initial_session: {
            current_type: 0,
            current_id: 1,
            round_team: 2,
            round_team_cnt: 2,
            round_count: 1,
            team_problems_count: 0,
            round_answering: false,
            round_delayed: false,
            round_flow_running: true,
            selected_team: 2,
            mode: 'round',
            auto_scoring: true,
            winner_takes_all: true,
            time_before: 3,
            time_after: 12,
            problem_number: 1,
            timer_started_at: now - 10,
            timer_duration: 3,
            timer_paused: false,
            timer_mode: 2
        }
    });
    const ctx = h.context;

    assert.strictEqual(ctx.Round_Team, 2, 'F5 restore must keep Round_Team=2');
    assert.strictEqual(ctx.IsRoundDelayed, 0, 'F5 restore must NOT call RoundDelay when unsaved draw exists');
    assert.strictEqual(ctx.IsRoundFlowRunning, 1, 'F5 restore must preserve IsRoundFlowRunning=1');
    assert.strictEqual(h.getEl('reshowproblem').disabled, false);

    let reshowEmptyBody = null;
    h.setAjaxHandler((opts) => {
        if (opts.url.includes('reshow=1')) {
            reshowEmptyBody = opts.data;
            return makeProblemHtml(2, 0);
        }
        return 'success';
    });

    ctx.reshowLastProblem();
    assert.strictEqual(reshowEmptyBody, '');
    assert.strictEqual(ctx.ProblemID, 2);
    assert.strictEqual(ctx.Team_Problems_Count, 1);
    assert.strictEqual(ctx.RoundAnswering, 1);
    assert.strictEqual(ctx.IsStarted, 1);
    assert.strictEqual(ctx.Seconds, 12);
}

// Scenario 3: Responder mode write failure -> empty-body reshow triggers ResponderFlow once
async function testResponderModeFailureRecovery() {
    const h = createHarness();
    const ctx = h.context;
    ctx.IsAutoScoring = 1;
    ctx.ResponderTimerMode = 1;
    ctx.timeBefore = 3;
    ctx.timeAfter = 15;

    let failWrite = true;
    let drawCalls = 0;
    h.setAjaxHandler((opts) => {
        if (opts.url.startsWith('/main/competition/display/problem')) {
            if (opts.url.includes('reshow=1')) {
                assert.strictEqual(opts.data, '');
                return makeProblemHtml(5, 0);
            }
            drawCalls++;
            if (failWrite) return 'fail : session_save_failed|0|5';
            return makeProblemHtml(5, 0);
        }
        return 'success';
    });

    ctx.buttonNextProblem();
    assert.strictEqual(drawCalls, 1);
    assert.strictEqual(ctx.IsResponderFlowRunning, 0);
    assert.strictEqual(ctx.ProblemID, 0);
    assert.strictEqual(h.getEl('reshowproblem').disabled, false);

    failWrite = false;
    ctx.reshowLastProblem();
    assert.strictEqual(drawCalls, 1, 'Must not draw again on reshow');
    assert.strictEqual(ctx.ProblemID, 1);
    assert.strictEqual(ctx.IsResponderFlowRunning, 1, 'ResponderFlow must start after recovery');
    assert.strictEqual(ctx.IntervalMode, 1);
    assert.strictEqual(ctx.Seconds, 3);
}

// Scenario 4: Auto-round with non-zero timeBefore countdown & IsWinnerTakesAll=0
async function testAutoRoundWithTimeBeforeCountdownAndCorrectAnswer() {
    const h = createHarness();
    const ctx = h.context;
    ctx.IsAutoScoring = 1;
    ctx.ResponderTimerMode = 0;
    ctx.IsWinnerTakesAll = 0;
    ctx.timeBefore = 2;
    ctx.timeAfter = 10;

    let failWrite = true;
    let drawCalls = 0;
    h.setAjaxHandler((opts) => {
        if (opts.url.startsWith('/main/competition/display/problem')) {
            if (opts.url.includes('requestcomplexity=1')) return '1';
            if (opts.url.includes('reshow=1')) {
                assert.strictEqual(opts.data, '');
                return makeProblemHtml(10 + drawCalls, 0);
            }
            drawCalls++;
            if (failWrite && drawCalls === 1) {
                return 'fail : session_save_failed|0|11';
            }
            return makeProblemHtml(10 + drawCalls, 0);
        }
        return 'success';
    });

    // Start Team 1 with timeBefore=2 countdown -> tick down to 0 -> triggers Problem(0) which fails
    ctx.buttonNextProblem();
    assert.strictEqual(ctx.Round_Team, 1);
    assert.strictEqual(ctx.IntervalMode, 2);
    assert.strictEqual(ctx.Seconds, 2);
    h.tickTimerOnce();
    assert.strictEqual(ctx.Seconds, 1);
    h.tickTimerOnce();
    assert.strictEqual(drawCalls, 1);
    assert.strictEqual(ctx.Round_Team, 1);
    assert.strictEqual(ctx.Round_Count, 1);
    assert.strictEqual(ctx.IsRoundDelayed, 0);
    assert.strictEqual(ctx.IsRoundFlowRunning, 1);
    assert.strictEqual(ctx.RoundAnswering, 0);
    assert.strictEqual(ctx.ProblemID, 0);

    // Recover Q1 via empty-body reshow
    failWrite = false;
    ctx.reshowLastProblem();
    assert.strictEqual(drawCalls, 1);
    assert.strictEqual(ctx.ProblemID, 1);
    assert.strictEqual(ctx.RoundAnswering, 1);
    assert.strictEqual(ctx.IsStarted, 1);
    assert.strictEqual(ctx.IntervalMode, 0);
    assert.strictEqual(ctx.Seconds, 10);

    // Grade Team 1 correct (IsWinnerTakesAll=0 does not set IsRoundDelayed), advance to Team 2
    ctx.ShowAnswer();
    ctx.autoScoring_Correct();
    assert.strictEqual(ctx.scores[0], 1);
    assert.strictEqual(ctx.IsRoundDelayed, 0);

    ctx.buttonNextProblem();
    assert.strictEqual(ctx.Round_Team, 2, 'Team must advance to Team 2 after Team 1 answered');
    assert.strictEqual(ctx.SelectedTeam, 2);
    h.tickTimerOnce();
    h.tickTimerOnce();
    assert.strictEqual(drawCalls, 2);
    assert.strictEqual(ctx.ProblemID, 2);
}

// Scenario 5: Normal reshow failure on an already-displayed & revealed problem (e.g. Q3 with id=5)
// must preserve ProblemID, answer visibility, timer, and scoring state across failure -> retry
async function testNormalReshowFailureOnRevealedProblemPreservesState() {
    for (const modeCfg of [
        { name: 'responder', responderTimerMode: 1 },
        { name: 'round', responderTimerMode: 0 }
    ]) {
        const h = createHarness({
            scores: [2, 1, 0]
        });
        const ctx = h.context;
        ctx.IsAutoScoring = 1;
        ctx.ResponderTimerMode = modeCfg.responderTimerMode;
        ctx.timeBefore = 3;
        ctx.timeAfter = 15;
        ctx.ProblemID = 3;
        ctx.CurrentProblemType = 0;
        ctx.CurrentProblemID = 5;
        ctx.LastProblemType = 0;
        ctx.LastProblemID = 5;
        ctx.SelectedTeam = 1;
        ctx.Team_Problems_Count = 1;
        ctx.Round_Team = 2;
        ctx.Round_Team_Cnt = 2;
        ctx.Round_Count = 1;
        ctx.IsRoundFlowRunning = 0;
        ctx.IsResponderFlowRunning = 0;
        ctx.applyRenderedProblem(makeProblemHtml(5, 0), 0, {
            problemNumber: 3,
            infoId: 0,
            showAnswer: false,
            discarded: false
        });
        ctx.ShowAnswer();

        assert.strictEqual(ctx.ProblemID, 3);
        assert.strictEqual(ctx.IsShowAnswer, 1);
        assert.strictEqual(h.getEl('ansarea').style.display, 'block');
        assert.strictEqual(h.getEl('showanswer').disabled, true);
        assert.strictEqual(ctx.isConfirmPending, true);

        let failWrite = true;
        let reshowCalls = 0;
        let drawCalls = 0;
        let scorePosts = 0;
        h.setAjaxHandler((opts) => {
            if (opts.url.startsWith('/main/competition/display/problem')) {
                if (opts.url.includes('reshow=1')) {
                    reshowCalls++;
                    assert.strictEqual(opts.data, '');
                    if (failWrite) {
                        return 'fail : session_save_failed|0|5';
                    }
                    return makeProblemHtml(5, 0);
                }
                drawCalls++;
                return '';
            }
            if (opts.url.startsWith('/main/competition/display/scores')) {
                scorePosts++;
                return 'success';
            }
            return 'success';
        });

        // First reshow fails snapshot save
        ctx.reshowLastProblem();
        assert.strictEqual(reshowCalls, 1);
        assert.strictEqual(ctx.pendingUnsavedDraw, null, `(${modeCfg.name}) normal reshow failure must not set pendingUnsavedDraw`);
        assert.strictEqual(ctx.ProblemID, 3);
        assert.strictEqual(ctx.IsShowAnswer, 1);

        // Second reshow succeeds
        failWrite = false;
        ctx.reshowLastProblem();
        assert.strictEqual(reshowCalls, 2);
        assert.strictEqual(drawCalls, 0);
        assert.strictEqual(scorePosts, 0);
        assert.strictEqual(ctx.pendingUnsavedDraw, null);
        assert.strictEqual(ctx.ProblemID, 3, `(${modeCfg.name}) ProblemID must remain 3`);
        assert.strictEqual(h.getEl('problemtitle').innerHTML, '第3题');
        assert.strictEqual(ctx.CurrentProblemID, 5);
        assert.strictEqual(ctx.IsShowAnswer, 1, `(${modeCfg.name}) answer visibility must stay revealed`);
        assert.strictEqual(h.getEl('ansarea').style.display, 'block');
        assert.strictEqual(h.getEl('showanswer').disabled, true);
        assert.strictEqual(ctx.IsStarted, 0, `(${modeCfg.name}) timer must remain stopped`);
        assert.strictEqual(ctx.IsResponderFlowRunning, 0, `(${modeCfg.name}) ResponderFlow must not start`);
        assert.strictEqual(ctx.IsRoundFlowRunning, 0, `(${modeCfg.name}) RoundFlow must not start`);
        assert.strictEqual(ctx.Team_Problems_Count, 1, `(${modeCfg.name}) Team_Problems_Count must stay 1`);
        assert.strictEqual(ctx.Round_Team, 2);
        assert.deepStrictEqual(Array.from(ctx.scores), [2, 1, 0]);
        assert.strictEqual(ctx.isConfirmPending, true);
    }
}

async function testEndCompetitionDoubleConfirmation() {
    const swalResponses = [];
    const h = createHarness({
        location: { href: '' },
        Swal: {
            fire(opts) {
                const next = swalResponses.shift() || { isConfirmed: false, isDenied: false, isDismissed: true };
                return Promise.resolve(next);
            }
        }
    });
    const ctx = h.context;
    const endCalls = [];
    h.setAjaxHandler((opts) => {
        if (opts.url.startsWith('/main/competition/display/enddisplay')) {
            endCalls.push(opts.url);
            return 'success';
        }
        return 'success';
    });

    // 1) Dismissing the dialog must NOT pause or finish the competition or clear onbeforeunload
    swalResponses.push({ isConfirmed: false, isDenied: false, isDismissed: true });
    ctx.EndCompetition();
    await new Promise((r) => setImmediate(r));
    assert.strictEqual(endCalls.length, 0);
    assert.strictEqual(typeof ctx.onbeforeunload, 'function');

    // 2) Choosing "结束比赛" (isDenied) and cancelling the second confirmation must NOT finish
    swalResponses.push({ isConfirmed: false, isDenied: true, isDismissed: false });
    swalResponses.push({ isConfirmed: false, isDenied: false, isDismissed: true });
    ctx.EndCompetition();
    await new Promise((r) => setImmediate(r));
    await new Promise((r) => setImmediate(r));
    assert.strictEqual(endCalls.length, 0);
    assert.strictEqual(typeof ctx.onbeforeunload, 'function');

    // 3) Choosing "结束比赛" (isDenied) and confirming the second prompt sends action=1
    swalResponses.push({ isConfirmed: false, isDenied: true, isDismissed: false });
    swalResponses.push({ isConfirmed: true, isDenied: false, isDismissed: false });
    ctx.EndCompetition();
    await new Promise((r) => setImmediate(r));
    await new Promise((r) => setImmediate(r));
    assert.strictEqual(endCalls.length, 1);
    assert.ok(endCalls[0].includes('action=1'));
    assert.strictEqual(ctx.onbeforeunload, undefined);
}

(async () => {
    await testAutoRoundFailureRecoveryAndNextTeam();
    await testAutoRoundF5RefreshBeforeRetry();
    await testResponderModeFailureRecovery();
    await testAutoRoundWithTimeBeforeCountdownAndCorrectAnswer();
    await testNormalReshowFailureOnRevealedProblemPreservesState();
    await testEndCompetitionDoubleConfirmation();
    console.log('All JS display flow tests passed.');
})().catch((err) => {
    console.error(err);
    process.exit(1);
});
