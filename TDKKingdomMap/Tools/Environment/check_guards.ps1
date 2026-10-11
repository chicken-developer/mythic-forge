param([string]$Engine='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $projectRoot
try {
    $mapFile=Join-Path $projectRoot 'Content/Maps/Blockout/L_ThanhA_Blockout.umap'
    $receiptFile=Join-Path $projectRoot 'Design/Environment/Generated/saved_state.json'
    $before=(Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash
    $originalBytes=[IO.File]::ReadAllBytes($receiptFile)
    try {
        # Inject a stale receipt, never edit the user's map to test the rejection.
        $stale=Get-Content -LiteralPath $receiptFile -Raw | ConvertFrom-Json
        $stale.map_sha256='intentional-stale-receipt-for-guard-test'
        [IO.File]::WriteAllText($receiptFile,($stale | ConvertTo-Json -Depth 100),[Text.UTF8Encoding]::new($false))
        $ErrorActionPreference='Continue' # A nonzero exit/stderr is the expected outcome here.
        & powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Generate -SkipBuild *> Saved/Logs/environment_stale_guard_console.txt
        $staleExit=$LASTEXITCODE
    } finally {$ErrorActionPreference='Stop'; [IO.File]::WriteAllBytes($receiptFile,$originalBytes)}
    $editor=Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $ErrorActionPreference='Continue'
    & $editor (Join-Path $projectRoot 'TDKKingdomMap.uproject') -run=ThanhABlockout -mode=generate -nullrhi -unattended -nosound '-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=112' '-Log=ThanhA_Environment_LegacyGuard.log' *> Saved/Logs/environment_legacy_guard_console.txt
    $legacyExit=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    $after=(Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash
    $staleReason=[bool](Select-String -LiteralPath Saved/Logs/environment_stale_guard_console.txt -Pattern 'Saved map has changed since' -Quiet)
    $legacyReason=[bool](Select-String -LiteralPath Saved/Logs/ThanhA_Environment_LegacyGuard.log -Pattern 'Blockout Generate is disabled' -Quiet)
    $pass=$staleExit -ne 0 -and $legacyExit -ne 0 -and $staleReason -and $legacyReason -and $before -eq $after
    $report=[ordered]@{status=$(if($pass){'pass'}else{'fail'});execution='Actual UE process rejection; temporarily injected stale saved-map receipt restored byte-for-byte; primary map never edited';stale_receipt_exit=$staleExit;legacy_generator_exit=$legacyExit;stale_receipt_reason_verified=$staleReason;legacy_reason_verified=$legacyReason;map_sha256_before=$before;map_sha256_after=$after;map_file_unchanged=($before -eq $after)}
    [IO.File]::WriteAllText((Join-Path $projectRoot 'Design/Environment/Generated/guard_validation.json'),($report | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    if (!$pass) {throw 'Generator guard validation failed'}
    Write-Output ($report | ConvertTo-Json -Compress)
} finally {Pop-Location}
