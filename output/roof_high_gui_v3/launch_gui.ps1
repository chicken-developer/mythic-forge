$ErrorActionPreference = 'Stop'
$reviewRoot = $PSScriptRoot
$env:HFS = 'E:/SideFX/Houdini 22.0.368'
$env:SHFS = $env:HFS + '/shfs'
$env:HB = $env:HFS + '/bin'
$env:HH = $env:HFS + '/houdini'
$env:PATH = $env:HB + ';' + $env:PATH
$env:HOUDINI_USER_PREF_DIR = $reviewRoot + '/gui_prefs/houdini__HVER__'
$env:TDK_REVIEW_ROOT = $reviewRoot
$env:HOUDINI_MAXTHREADS = '8'
$gui = Start-Process -FilePath ($env:HB + '/houdini.exe') -WorkingDirectory $env:HB -ArgumentList @('-geometry=1600x1000+50+20','waitforui',($reviewRoot + '/gui_bootstrap.py')) -RedirectStandardOutput ($reviewRoot + '/results/gui_stdout.log') -RedirectStandardError ($reviewRoot + '/results/gui_stderr.log') -PassThru
$gui.Id | Set-Content -LiteralPath ($reviewRoot + '/results/gui_pid.txt')
$gui.WaitForExit()
