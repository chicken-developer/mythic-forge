param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('BuildEditor', 'Bootstrap', 'Test', 'BootSmoke', 'CookBoot')]
	[string]$Action,
	[string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
	[string]$EvidenceName = '',
	[ValidateRange(0, 9)]
	[int]$Milestone = 0,
	[ValidatePattern('^[A-Za-z0-9_.]+$')]
	[string]$TestFilter = 'Knight.'
)

$ErrorActionPreference = 'Stop'
$TaskProjectRoot = Split-Path -Parent $PSScriptRoot
$TaskProject = Join-Path $TaskProjectRoot 'ThreeDreamingKnights.uproject'
$TaskEvidence = Join-Path $TaskProjectRoot 'Evidence\ContinuousRun'
New-Item -ItemType Directory -Path $TaskEvidence -Force | Out-Null
if ([string]::IsNullOrWhiteSpace($EvidenceName))
{
	$EvidenceName = '{0}-{1}' -f $Action, (Get-Date -Format 'yyyyMMdd-HHmmss')
}
if ($EvidenceName -notmatch '^[A-Za-z0-9_-]+$')
{
	throw 'EvidenceName must be a simple file name.'
}
$TaskLog = Join-Path $TaskEvidence ($EvidenceName + '.log')
$TaskStarted = (Get-Date).ToUniversalTime().ToString('o')
$TaskExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
switch ($Action)
{
	'BuildEditor'
	{
		$TaskExecutable = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
		$TaskArguments = @('ThreeDreamingKnightsEditor', 'Win64', 'Development',
			('-Project=' + $TaskProject), '-WaitMutex', '-NoHotReloadFromIDE', '-NoLiveCoding')
	}
	'Bootstrap'
	{
		$TaskArguments = @($TaskProject, '-run=KnightBootstrap', '-unattended', '-nop4', '-NullRHI',
			('-Milestone=' + $Milestone), ('-abslog=' + $TaskLog))
	}
	'Test'
	{
		$TaskArguments = @($TaskProject, '-unattended', '-nop4', '-NullRHI', '-nosplash',
			('-ExecCmds=Automation RunTests ' + $TestFilter), '-TestExit=Automation Test Queue Empty',
			('-ReportExportPath=' + (Join-Path $TaskEvidence ($EvidenceName + '-report'))),
			('-abslog=' + $TaskLog))
	}
	'BootSmoke'
	{
		$TaskArguments = @($TaskProject, '/Game/Knight/Maps/Playables/L_M00_Boot', '-game',
			'-KnightBootSmoke', '-unattended', '-nop4', '-NullRHI', '-nosplash', ('-abslog=' + $TaskLog))
	}
	'CookBoot'
	{
		$TaskArguments = @($TaskProject, '-run=Cook', '-TargetPlatform=Windows',
			'-Map=/Game/Knight/Maps/Playables/L_M00_Boot', '-unattended', '-nop4', '-NullRHI',
			('-abslog=' + $TaskLog))
	}
}
if (!(Test-Path -LiteralPath $TaskExecutable))
{
	throw "Missing engine tool: $TaskExecutable"
}
$TaskRecord = [ordered]@{
	action = $Action
	startedUtc = $TaskStarted
	executable = $TaskExecutable
	arguments = $TaskArguments
	log = $TaskLog
}
$TaskRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($TaskLog + '.command.json') -Encoding UTF8
& $TaskExecutable @TaskArguments 2>&1 | Tee-Object -FilePath ($TaskLog + '.stdout')
$TaskExit = $LASTEXITCODE
$TaskRecord.nativeExitCode = $TaskExit
if ($Action -eq 'Test' -and $TaskExit -eq 0)
{
	$TaskReportPath = Join-Path $TaskEvidence ($EvidenceName + '-report\index.json')
	if (!(Test-Path -LiteralPath $TaskReportPath))
	{
		$TaskExit = 1
		$TaskRecord.validationFailure = 'Automation did not export a report.'
	}
	else
	{
		$TaskReport = Get-Content -LiteralPath $TaskReportPath -Raw | ConvertFrom-Json
		$TaskRecord.testSummary = [ordered]@{
			succeeded = $TaskReport.succeeded
			succeededWithWarnings = $TaskReport.succeededWithWarnings
			failed = $TaskReport.failed
			notRun = $TaskReport.notRun
			inProcess = $TaskReport.inProcess
		}
		if ($TaskReport.failed -gt 0 -or $TaskReport.notRun -gt 0 -or $TaskReport.inProcess -gt 0 -or
			($TaskReport.succeeded + $TaskReport.succeededWithWarnings) -eq 0)
		{
			$TaskExit = 1
			$TaskRecord.validationFailure = 'Automation report contains failures/skips or no completed tests.'
		}
	}
}
if ($Action -eq 'BootSmoke' -and $TaskExit -eq 0)
{
	$TaskBootOutput = Get-Content -LiteralPath $TaskLog -Raw
	if ($TaskBootOutput -notmatch 'FoundationReady=true' -or $TaskBootOutput -match ': Error:|Fatal error:')
	{
		$TaskExit = 1
		$TaskRecord.validationFailure = 'Boot did not reach verified readiness, or the engine logged an error.'
	}
}
$TaskRecord.finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
$TaskRecord.exitCode = $TaskExit
$TaskRecord.result = if ($TaskExit -eq 0) { 'PASS' } else { 'FAIL' }
$TaskRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($TaskLog + '.result.json') -Encoding UTF8
exit $TaskExit
