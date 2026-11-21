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
