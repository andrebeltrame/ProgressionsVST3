# Progressions - installs the plug-in on Windows and says what it found.
#
# Every way this fails on Windows fails in silence: a plug-in still carrying
# the downloaded-file mark is refused without a word, a copy into the wrong
# folder is never scanned, and a plug-in Live rejected once is not looked at
# again on an ordinary rescan. So this does the install and then prints what
# is actually on disk, which is the part that has been missing.
#
# Put this file next to Progressions.vst3 and right-click it ->
# "Run with PowerShell".

$ErrorActionPreference = 'Stop'

function Say($text)  { Write-Host $text }
function Good($text) { Write-Host "  OK    $text" -ForegroundColor Green }
function Bad($text)  { Write-Host "  BAD   $text" -ForegroundColor Red }

Say ""
Say "Progressions - Windows install"
Say "=============================="
Say ""

# ---- Find the plug-in sitting next to this script --------------------------
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'Progressions.vst3'

if (-not (Test-Path $source)) {
    Bad "No Progressions.vst3 next to this script."
    Say ""
    Say "  This script has to live in the same folder as the plug-in."
    Say "  Folder searched: $here"
    Say ""
    Read-Host "Press Enter to close"
    exit 1
}

$isBundle = (Get-Item $source) -is [System.IO.DirectoryInfo]
Good ("Found the plug-in as a " + $(if ($isBundle) { "folder (bundle)" } else { "single file" }))

# The binary that Windows actually loads, whichever layout this is.
$binary = if ($isBundle) { Join-Path $source 'Contents\x86_64-win\Progressions.vst3' } else { $source }

if (-not (Test-Path $binary)) {
    Bad "The bundle has no Contents\x86_64-win\Progressions.vst3 inside it."
    Say "  This copy is incomplete - ask for the file again."
    Say ""
    Read-Host "Press Enter to close"
    exit 1
}

# A Windows DLL starts with "MZ". Anything else means the wrong file travelled -
# a macOS binary, or something a transfer mangled on the way here.
$head = [System.IO.File]::ReadAllBytes($binary)[0..1]
if ($head[0] -eq 0x4D -and $head[1] -eq 0x5A) {
    Good ("It is a Windows binary, " + [math]::Round((Get-Item $binary).Length / 1MB, 1) + " MB")
} else {
    Bad "This is not a Windows binary - the wrong file was sent, or it arrived damaged."
    Say ""
    Read-Host "Press Enter to close"
    exit 1
}

# ---- Administrator, because the VST3 folder is under Program Files ---------
$admin = ([Security.Principal.WindowsPrincipal] `
          [Security.Principal.WindowsIdentity]::GetCurrent()
         ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $admin) {
    Say ""
    Say "  Needs administrator rights to write into Program Files."
    Say "  Asking Windows to run this again, elevated..."
    Start-Process powershell -Verb RunAs -ArgumentList `
        "-ExecutionPolicy Bypass -File `"$($MyInvocation.MyCommand.Path)`""
    exit 0
}

# ---- Install ---------------------------------------------------------------
$vst3 = Join-Path $env:CommonProgramFiles 'VST3'
$vendor = Join-Path $vst3 'Nowhr Dynamics'
$target = Join-Path $vendor 'Progressions.vst3'

New-Item -ItemType Directory -Force -Path $vendor | Out-Null

# Both layouts install to the same name, so an old one has to go first or the
# two get mixed together and neither loads.
if (Test-Path $target) {
    Remove-Item -Recurse -Force $target
    Good "Removed the copy that was already there"
}

Copy-Item -Recurse -Force $source $target
Good "Copied to $target"

# ---- Unblock ---------------------------------------------------------------
# Windows marks everything that came from the internet, the mark survives the
# copy, and a marked plug-in is refused with nothing said anywhere.
$blocked = @(Get-ChildItem -Recurse -Force $target -ErrorAction SilentlyContinue) + @(Get-Item $target)
$marks = 0
foreach ($item in $blocked) {
    if (Get-Item $item.FullName -Stream Zone.Identifier -ErrorAction SilentlyContinue) {
        Unblock-File -Path $item.FullName -ErrorAction SilentlyContinue
        $marks++
    }
}
if ($marks -gt 0) {
    Good "Cleared the downloaded-file mark from $marks file(s) - this alone can be the whole problem"
} else {
    Good "Nothing was marked as downloaded"
}

# ---- Report ----------------------------------------------------------------
Say ""
Say "Installed. What is on disk now:"
Say ""
Get-ChildItem -Recurse $target | Select-Object -ExpandProperty FullName | ForEach-Object { Say "  $_" }

Say ""
Say "In Ableton Live, in this order:"
Say ""
Say "  1. Preferences -> Plug-Ins: 'Use VST3 Plug-In System Folders' must be On."
Say "     If a custom VST3 folder is set there, say what it is - the plug-in"
Say "     has to go in that one instead."
Say "  2. Hold ALT and click Rescan. Holding Alt is what makes Live look again"
Say "     at a plug-in it has already refused once; an ordinary rescan will not."
Say "  3. Look under INSTRUMENTS, not audio effects. Progressions is an"
Say "     instrument."
Say ""
Say "Still not listed after all three? Send back everything printed above."
Say ""
Read-Host "Press Enter to close"
