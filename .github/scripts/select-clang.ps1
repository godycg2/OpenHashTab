# Picks a Clang that the MSVC STL installed in the runner image accepts, and puts it first on PATH.
#
# Why this exists: the MSVC STL that ships with the Visual Studio on the image rejects any compiler older than the
# version it was built for (STL1000 in yvals_core.h), while the standalone LLVM the image ships can be older than
# that. build_all.ps1 takes CC, CXX and RC from the directory `clang` resolves to, so fixing PATH is enough.
#
# Selection order:
#   1. the Clang bundled with Visual Studio, which is always in sync with its own STL
#   2. the standalone LLVM, if it happens to be new enough
#   3. the newest LLVM from winget

$ErrorActionPreference = 'Stop'

# STL1000 in yvals_core.h of MSVC 14.52 asks for Clang 22 or newer
$minimum = 22

$candidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\Llvm\x64\bin',
    'C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Tools\Llvm\x64\bin',
    'C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin',
    'C:\Program Files\LLVM\bin'
)

function Get-ClangMajor([string] $directory) {
    $exe = Join-Path $directory 'clang-cl.exe'
    if (-not (Test-Path $exe)) {
        return $null
    }

    # clang-cl prints "clang version 20.1.8" as the first line
    $first = (& $exe --version 2>&1 | Select-Object -First 1)
    if ("$first" -match 'clang version (\d+)') {
        return [int] $Matches[1]
    }

    return $null
}

$selected = $null
foreach ($directory in $candidates) {
    $major = Get-ClangMajor $directory
    if ($null -eq $major) {
        continue
    }

    Write-Host "$directory ships Clang $major"
    if (-not $selected -and $major -ge $minimum) {
        $selected = $directory
    }
}

if (-not $selected) {
    Write-Host "No Clang $minimum or newer on this image, installing the latest LLVM"
    winget install --source winget --exact --id LLVM.LLVM --force --silent --disable-interactivity `
        --accept-source-agreements --accept-package-agreements

    if ((Get-ClangMajor 'C:\Program Files\LLVM\bin') -ge $minimum) {
        $selected = 'C:\Program Files\LLVM\bin'
    }
}

if (-not $selected) {
    throw "No Clang $minimum or newer available, which the MSVC STL in this image requires. Check what winget reports above."
}

# The runner prepends the contents of GITHUB_PATH to PATH for the following steps
$selected | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append

Write-Host "Using Clang from $selected"
& (Join-Path $selected 'clang.exe') --version | Select-Object -First 1 | Write-Host

# winget reports "already installed" as a non-zero exit code, which must not fail the step once a usable Clang is found
$global:LASTEXITCODE = 0
