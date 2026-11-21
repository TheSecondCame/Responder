param(
    [string]$ExePath = ".\x64\Release\Server.exe",
    [string]$AssetsDir = ".\assets",
    [int]$Port = 8087
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}
if (-not (Test-Path $AssetsDir)) {
    throw "Assets directory not found: $AssetsDir"
}

$resolvedExe = (Resolve-Path $ExePath).Path
$resolvedAssets = (Resolve-Path $AssetsDir).Path

# Verify PE headers via dumpbin if available
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($dumpbin) {
    $headers = & $dumpbin.Source /headers $resolvedExe | Out-String
    Write-Host "=== PE Headers (summary) ==="
    ($headers -split "`r?`n" | Select-String -Pattern "machine|subsystem|entry point") | ForEach-Object {
        Write-Host $_.Line.Trim()
    }
    if ($headers -notmatch "x64") {
        throw "Expected x64 PE binary"
    }
}

$staging = Join-Path $env:TEMP ("responder_smoke_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $staging | Out-Null

$proc = $null
try {
    $stagedExe = Join-Path $staging "Server.exe"
    Copy-Item $resolvedExe $stagedExe -Force
    Copy-Item $resolvedAssets (Join-Path $staging "assets") -Recurse -Force

    $logPath = Join-Path $staging "server.log"
    $stdoutPath = Join-Path $staging "stdout.log"
    $stderrPath = Join-Path $staging "stderr.log"

    $proc = Start-Process -FilePath $stagedExe `
        -ArgumentList "--console" `
        -WorkingDirectory $staging `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    $pin = $null
    $listening = $false
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        if ($proc.HasExited) {
            $logDump = if (Test-Path $logPath) { Get-Content $logPath -Raw -Encoding UTF8 } else { "<no server.log>" }
            throw "Server.exe exited prematurely with code $($proc.ExitCode). Log:`n$logDump"
        }

        if (Test-Path $logPath) {
            $logText = Get-Content $logPath -Raw -Encoding UTF8
            if (-not $pin -and $logText -match "PIN code for login:\s*(\d{6})") {
                $pin = $Matches[1]
            }
            if ($logText -match "Starting server on port\s+$Port") {
                try {
                    $client = New-Object System.Net.Sockets.TcpClient
                    $ar = $client.BeginConnect("127.0.0.1", $Port, $null, $null)
                    if ($ar.AsyncWaitHandle.WaitOne(200)) {
                        $client.EndConnect($ar)
                        $client.Close()
                        $listening = $true
                        break
                    }
                    $client.Close()
                } catch {
                }
            }
        }
        Start-Sleep -Milliseconds 200
    }

    if (-not $listening -or -not $pin) {
        $logDump = if (Test-Path $logPath) { Get-Content $logPath -Raw -Encoding UTF8 } else { "<no server.log>" }
        throw "Timed out waiting for server on port $Port (listening=$listening, pin=$pin). Log:`n$logDump"
    }

    $baseUrl = "http://127.0.0.1:$Port"

    $rWelcome = Invoke-WebRequest -Uri "$baseUrl/" -UseBasicParsing
    if ($rWelcome.StatusCode -ne 200 -or $rWelcome.Content -notmatch "Responder") {
        throw "GET / failed validation"
    }

    $rStart = Invoke-WebRequest -Uri "$baseUrl/start" -UseBasicParsing
    if ($rStart.StatusCode -ne 200) {
        throw "GET /start returned $($rStart.StatusCode)"
    }

    $rAsset = Invoke-WebRequest -Uri "$baseUrl/assets/css/style.css" -UseBasicParsing
    if ($rAsset.StatusCode -ne 200) {
        throw "GET /assets/css/style.css returned $($rAsset.StatusCode)"
    }

    $rUnauth = Invoke-WebRequest -Uri "$baseUrl/main" -UseBasicParsing
    if ($rUnauth.Content -notmatch 'window\.location\.href\s*=\s*"/"') {
        throw "Expected unauthenticated /main to redirect to /"
    }

    $rLogin = Invoke-WebRequest -Uri "$baseUrl/start" -Method Post -Body "pin=$pin" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rLogin.Content -notmatch 'document\.cookie\s*=\s*"token=([A-Z0-9]{50})') {
        throw "Failed to extract session token from POST /start response"
    }
    $token = $Matches[1]

    $headers = @{ Cookie = "token=$token" }
    $rMain = Invoke-WebRequest -Uri "$baseUrl/main" -Headers $headers -UseBasicParsing
    if ($rMain.StatusCode -ne 200 -or $rMain.Content -notmatch "Responder") {
        throw "Authenticated GET /main failed"
    }

    $rProblems = Invoke-WebRequest -Uri "$baseUrl/main/problems" -Headers $headers -UseBasicParsing
    if ($rProblems.StatusCode -ne 200) {
        throw "Authenticated GET /main/problems failed"
    }

    $rBackup = Invoke-WebRequest -Uri "$baseUrl/main/problems" -Method Post -Headers $headers -Body "action=backup" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rBackup.StatusCode -ne 200 -or -not (Test-Path (Join-Path $staging "data\problems_backup"))) {
        throw "POST /main/problems action=backup failed"
    }

    # Upload a minimal UTF-8 problem bank and test display session persistence & recovery
    $bankText = @(
        "1 0",
        "CI Choice Question 1",
        "Option A",
        "Option B",
        "Option C",
        "Option D",
        "A",
        "",
        "2 0",
        "CI Choice Question 2",
        "Opt 1",
        "Opt 2",
        "Opt 3",
        "Opt 4",
        "B",
        "",
        "1 1",
        "CI Completion Question 1",
        "42",
        "",
        "1 2",
        "CI Info Question 1",
        "Clue 1",
        "Clue 2",
        "Clue 3",
        "Clue 4",
        "Answer"
    ) -join "`n"
    $bankBytes = [System.Text.Encoding]::UTF8.GetBytes($bankText)

    Add-Type -AssemblyName System.Net.Http
    $httpHandler = New-Object System.Net.Http.HttpClientHandler
    $httpHandler.UseCookies = $false
    $httpClient = New-Object System.Net.Http.HttpClient($httpHandler)
    try {
        $httpClient.DefaultRequestHeaders.Add("Cookie", "token=$token")
        $multipart = New-Object System.Net.Http.MultipartFormDataContent
        $actionContent = New-Object System.Net.Http.StringContent("convert")
        $multipart.Add($actionContent, "action")
        $fileContent = New-Object System.Net.Http.ByteArrayContent(,$bankBytes)
        $fileContent.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("text/plain")
        $multipart.Add($fileContent, "file", "bank.txt")
        $uploadResp = $httpClient.PostAsync("$baseUrl/main/problems", $multipart).GetAwaiter().GetResult()
        if (-not $uploadResp.IsSuccessStatusCode) {
            throw "POST /main/problems convert failed with status $($uploadResp.StatusCode)"
        }
    } finally {
        $httpClient.Dispose()
    }

    $problemListPath = Join-Path $staging "data\problems\problem_list.json"
    if (-not (Test-Path $problemListPath)) {
        throw "problem_list.json was not created after convert"
    }
    $plBefore = Get-Content $problemListPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($plBefore.choice_problems.Count -ne 2) {
        throw "Expected 2 choice problems after convert, got $($plBefore.choice_problems.Count)"
    }

    $newCompBody = "name=CISessionMatch&description=Smoke&choiceCount=2&completionCount=1&infoCount=1&teamName1=Alpha&teamName2=Beta"
    $rNewComp = Invoke-WebRequest -Uri "$baseUrl/main/competition/new" -Method Post -Headers $headers -Body $newCompBody -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rNewComp.Content -notmatch 'guid=([A-Fa-f0-9\-]+)') {
        throw "Failed to extract competition GUID from POST /main/competition/new"
    }
    $compGuid = $Matches[1]

    $rDisplayInit = Invoke-WebRequest -Uri "$baseUrl/main/competition/display?guid=$compGuid" -Headers $headers -UseBasicParsing
    if ($rDisplayInit.StatusCode -ne 200 -or $rDisplayInit.Content -notmatch 'var has_initial_session = false;') {
        throw "Initial GET /main/competition/display did not report has_initial_session = false"
    }

    $sessionState = '{"mode":"round","round_team":2,"round_team_cnt":2,"round_count":15,"auto_scoring":true,"problem_number":1,"team_order":[1,0]}'
    $encodedState = [System.Uri]::EscapeDataString($sessionState)
    $rDraw = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&type=0" -Method Post -Headers $headers -Body "state=$encodedState" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rDraw.StatusCode -ne 200 -or $rDraw.Content -notmatch 'id="problemMeta" data-type="0" data-id="(\d+)"') {
        throw "POST /main/competition/display/problem did not return expected problemMeta"
    }
    $drawnId = [int]$Matches[1]

    $rTimerStart = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/session" -Method Post -Headers $headers -Body "guid=$compGuid&timer_action=start&timer_duration=15&timer_mode=0" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rTimerStart.Content.Trim() -ne "success") {
        throw "POST /main/competition/display/session failed: $($rTimerStart.Content)"
    }

    function Get-UsedChoiceCount($path) {
        $plJson = Get-Content $path -Raw -Encoding UTF8 | ConvertFrom-Json
        $used = 0
        foreach ($entry in $plJson.choice_problems) {
            if ($entry[1] -eq 1) {
                $used++
            }
        }
        return $used
    }

    $usedAfterDraw = Get-UsedChoiceCount $problemListPath
    if ($usedAfterDraw -ne 1) {
        throw "Expected 1 used choice problem after draw, got $usedAfterDraw"
    }

    # Simulate browser F5 refresh: session & question must be restored without consuming a new problem
    $rDisplayRefresh = Invoke-WebRequest -Uri "$baseUrl/main/competition/display?guid=$compGuid" -Headers $headers -UseBasicParsing
    if ($rDisplayRefresh.Content -notmatch 'var has_initial_session = true;') {
        throw "Refreshed GET /main/competition/display did not report has_initial_session = true"
    }
    if ($rDisplayRefresh.Content -notmatch 'var initial_session = (\{[^\r\n]+\});') {
        throw "Refreshed GET /main/competition/display missing initial_session JSON"
    }
    $restoredSession = $Matches[1] | ConvertFrom-Json
    if ($restoredSession.current_type -ne 0 -or $restoredSession.current_id -ne $drawnId -or $restoredSession.round_team -ne 2 -or $restoredSession.round_count -ne 15 -or $restoredSession.mode -ne "round" -or $restoredSession.timer_started_at -le 0 -or -not $restoredSession.auto_scoring) {
        throw "Restored session fields mismatch: $($Matches[1])"
    }
    if ($rDisplayRefresh.Content -notmatch "CI Choice Question") {
        throw "Refreshed display page missing drawn problem content"
    }

    $usedAfterRefresh = Get-UsedChoiceCount $problemListPath
    if ($usedAfterRefresh -ne 1) {
        throw "Refresh consumed additional problem! used=$usedAfterRefresh"
    }

    # Reshow previous problem without consuming a new problem
    $rReshow = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&reshow=1" -Method Post -Headers $headers -Body "" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rReshow.StatusCode -ne 200 -or $rReshow.Content -notmatch "data-id=`"$drawnId`"") {
        throw "Reshow failed to return drawn problem $drawnId"
    }
    $usedAfterReshow = Get-UsedChoiceCount $problemListPath
    if ($usedAfterReshow -ne 1) {
        throw "Reshow consumed additional problem! used=$usedAfterReshow"
    }

    $sessionFilePath = Join-Path $staging "data\competitions\$compGuid\session.json"
    if (-not (Test-Path $sessionFilePath)) {
        throw "Expected session.json to exist before pause"
    }

    # Simulate session.json write failure (e.g. read-only / locked file) during draw of Q2:
    # must surface failure visibly, keep previous session intact, and allow same-question retry without consuming a new problem
    $sessionItem = Get-Item $sessionFilePath
    $sessionItem.IsReadOnly = $true
    try {
        $rDrawFail = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&type=0" -Method Post -Headers $headers -Body "state=$encodedState" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
        if ($rDrawFail.Content.Trim() -notmatch '^fail : session_save_failed\|0\|(\d+)$') {
            throw "Expected fail : session_save_failed|0|<id> on read-only session.json, got: $($rDrawFail.Content)"
        }
        $failedQ2Id = [int]$Matches[1]
        if ($failedQ2Id -eq $drawnId) {
            throw "Expected Q2 draw to pick a different problem from Q1 ($drawnId)"
        }
        $sessionAfterFail = Get-Content $sessionFilePath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($sessionAfterFail.current_id -ne $drawnId) {
            throw "session.json should still hold Q1 ($drawnId) after failed Q2 write, got $($sessionAfterFail.current_id)"
        }
        $rReshowWhileFail = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&reshow=1&type=0&id=$failedQ2Id" -Method Post -Headers $headers -Body "state=$encodedState" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
        if ($rReshowWhileFail.Content.Trim() -ne "fail : session_save_failed|0|$failedQ2Id") {
            throw "Expected reshow while read-only to return fail : session_save_failed|0|$failedQ2Id, got: $($rReshowWhileFail.Content)"
        }
    } finally {
        $sessionItem.IsReadOnly = $false
    }

    # Retry same question via reshow=1 after write access is restored; must not consume another problem
    $rReshowRetryQ2 = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&reshow=1&type=0&id=$failedQ2Id" -Method Post -Headers $headers -Body "state=$encodedState" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rReshowRetryQ2.StatusCode -ne 200 -or $rReshowRetryQ2.Content -notmatch "data-id=`"$failedQ2Id`"") {
        throw "Retry reshow failed to return Q2 ($failedQ2Id)"
    }
    $sessionAfterRetry = Get-Content $sessionFilePath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($sessionAfterRetry.current_id -ne $failedQ2Id) {
        throw "Expected session.json current_id=$failedQ2Id after retry, got $($sessionAfterRetry.current_id)"
    }
    $usedAfterQ2Retry = Get-UsedChoiceCount $problemListPath
    if ($usedAfterQ2Retry -ne 2) {
        throw "Expected 2 used choice problems after Q2 retry, got $usedAfterQ2Retry"
    }

    # Pause clears session.json while preserving last_problem_id ($failedQ2Id) for reshow
    $rPause = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/enddisplay?guid=$compGuid&action=0" -Method Post -Headers $headers -Body "" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rPause.Content.Trim() -ne "success" -or (Test-Path $sessionFilePath)) {
        throw "Pause did not succeed or did not clear session.json"
    }

    # Delayed session sync arriving after pause must be rejected and must NOT recreate session.json
    $rDelayedSyncPause = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/session" -Method Post -Headers $headers -Body "guid=$compGuid&timer_action=start&timer_duration=15&timer_mode=0" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rDelayedSyncPause.Content.Trim() -ne "fail : competition isn't ongoing" -or (Test-Path $sessionFilePath)) {
        throw "Delayed sync after pause was not rejected or recreated session.json: $($rDelayedSyncPause.Content)"
    }

    $rDisplayResume = Invoke-WebRequest -Uri "$baseUrl/main/competition/display?guid=$compGuid" -Headers $headers -UseBasicParsing
    if ($rDisplayResume.Content -notmatch 'var has_initial_session = false;' -or $rDisplayResume.Content -notmatch "var last_problem_id = $failedQ2Id;") {
        throw "Resumed display page state unexpected after pause"
    }
    $rReshowAfterPause = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/problem?guid=$compGuid&reshow=1" -Method Post -Headers $headers -Body "" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rReshowAfterPause.Content -notmatch "data-id=`"$failedQ2Id`"") {
        throw "Reshow after pause failed"
    }

    # Concurrent session sync burst racing with finish (action=1) must leave session.json absent
    $raceHandler = New-Object System.Net.Http.HttpClientHandler
    $raceHandler.UseCookies = $false
    $raceClient = New-Object System.Net.Http.HttpClient($raceHandler)
    try {
        $raceClient.DefaultRequestHeaders.Add("Cookie", "token=$token")
        $raceTasks = New-Object System.Collections.Generic.List[System.Threading.Tasks.Task[System.Net.Http.HttpResponseMessage]]
        for ($k = 0; $k -lt 10; $k++) {
            $syncContent = New-Object System.Net.Http.StringContent("guid=$compGuid&timer_action=start&timer_duration=15&timer_mode=0", [System.Text.Encoding]::UTF8, "application/x-www-form-urlencoded")
            $raceTasks.Add($raceClient.PostAsync("$baseUrl/main/competition/display/session", $syncContent))
            if ($k -eq 3) {
                $finContent = New-Object System.Net.Http.StringContent("", [System.Text.Encoding]::UTF8, "application/x-www-form-urlencoded")
                $finishTask = $raceClient.PostAsync("$baseUrl/main/competition/display/enddisplay?guid=$compGuid&action=1", $finContent)
                $raceTasks.Add($finishTask)
            }
        }
        [System.Threading.Tasks.Task]::WaitAll($raceTasks.ToArray())
        $finishBody = $finishTask.Result.Content.ReadAsStringAsync().GetAwaiter().GetResult().Trim()
        if ($finishBody -ne "success") {
            throw "Concurrent finish did not succeed: $finishBody"
        }
    } finally {
        $raceClient.Dispose()
    }

    $compJsonPath = Join-Path $staging "data\competitions\$compGuid\competition.json"
    $compAfterFinish = Get-Content $compJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($compAfterFinish.competition_status -ne 3 -or (Test-Path $sessionFilePath)) {
        throw "Finish did not persist FINISHED status or left session.json behind"
    }
    $rDelayedSyncFinish = Invoke-WebRequest -Uri "$baseUrl/main/competition/display/session" -Method Post -Headers $headers -Body "guid=$compGuid&timer_action=start&timer_duration=15&timer_mode=0" -ContentType "application/x-www-form-urlencoded" -UseBasicParsing
    if ($rDelayedSyncFinish.Content.Trim() -ne "fail : competition isn't ongoing" -or (Test-Path $sessionFilePath)) {
        throw "Delayed sync after finish was not rejected or recreated session.json: $($rDelayedSyncFinish.Content)"
    }

    $finalLog = Get-Content $logPath -Raw -Encoding UTF8
    if ($finalLog -notmatch "Successful login\. New PIN code:") {
        throw "Expected rotated PIN log entry in server.log"
    }

    Write-Host "HTTP smoke test passed on $baseUrl"
}
finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        $proc.WaitForExit(5000) | Out-Null
    }
    Remove-Item -Path $staging -Recurse -Force -ErrorAction SilentlyContinue
}
