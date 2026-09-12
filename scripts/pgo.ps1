# L^ (lhat) -- build the libraries with a profile.
#
# 03 の 6.2改: the profile-guided build is what settles the code layout, and
# without it the layout lottery moves a measurement further than the change
# being measured. So the shipping libraries are built this way, and the
# dispatch the core takes under Clang (5.2改) is only worth its numbers here.
#
# Three steps over one tree per runtime, the shape scripts/pgo.ps1 takes in
# the core: GENERATE instruments, a training run writes one raw profile per
# process, llvm-profdata merges them, and the tree is configured again with
# USE and rebuilt. What differs is the training: the library runs inside
# Godot, so the run is a Godot run.
#
#     . .\scripts\devshell.ps1
#     .\scripts\pgo.ps1 -Godot D:\path\to\Godot_console.exe
#     .\scripts\pgo.ps1 -Godot ... -Targets editor
#
# The optimised libraries land in demo/bin, where every other build puts
# them. The trees are build/pgo-<target> and the profiles build/pgo/<target>.
#
# What each runtime is trained on is what it will be asked to do:
#
#   editor                 the editor reading all three demo projects, the two
#                          game demos running, and demo/bench.gd's loops
#   template_debug         a Text export, whose game loads the library with
#   template_release       the front end under the lhat_text row
#   template_*-vmonly      a Compiled export, whose game loads the VM-only
#                          library under the plain row
#
# An export runs the editor, so demo/bin has to hold an editor library before
# a template target is trained -- `cmake --build --preset release` first, or
# let this script do the editor target before the others, which is the order
# it takes by default.

[CmdletBinding()]
param(
    [string[]]$Targets = @("editor", "template_debug", "template_release",
                           "template_debug-vmonly", "template_release-vmonly"),
    [string]$Godot = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not $env:VCToolsInstallDir) {
    throw "MSVC environment is not loaded; dot-source scripts/devshell.ps1 first."
}

