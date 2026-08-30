# L^ (lhat) -- write lhat-host.json for the language server.
#
# 05 の 8.7: the server has to be told what the checker was told, and in this
# workspace that is what the extension registers -- `godot`, and the handful of
# names bound beside it. Only the engine can say: the registrations happen in a
# library the engine loads, so this takes a built demo/bin and a Godot to load
# it with. The extension links no stdlib, and neither does the file.
#
#     .\scripts\dump-host-api.ps1
#     .\scripts\dump-host-api.ps1 -Godot D:\path\to\Godot_console.exe
#
# Godot is taken from -Godot, then $env:GODOT, then PATH. The file lands at the
# workspace root, which is where the server looks for one (lsp/workspace.c),
# and is not tracked: it says what src/ registers, so it goes stale as soon as
# src/ changes -- generate it again rather than keep it.

[CmdletBinding()]
param(
    [string]$Godot = "",
    [string]$Out = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Out -eq "") { $Out = Join-Path $root "lhat-host.json" }

if ($Godot -eq "") { $Godot = $env:GODOT }
if ($Godot -eq "") {
    $found = Get-Command godot, Godot_console.exe -CommandType Application `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $Godot = $found.Source }
}
if ($Godot -eq "") { throw "no Godot -- pass -Godot, or set GODOT" }
if (-not (Test-Path -LiteralPath $Godot)) { throw "no Godot at $Godot" }

# Without the library the project still opens, and the script then fails on a
# LhatRuntime that is not there. Saying so here says which of the two is wrong.
$demo = Join-Path $root "demo"
$built = Get-ChildItem -Path (Join-Path $demo "bin") -Filter "liblhat.*" `
    -ErrorAction SilentlyContinue
if (-not $built) {
    throw "no extension in demo/bin -- cmake --build --preset release first"
}

# An absolute path, since the file belongs at the workspace root rather than in
# the demo project; FileAccess takes one, so nothing has to go through user://.
$abs = [System.IO.Path]::GetFullPath($Out).Replace('\', '/')
& $Godot --headless --path $demo --script dump_host_api.gd -- $abs | Out-Null
if ($LASTEXITCODE -ne 0) { throw "the extension would not dump its registrations" }
if (-not (Test-Path -LiteralPath $abs)) { throw "the extension wrote nothing to $abs" }

$dumped = Get-Content -LiteralPath $abs -Raw -Encoding utf8 | ConvertFrom-Json
"$abs : $($dumped.types.Count) types, $($dumped.functions.Count) functions, $($dumped.annotations.Count) annotations, $($dumped.bindings.Count) bindings"
