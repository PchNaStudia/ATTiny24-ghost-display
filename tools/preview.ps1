$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root '.pio\preview'
New-Item -ItemType Directory -Force $output | Out-Null
& g++ -std=c++17 -Wall -Wextra -Werror "-I$root\include" "$root\src\main.cpp" "$root\tools\HalHost.cpp" "$root\src\DisplayController.cpp" -o "$output\preview_host.exe"
if ($LASTEXITCODE -ne 0) { throw 'Preview compilation failed. Ensure g++ is on PATH.' }
if ($args -contains '-BuildOnly') { exit 0 }
if ($args -notcontains '-NoOpen') {
    Start-Process powershell -ArgumentList '-NoProfile', '-Command', 'Start-Sleep -Seconds 2; Start-Process http://127.0.0.1:8080'
}
& node "$root\tools\preview_server.js"