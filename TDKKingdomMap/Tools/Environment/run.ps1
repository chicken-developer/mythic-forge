param(
    [ValidateSet('Audit','FoliageAudit','Prepare','Trial','TrialCapture','SampleCapture','Generate','Verify','Capture','Benchmark','Build')][string]$Mode='Verify',
    [string]$Engine='C:/Program Files/Epic Games/UE_5.8',
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$python=Join-Path $Engine 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
$editor=Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$projectFile=Join-Path $projectRoot 'TDKKingdomMap.uproject'
$generated=Join-Path $projectRoot 'Design/Environment/Generated'
Push-Location $projectRoot
try {
    if (!$SkipBuild -or $Mode -eq 'Build') {
        & powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Build -Engine $Engine
        if ($LASTEXITCODE -ne 0) {throw 'Unreal build failed'}
    }
    if ($Mode -eq 'Build') {return}
    if ($Mode -eq 'Benchmark') {
        $performance=Join-Path $generated 'performance.json'
        if (Test-Path $performance) {Remove-Item -LiteralPath $performance}
        & $editor $projectFile '/Game/Maps/Blockout/L_ThanhA_Blockout' -game -ThanhAEnvironmentBenchmark -RenderOffScreen -d3d12 -ResX=1600 -ResY=1200 -ForceRes -windowed -unattended -nosound -NoSplash -NoVSync '-ExecCmds=sg.ViewDistanceQuality 3,sg.AntiAliasingQuality 3,sg.ShadowQuality 3,sg.GlobalIlluminationQuality 3,sg.ReflectionQuality 3,sg.PostProcessQuality 3,sg.TextureQuality 3,sg.EffectsQuality 3,sg.FoliageQuality 3,sg.ShadingQuality 3,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0' '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=112' '-Log=ThanhA_Environment_Benchmark.log'
        if ($LASTEXITCODE -ne 0 -or !(Test-Path $performance)) {throw 'Actual game benchmark failed; no FPS result is available.'}
        return
    }
    if ($Mode -eq 'Audit' -and (Test-Path (Join-Path $generated 'execution.json'))) {throw 'Baseline already exists for this environment. Preserve it; use the existing audit/manifest.'}
    if ($Mode -in @('Generate','Capture') -and (Test-Path (Join-Path $generated 'saved_state.json'))) {
        & $editor $projectFile -run=pythonscript "-script=$projectRoot/Tools/Environment/inspect_saved.py" -EnvironmentGuard -nullrhi -unattended -nosound '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=112' '-Log=ThanhA_Environment_ManualGuard.log'
        if ($LASTEXITCODE -ne 0) {throw 'Saved vegetation has manual edits; regeneration stopped before replacing it.'}
    }
    if ($Mode -in @('Prepare','Trial','Generate')) {
        & $python Tools/Environment/prepare.py
        if ($LASTEXITCODE -ne 0) {throw 'Environment inputs or masks failed validation'}
        if ($Mode -eq 'Prepare') {return}
    }
    if ($Mode -eq 'Generate') {
        $compiled=Get-Content (Join-Path $generated 'environment_compiled.json') -Raw | ConvertFrom-Json
        $trial=Get-Content (Join-Path $generated 'trial_execution.json') -Raw | ConvertFrom-Json
        $review=Get-Content (Join-Path $generated 'trial_review.json') -Raw | ConvertFrom-Json
        if ($trial.status -ne 'pass' -or $review.fingerprint -ne $compiled.fingerprint -or !$review.visually_reviewed) {throw 'Render and inspect the current Trial before Generate; store its fingerprint in trial_review.json.'}
    }
    $modes=@{Audit='environmentaudit';FoliageAudit='samplefoliageaudit';Trial='environmenttrial';TrialCapture='environmenttrialcapture';SampleCapture='environmentsamplecapture';Generate='environmentgenerate';Verify='environmentverify';Capture='environmentcapture'}
    $argsForEditor=@($projectFile,'-run=ThanhABlockout',"-mode=$($modes[$Mode])",'-unattended','-nosound',"-Log=ThanhA_Environment_$Mode.log",'-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=112')
    if ($Mode -eq 'Capture') {$argsForEditor+='-EnvironmentViews'}
    if ($Mode -in @('TrialCapture','SampleCapture','Capture')) {$argsForEditor+=@('-AllowCommandletRendering','-d3d12')} else {$argsForEditor+='-nullrhi'}
    & $editor @argsForEditor
    if ($LASTEXITCODE -ne 0) {throw "Unreal environment $Mode failed; inspect Saved/Logs and Generated reports"}
    if ($Mode -in @('Generate','Verify','Trial')) {
        $verifyArgs=@('Tools/Environment/verify.py'); if ($Mode -eq 'Trial') {$verifyArgs+='--trial'}
        & $python @verifyArgs
        if ($LASTEXITCODE -ne 0) {throw 'Blockout/package preservation verification failed'}
    }
    if ($Mode -in @('Generate','Capture')) {
        & $editor $projectFile -run=pythonscript "-script=$projectRoot/Tools/Environment/inspect_saved.py" -nullrhi -unattended -nosound '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=112' '-Log=ThanhA_Environment_State.log'
        if ($LASTEXITCODE -ne 0) {throw 'Saved environment instance validation failed'}
    }
} finally {Pop-Location}
