# L^ (lhat) -- point .github/lhat-ref at a commit of the core.
#
#     .\scripts\bump-lhat-ref.ps1            # the head of the core's origin/main
#     .\scripts\bump-lhat-ref.ps1 <commit>   # that commit
#
# It writes the file and commits nothing: what the message of that commit has
# to say is what was checked against the new core, and only a person who ran
# the checks knows that.
#
# CI checks the core out from GitHub, so a commit that was never pushed is one
# it cannot find. Anything not on origin/main is refused here rather than found
# red later, which also keeps a release off a branch the core may yet drop. A
# desk on another commit is only warned about, since a bump may come before the
# build -- but what was tried there is then not what CI will build.

param(
    [string]$Commit = "origin/main"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$core = if ($env:LHAT_CORE_DIR) { $env:LHAT_CORE_DIR } else { Join-Path $root "..\lhat" }
$file = Join-Path $root ".github\lhat-ref"

git -C $core rev-parse --git-dir 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) { throw "no L^ core at $core -- set LHAT_CORE_DIR" }

git -C $core fetch -q origin
if ($LASTEXITCODE -ne 0) { throw "could not fetch the core's origin" }

$sha = git -C $core rev-parse --verify -q "$Commit^{commit}"
if ($LASTEXITCODE -ne 0) { throw "the core has no commit named $Commit" }
$short = git -C $core rev-parse --short $sha

git -C $core merge-base --is-ancestor $sha origin/main
if ($LASTEXITCODE -ne 0) {
    throw "$short is not on the core's origin/main -- push or merge it first"
}

# The desk: which commit it is on, and what it holds beyond that commit.
$here = git -C $core rev-parse HEAD
if ($here -ne $sha) {
    Write-Warning "the core here is on $(git -C $core rev-parse --short $here), not $short"
}
$changed = git -C $core status --porcelain --untracked-files=no
if ($changed) {
    Write-Warning "the core here has uncommitted changes CI will not see:"
    $changed | ForEach-Object { Write-Warning $_ }
}

$was = (Get-Content -LiteralPath $file -Raw).Trim()
if ($was -eq $sha) {
    "lhat-ref is already $short"
    exit 0
}

# One LF and no BOM, whatever this PowerShell would write by default.
[System.IO.File]::WriteAllText($file, "$sha`n")
"lhat-ref: $(git -C $core rev-parse --short $was) -> $short"
git -C $core log --oneline "$was..$sha"