if ($Godot -eq "") { $Godot = $env:GODOT }
if ($Godot -eq "") {
    $found = Get-Command godot, Godot_console.exe -CommandType Application `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $Godot = $found.Source }
}
if ($Godot -eq "") { throw "no Godot -- pass -Godot, or set GODOT" }
if (-not (Test-Path -LiteralPath $Godot)) { throw "no Godot at $Godot" }

$clang = (Get-Command clang-cl -ErrorAction SilentlyContinue).Source
if (-not $clang) { throw "no clang-cl on the PATH" }
$profdata = Join-Path (Split-Path -Parent $clang) "llvm-profdata.exe"
if (-not (Test-Path $profdata)) { throw "no llvm-profdata beside $clang" }

function Invoke-Program([string]$program, [string]$what,
                        [string[]]$programArguments) {
    $output = @(& $program @programArguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $output | Write-Host
        throw "$what failed with Godot exit code $LASTEXITCODE"
    }
}

function Invoke-Godot([string]$what, [string[]]$godotArguments) {
    Invoke-Program $Godot $what $godotArguments
}

# A training project is a copy: the run writes .godot/ and, for a template,
# an export beside it, and none of that belongs in the tree.
function Copy-Project([string]$from, [string]$to) {
    if (Test-Path $to) { Remove-Item -Recurse -Force -LiteralPath $to }
    Copy-Item -Recurse -LiteralPath (Join-Path $root $from) $to
    $bin = Join-Path $to "bin"
    if (-not (Test-Path $bin)) { New-Item -ItemType Directory -Force -Path $bin | Out-Null }
    Get-ChildItem $bin -Filter *.dll -ErrorAction SilentlyContinue | Remove-Item -Force
    Copy-Item (Join-Path $root "demo\bin\*.dll") $bin -Force
    Copy-Item (Join-Path $root "demo\lhat.gdextension") $to -Force
    # The editor writes .godot/, and a game run without one attaches no
    # scripts at all while still loading the library -- it would train the
    # boundary and nothing behind it.
    Invoke-Godot "importing $from" @(
        "--headless", "--editor", "--path", $to, "--quit-after", "200")
}

function Train-Editor([string]$work) {
    foreach ($name in "demo", "dodge_the_creeps", "squash_the_creeps") {
        $from = if ($name -eq "demo") { "demo" } else { "demo-projects\$name" }
        $copy = Join-Path $work $name
        Write-Host "  project: $name"
        Copy-Project $from $copy
        # demo has no main scene. bench.gd below exercises it instead; a
        # normal project run would just exit and leave an empty profile.
        if ($name -ne "demo") {
            Invoke-Godot "training $name" @(
                "--headless", "--path", $copy, "--quit-after", "400")
        }
    }
    # 03 の 5.2改's loops, which is what the dispatch decides. The bench is
    # the one workload here that spends its time inside the interpreter
    # rather than at the boundary.
    Write-Host "  bench"
    Invoke-Godot "training bench" @(
        "--headless", "--path", (Join-Path $work "demo"), "--script",
        "res://bench.gd", "--quit-after", "600")
}

function Train-Export([string]$work, [string]$level, [int]$mode) {
    foreach ($name in "dodge_the_creeps", "squash_the_creeps") {
        $copy = Join-Path $work $name
        Write-Host "  project: $name ($(if ($mode -eq 1) { 'Compiled' } else { 'Text' }), $level)"
        Copy-Project "demo-projects\$name" $copy
        # The preset says which library the exported game will load: Text
        # ships the one with the front end and raises lhat_text, Compiled
        # ships the VM-only one alone (src/lhat_export.cpp).
        $presets = Join-Path $copy "export_presets.cfg"
        $text = (Get-Content $presets -Raw) -replace 'lhat/script_export_mode=\d', ''
        $text = $text -replace '(?m)^\[preset\.0\.options\]', "[preset.0.options]`n`nlhat/script_export_mode=$mode"
        Set-Content $presets $text
        $out = Join-Path $copy "export"
        New-Item -ItemType Directory -Force -Path $out | Out-Null
        Invoke-Godot "exporting $name for $level" @(
            "--headless", "--path", $copy, "--export-$level", "Windows Desktop",
            (Join-Path $out "game.exe"))
        $console = Join-Path $out "game.console.exe"
        if (-not (Test-Path $console)) {
            # A release export has no console wrapper of its own; the
            # template beside it launches the exe of its own basename.
            $templates = Join-Path $env:APPDATA "Godot\export_templates\4.7.1.stable"
            Copy-Item (Join-Path $templates "windows_${level}_x86_64_console.exe") $console -Force
        }
        Push-Location $out
        try {
            Invoke-Program ".\game.console.exe" "training exported $name for $level" @(
                "--headless", "--quit-after", "400")
        }
        finally { Pop-Location }
    }
}

foreach ($target in $Targets) {
    $godot_target = $target -replace "-vmonly$", ""
    $vm_only = if ($target -match "-vmonly$") { "ON" } else { "OFF" }
    $tree = Join-Path $root "build\pgo-$target"
    $prof = Join-Path $root "build\pgo\$target"
    $merged = Join-Path $root "build\pgo\$target.profdata"
    $work = Join-Path $root "build\pgo\train-$target"

    Write-Host "=== ${target}: instrumenting ==="
    # From scratch: the profile has to come from this build, and a tree
    # configured for one phase carries flags into the other.
    if (Test-Path $tree) { Remove-Item -Recurse -Force -LiteralPath $tree }
    if (Test-Path $prof) { Remove-Item -Recurse -Force -LiteralPath $prof }
    if (Test-Path $merged) { Remove-Item -Force -LiteralPath $merged }
    New-Item -ItemType Directory -Force -Path $prof | Out-Null
    cmake -S $root -B $tree -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
        "-DGODOTCPP_TARGET=$godot_target" "-DLHAT_VM_ONLY=$vm_only" `
        -DLHAT_GD_PGO=GENERATE | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$target : configure failed" }
    cmake --build $tree | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$target : instrumented build failed" }

    Write-Host "=== ${target}: training ==="
    # One raw profile per process; %p keeps a run from overwriting the last.
    $env:LLVM_PROFILE_FILE = Join-Path $prof "lhat-%p.profraw"
    try {
        if (Test-Path $work) { Remove-Item -Recurse -Force -LiteralPath $work }
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        switch -Regex ($target) {
            "^editor$"                  { Train-Editor $work }
            "^template_debug$"          { Train-Export $work "debug" 0 }
            "^template_release$"        { Train-Export $work "release" 0 }
            "^template_debug-vmonly$"   { Train-Export $work "debug" 1 }
            "^template_release-vmonly$" { Train-Export $work "release" 1 }
            default { throw "no training written for $target" }
        }
    }
    finally { Remove-Item Env:\LLVM_PROFILE_FILE }

    $raw = @(Get-ChildItem "$prof\*.profraw" -ErrorAction SilentlyContinue)
    if ($raw.Count -eq 0) { throw "$target : the training wrote no profile" }
    $empty = @($raw | Where-Object Length -eq 0)
    if ($empty.Count -ne 0) {
        throw "$target : the training left empty profiles: $($empty.Name -join ', ')"
    }
    Write-Host "=== ${target}: merging $($raw.Count) profiles ==="
    & $profdata merge -output="$merged" @($raw.FullName)
    if ($LASTEXITCODE -ne 0) { throw "$target : llvm-profdata merge failed" }

    Write-Host "=== ${target}: building with the profile ==="
    cmake -S $root -B $tree -DLHAT_GD_PGO=USE `
        "-DLHAT_GD_PGO_PROFILE=$($merged -replace '\\', '/')" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$target : reconfigure failed" }
    cmake --build $tree | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$target : optimised build failed" }
}

Write-Host ""
Write-Host "Built with a profile:"
Get-ChildItem (Join-Path $root "demo\bin\*.dll") |
    ForEach-Object { "  {0,10:N0}  {1}" -f $_.Length, $_.Name }
