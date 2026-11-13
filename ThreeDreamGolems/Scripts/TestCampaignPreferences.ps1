param(
	[string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
	[ValidatePattern('^[A-Za-z0-9_-]+$')]
	[string]$EvidenceName = 'cold-01'
)

$ErrorActionPreference = 'Stop'
$TaskRoot = Split-Path -Parent $PSScriptRoot
$TaskDirectory = Join-Path $TaskRoot ('Saved\Evidence\M7\Settings\' + $EvidenceName)
New-Item -ItemType Directory -Path $TaskDirectory -Force | Out-Null
$TaskFixture = Join-Path $TaskDirectory 'UserPreferences.ini'
$TaskExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$TaskResults = @()
try
{
	foreach ($TaskOperation in @('Write', 'Verify', 'ReadOnly', 'Reset', 'VerifyReset'))
	{
		if ($TaskOperation -eq 'ReadOnly')
		{
			Set-ItemProperty -LiteralPath $TaskFixture -Name IsReadOnly -Value $true
		}
		elseif (Test-Path -LiteralPath $TaskFixture)
		{
			Set-ItemProperty -LiteralPath $TaskFixture -Name IsReadOnly -Value $false
		}
		$TaskLog = Join-Path $TaskDirectory ($TaskOperation + '.log')
		$TaskArguments = @((Join-Path $TaskRoot 'ThreeDreamGolems.uproject'), '-run=GolemCampaignPreferences',
			('-Operation=' + $TaskOperation), ('-SettingsFixture=' + $TaskFixture), ('-GameUserSettingsINI=' + $TaskFixture), '-unattended', '-nop4', '-NullRHI',
			'-nosplash', '-DisablePlugins=RiderLink', '-DDC=InstalledNoZenLocalFallback', ('-abslog=' + $TaskLog))
		$TaskStarted = (Get-Date).ToUniversalTime().ToString('o')
		& $TaskExecutable @TaskArguments *> ($TaskLog + '.stdout')
		$TaskExit = $LASTEXITCODE
		$TaskResults += [ordered]@{
			operation = $TaskOperation
			startedUtc = $TaskStarted
			finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
			executable = $TaskExecutable
			arguments = $TaskArguments
			exitCode = $TaskExit
			log = $TaskLog
			result = $(if ($TaskExit -eq 0) { 'PASS' } else { 'FAIL' })
		}
		Write-Output ($TaskOperation + ': ' + $TaskExit)
		if ($TaskExit -ne 0)
		{
			break
		}
	}
}
finally
{
	if (Test-Path -LiteralPath $TaskFixture)
	{
		Set-ItemProperty -LiteralPath $TaskFixture -Name IsReadOnly -Value $false
	}
	$TaskResults | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $TaskDirectory 'results.json')
}
if ($TaskResults.Count -ne 5 -or @($TaskResults | Where-Object { $_.exitCode -ne 0 }).Count -ne 0)
{
	exit 1
}
