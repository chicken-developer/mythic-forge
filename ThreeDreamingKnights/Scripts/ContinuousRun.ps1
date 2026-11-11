param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('BuildEditor', 'Bootstrap', 'Test', 'BootSmoke', 'CookBoot')]
	[string]$Action,
	[string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
	[string]$EvidenceName = '',
	[ValidateSet('Development', 'DebugGame')]
	[string]$Configuration = 'Development',
	[ValidateSet(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 55, 56, 75)]
	[int]$Milestone = 0,
	[ValidatePattern('^[A-Za-z0-9_.]+$')]
	[string]$TestFilter = 'Knight.',
	[string]$ConsoleCommands = '',
	[switch]$Rendered,
	[string[]]$AdditionalEngineArguments = @()
)

$ErrorActionPreference = 'Stop'
$TaskProjectRoot = Split-Path -Parent $PSScriptRoot
$TaskProject = Join-Path $TaskProjectRoot 'ThreeDreamingKnights.uproject'
$TaskEvidence = Join-Path $TaskProjectRoot 'Saved\Evidence\ContinuousRun'
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
		$TaskArguments = @('ThreeDreamingKnightsEditor', 'Win64', $Configuration,
			('-Project=' + $TaskProject), '-WaitMutex', '-NoHotReloadFromIDE', '-NoLiveCoding')
	}
	'Bootstrap'
	{
		$TaskArguments = @($TaskProject, '-run=KnightBootstrap', '-unattended', '-nop4', '-NullRHI',
			('-Milestone=' + $Milestone), ('-abslog=' + $TaskLog))
	}
	'Test'
	{
		$TaskTestCommands = 'Automation RunTests ' + $TestFilter
		if (![string]::IsNullOrWhiteSpace($ConsoleCommands))
		{
			$TaskTestCommands = $ConsoleCommands.TrimEnd(',') + ',' + $TaskTestCommands
		}
		$TaskArguments = @($TaskProject, '-unattended', '-nop4', '-nosplash',
			('-ExecCmds=' + $TaskTestCommands), '-TestExit=Automation Test Queue Empty',
			('-ReportExportPath=' + (Join-Path $TaskEvidence ($EvidenceName + '-report'))),
			('-abslog=' + $TaskLog), '-DDC=InstalledNoZenLocalFallback')
		if ($Rendered)
		{
			$TaskArguments += @('-d3d12', '-windowed', '-ResX=1600', '-ResY=900')
		}
		else
		{
			$TaskArguments += '-NullRHI'
		}
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
if ($Configuration -eq 'DebugGame' -and $Action -ne 'BuildEditor')
{
	$TaskExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe'
}
if ($Action -ne 'BuildEditor')
{
	# Headless automation must not replace the interactive Editor's RiderLink discovery port.
	$TaskArguments += '-DisablePlugins=RiderLink'
	$TaskArguments += $AdditionalEngineArguments
}
if (!(Test-Path -LiteralPath $TaskExecutable))
{
	throw "Missing engine tool: $TaskExecutable"
}
$TaskRecord = [ordered]@{
	action = $Action
	configuration = $Configuration
	startedUtc = $TaskStarted
	executable = $TaskExecutable
	arguments = $TaskArguments
	log = $TaskLog
}
$TaskRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($TaskLog + '.command.json') -Encoding UTF8
if ($Action -eq 'BuildEditor' -or $Action -eq 'Bootstrap')
{
	$TaskLoadedProjectFilter = "Name LIKE 'UnrealEditor%.exe'"
	$TaskLoadedProjectProcesses = @(Get-CimInstance Win32_Process -Filter $TaskLoadedProjectFilter | Where-Object {
			$null -ne $_.CommandLine -and $_.CommandLine.Replace('/', '\').IndexOf(
				$TaskProject, [StringComparison]::OrdinalIgnoreCase) -ge 0
		})
	$TaskBlockingProcesses = $TaskLoadedProjectProcesses
	if ($Action -eq 'BuildEditor' -and $Configuration -eq 'DebugGame')
	{
		# DebugGame game DLLs and manifest are separate from Development. Never use hot reload here.
		$TaskBlockingProcesses = @($TaskLoadedProjectProcesses | Where-Object {
			try
			{
				$TaskProcessModules = @((Get-Process -Id $_.ProcessId -ErrorAction Stop).Modules)
				$TaskProjectDebugModules = @($TaskProcessModules | Where-Object {
					$_.FileName.StartsWith($TaskProjectRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
					$_.ModuleName -like '*-Win64-DebugGame.dll'
				})
				$TaskProjectDebugModules.Count -gt 0
			}
			catch
			{
				# If loaded modules cannot be read, retain the conservative lock guard.
				$true
			}
		})
	}
	if ($TaskBlockingProcesses.Count -gt 0)
	{
		$TaskRecord.nativeExitCode = $null
		$TaskRecord.blockingProcessIds = @($TaskBlockingProcesses | ForEach-Object { $_.ProcessId })
		$TaskRecord.validationFailure = 'Save and close the loaded project before linking DLLs or authoring assets.'
		$TaskRecord.finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
		$TaskRecord.exitCode = 2
		$TaskRecord.result = 'BLOCKED'
		$TaskRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($TaskLog + '.result.json') -Encoding UTF8
		Write-Output ('BLOCKED: project is loaded in process IDs {0}; no engine tool launched.' -f
			($TaskRecord.blockingProcessIds -join ', '))
		exit 2
	}
}
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
