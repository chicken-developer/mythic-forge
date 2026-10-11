param(
    [ValidateSet('Validate','Generate','Verify','Capture','Build','Playtest','Audit','Calibrate','CalibrateCapture')][string]$Mode='Validate',
    [string]$Engine='C:/Program Files/Epic Games/UE_5.8',
    [switch]$SkipBuild,
    [switch]$DetailedViews
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$projectFile=Join-Path $projectRoot 'TDKKingdomMap.uproject'
$python=Join-Path $Engine 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
$editor=Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
Push-Location $projectRoot
try {
    if ($Mode -in @('Validate','Generate')) {
        & $python (Join-Path $PSScriptRoot 'prepare.py')
        if ($LASTEXITCODE -ne 0) { throw 'Design validation/compilation failed' }
        & (Join-Path $PSScriptRoot 'render_preview.ps1')
    }
    if (!$SkipBuild -and $Mode -in @('Build','Generate','Validate','Playtest')) {
        $env:DOTNET_ROOT=Join-Path $Engine 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64'
        $env:DOTNET_ROLL_FORWARD='LatestMajor'
        & (Join-Path $env:DOTNET_ROOT 'dotnet.exe') (Join-Path $Engine 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll') TDKKingdomMapEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding "-Log=$projectRoot/Saved/Logs/ThanhA_Build.log"
        if ($LASTEXITCODE -ne 0) { throw 'Unreal C++ build failed' }
    }
    if ($Mode -eq 'Build') { return }
    if ($Mode -eq 'Playtest') {
        & $editor $projectFile '/Game/Maps/Blockout/L_ThanhA_Blockout' -game -ThanhAAutoWalk -nullrhi -unattended -nosound '-Log=ThanhA_Playtest.log'
    } elseif ($Mode -in @('Capture','CalibrateCapture')) {
        $unusedShaderThreads=[Math]::Max(0,[Environment]::ProcessorCount-16)
        $captureArgs=@(); if ($DetailedViews) { $captureArgs+='-details' }
        & $editor $projectFile -run=ThanhABlockout "-mode=$($Mode.ToLowerInvariant())" @captureArgs -AllowCommandletRendering -d3d12 -unattended -nosound "-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=$unusedShaderThreads" "-Log=ThanhA_$Mode.log"
    } else {
        & $editor $projectFile -run=ThanhABlockout "-mode=$($Mode.ToLowerInvariant())" -nullrhi -unattended -nosound "-Log=ThanhA_$Mode.log"
    }
    if ($LASTEXITCODE -ne 0) { throw "Unreal $Mode returned exit code $LASTEXITCODE; inspect Saved/Logs and Generated reports" }
} finally { Pop-Location }
