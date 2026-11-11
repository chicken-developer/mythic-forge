param(
	[string] $ProjectRoot = (Split-Path -Parent $PSScriptRoot),
	[string] $EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
	[string] $OutputPath = '',
	[string] $PrimaryModule = 'ThreeDreamGolems',
	[string] $ExpectedEngineAssociation = '5.8'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Failures = [System.Collections.Generic.List[string]]::new()
$DependencyGraph = [ordered]@{}
$PluginAudit = [System.Collections.Generic.List[object]]::new()
$ModuleTypes = @{}
$ExternalEditorModules = @{}
$VisitState = @{}
$VisitPath = [System.Collections.Generic.List[string]]::new()
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$EngineRoot = [IO.Path]::GetFullPath($EngineRoot)
if ([string]::IsNullOrWhiteSpace($OutputPath))
{
	$OutputPath = Join-Path $ProjectRoot 'Saved\Evidence\M0\module_dependency_audit.json'
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$ProjectFile = Join-Path $ProjectRoot ($PrimaryModule + '.uproject')

function Get-PropertyValue
{
	param([object] $Object, [string] $Name)
	$Property = $Object.PSObject.Properties[$Name]
	if ($null -ne $Property)
	{
		return $Property.Value
	}
	return $null
}

function Remove-CSharpComments
{
	param([string] $SourceText)
	# Preserve strings so commented examples cannot introduce dependency edges.
	return [regex]::Replace($SourceText, '"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/',
		[System.Text.RegularExpressions.MatchEvaluator] {
			param($Match)
			if ($Match.Value.StartsWith('"'))
			{
				return $Match.Value
			}
			return [regex]::Replace($Match.Value, '[^\r\n]', ' ')
		})
}

function Get-ModuleDependencies
{
	param([string] $BuildFile, [string] $ModuleName)
	$BuildText = Remove-CSharpComments (Get-Content -LiteralPath $BuildFile -Raw -Encoding UTF8)
	$CallPattern = '\b(?<Kind>PublicDependencyModuleNames|PrivateDependencyModuleNames|' +
		'DynamicallyLoadedModuleNames)\s*\.\s*(?<Method>Add|AddRange)\s*\((?<Arguments>.*?)\)\s*;'
	$Calls = [regex]::Matches($BuildText, $CallPattern, [Text.RegularExpressions.RegexOptions]::Singleline)
	$CallStarts = [regex]::Matches($BuildText,
		'\b(?:PublicDependencyModuleNames|PrivateDependencyModuleNames|DynamicallyLoadedModuleNames)\s*\.\s*Add(?:Range)?\s*\(')
	if ($Calls.Count -ne $CallStarts.Count)
	{
		$Failures.Add("Unparsed dependency call in $ModuleName; use literal Add/AddRange or extend the auditor.")
	}
	$Dependencies = [System.Collections.Generic.List[object]]::new()
	foreach ($Call in $Calls)
	{
		$Arguments = $Call.Groups['Arguments'].Value
		$LiteralMatches = [regex]::Matches($Arguments, '"(?<Name>[A-Za-z][A-Za-z0-9_]*)"')
		$Remainder = [regex]::Replace($Arguments, '"[A-Za-z][A-Za-z0-9_]*"', '')
		if ($Call.Groups['Method'].Value -eq 'AddRange')
		{
			$Remainder = [regex]::Replace($Remainder, '\bnew\s+(?:string\s*)?\[\s*\]', '')
			$Remainder = [regex]::Replace($Remainder, '[\s{},]', '')
		}
		else
		{
			$Remainder = $Remainder.Trim()
		}
		if ($Remainder.Length -gt 0)
		{
			$Failures.Add("Dynamic dependency expression in $ModuleName cannot be proven: $Arguments")
		}
		foreach ($Literal in $LiteralMatches)
		{
			$Dependencies.Add([ordered]@{
				Name = $Literal.Groups['Name'].Value
				Kind = $Call.Groups['Kind'].Value
			})
		}
	}
	return $Dependencies.ToArray()
}

function Visit-Module
{
	param([string] $ModuleName)
	if ($VisitState[$ModuleName] -eq 'Active')
	{
		$Failures.Add('Dependency cycle: ' + (($VisitPath.ToArray() + $ModuleName) -join ' -> '))
		return
	}
	if ($VisitState[$ModuleName] -eq 'Complete')
	{
		return
	}
	$VisitState[$ModuleName] = 'Active'
	$VisitPath.Add($ModuleName)
	foreach ($Dependency in $DependencyGraph[$ModuleName])
	{
		if ($ModuleTypes.ContainsKey($Dependency.Name))
		{
			Visit-Module $Dependency.Name
		}
	}
	$VisitPath.RemoveAt($VisitPath.Count - 1)
	$VisitState[$ModuleName] = 'Complete'
}

function Find-EditorLeak
{
	param([string] $ModuleName, [string[]] $Path, [hashtable] $Seen)
	if ($Seen.ContainsKey($ModuleName))
	{
		return
	}
	$Seen[$ModuleName] = $true
	foreach ($Dependency in $DependencyGraph[$ModuleName])
	{
		if ($ExternalEditorModules.ContainsKey($Dependency.Name))
		{
			$Failures.Add('Runtime -> Editor dependency: ' + (($Path + $Dependency.Name) -join ' -> '))
		}
		if (-not $ModuleTypes.ContainsKey($Dependency.Name))
		{
			continue
		}
		$NextPath = $Path + $Dependency.Name
		if ($ModuleTypes[$Dependency.Name] -like 'Editor*' -or $ModuleTypes[$Dependency.Name] -eq 'UncookedOnly')
		{
			$Failures.Add('Runtime -> Editor dependency: ' + ($NextPath -join ' -> '))
		}
		Find-EditorLeak $Dependency.Name $NextPath $Seen
	}
}

function Test-DescriptorFilter
{
	param([object] $Descriptor, [string] $Dimension, [string] $Value)
	$Allowed = @(Get-PropertyValue $Descriptor ($Dimension + 'AllowList'))
	$Denied = @(Get-PropertyValue $Descriptor ($Dimension + 'DenyList'))
	$Allowed = @($Allowed | Where-Object { $null -ne $_ })
	return ($Allowed.Count -eq 0 -or $Value -in $Allowed) -and $Value -notin $Denied
}

$ExpectedModules = @(
	$PrimaryModule, 'GolemCore', 'GolemFramework', 'GolemAbilitySystem', 'GolemCombat', 'GolemAI',
	'GolemItems', 'GolemQuest', 'GolemDialogue', 'GolemWorld', 'GolemUI', 'GolemOnline', 'GolemSave',
	'GolemDeveloper'
)
$RuntimePlugins = @(
	'GameplayAbilities', 'StateTree', 'GameplayStateTree', 'EnhancedInput', 'CommonUI', 'MotionWarping',
	'OnlineSubsystemNull', 'OnlineSubsystemSteam', 'SteamSockets'
)
$EditorPlugins = @('ModelingToolsEditorMode', 'PythonScriptPlugin')
$EngineAssociation = $null

try
{
	$ProjectDescriptor = Get-Content -LiteralPath $ProjectFile -Raw -Encoding UTF8 | ConvertFrom-Json
	$EngineAssociation = $ProjectDescriptor.EngineAssociation
	if ($EngineAssociation -ne $ExpectedEngineAssociation)
	{
		$Failures.Add("EngineAssociation is $EngineAssociation; required $ExpectedEngineAssociation.")
	}
	foreach ($Module in $ProjectDescriptor.Modules)
	{
		if ($ModuleTypes.ContainsKey($Module.Name))
		{
			$Failures.Add("Duplicate module descriptor: $($Module.Name)")
		}
		$ModuleTypes[$Module.Name] = $Module.Type
	}
	foreach ($ModuleName in $ExpectedModules)
	{
		if (-not $ModuleTypes.ContainsKey($ModuleName))
		{
			$Failures.Add("Missing logical module: $ModuleName (GolemGame maps to $PrimaryModule).")
		}
	}
	foreach ($ModuleName in $ModuleTypes.Keys)
	{
		if ($ModuleName -notin $ExpectedModules)
		{
			$Failures.Add("Unexpected project module outside locked architecture: $ModuleName")
		}
		$ExpectedType = 'Runtime'
		if ($ModuleName -eq 'GolemDeveloper')
		{
			$ExpectedType = 'Editor'
		}
		if ($ModuleTypes[$ModuleName] -ne $ExpectedType)
		{
			$Failures.Add("Module $ModuleName has type $($ModuleTypes[$ModuleName]); required $ExpectedType.")
		}
		$BuildFile = Join-Path $ProjectRoot "Source\$ModuleName\$ModuleName.Build.cs"
		if (-not (Test-Path -LiteralPath $BuildFile))
		{
			$Failures.Add("Missing Build.cs: $BuildFile")
			$DependencyGraph[$ModuleName] = @()
			continue
		}
		$DependencyGraph[$ModuleName] = @(Get-ModuleDependencies $BuildFile $ModuleName)
	}
	foreach ($ModuleName in $ModuleTypes.Keys)
	{
		Visit-Module $ModuleName
	}
	$SourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Source') -Filter '*.cpp' -Recurse -File)
	$PrimaryRegistrations = @($SourceFiles | Where-Object {
		$SourceText = Remove-CSharpComments (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8)
		$SourceText -match '\bIMPLEMENT_PRIMARY_GAME_MODULE\s*\('
	})
	if ($PrimaryRegistrations.Count -ne 1)
	{
		$Failures.Add("Expected one primary game module; found $($PrimaryRegistrations.Count).")
	}
	foreach ($TargetSuffix in @('', 'Editor'))
	{
		$TargetPath = Join-Path $ProjectRoot ('Source\' + $PrimaryModule + $TargetSuffix + '.Target.cs')
		$TargetText = Remove-CSharpComments (Get-Content -LiteralPath $TargetPath -Raw -Encoding UTF8)
		$HasDeveloper = $TargetText -match 'ExtraModuleNames\s*\.\s*Add(?:Range)?\s*\([^;]*"GolemDeveloper"'
		if (($TargetSuffix -eq 'Editor') -ne $HasDeveloper)
		{
			$Failures.Add("GolemDeveloper target segregation invalid: $TargetPath")
		}
		if ($TargetText -notmatch ('EngineIncludeOrderVersion\.Unreal' + $ExpectedEngineAssociation.Replace('.', '_')))
		{
			$Failures.Add("Target include order differs from locked engine: $TargetPath")
		}
	}
	$PluginScan = @{
		LiteralPath = Join-Path $EngineRoot 'Engine\Plugins'
		Filter = '*.uplugin'
		Recurse = $true
		File = $true
	}
	$PluginFiles = @(Get-ChildItem @PluginScan)
	$ProjectPluginRoot = Join-Path $ProjectRoot 'Plugins'
	if (Test-Path -LiteralPath $ProjectPluginRoot)
	{
		$PluginFiles += @(Get-ChildItem -LiteralPath $ProjectPluginRoot -Filter '*.uplugin' -Recurse -File)
	}
	$PluginIndex = @{}
	foreach ($PluginFile in $PluginFiles)
	{
		$PluginIndex[$PluginFile.BaseName] = $PluginFile.FullName
	}
	$EnabledPluginNames = @($ProjectDescriptor.Plugins | Where-Object { $_.Enabled } | ForEach-Object { $_.Name })
	foreach ($PluginName in ($RuntimePlugins + $EditorPlugins))
	{
		if ($PluginName -notin $EnabledPluginNames)
		{
			$Failures.Add("Required plugin is not enabled: $PluginName")
		}
	}
	foreach ($Plugin in ($ProjectDescriptor.Plugins | Where-Object { $_.Enabled }))
	{
		if (-not $PluginIndex.ContainsKey($Plugin.Name))
		{
			$Failures.Add("Enabled plugin not installed: $($Plugin.Name)")
			continue
		}
		$DescriptorPath = $PluginIndex[$Plugin.Name]
		$InstalledDescriptor = Get-Content -LiteralPath $DescriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
		$PluginModules = @(Get-PropertyValue $InstalledDescriptor 'Modules' | Where-Object { $null -ne $_ })
		foreach ($PluginModule in $PluginModules)
		{
			if ($PluginModule.Type -like 'Editor*' -or $PluginModule.Type -eq 'UncookedOnly')
			{
				$ExternalEditorModules[$PluginModule.Name] = $DescriptorPath
			}
		}
		$SupportsWin64 = Test-DescriptorFilter $Plugin 'Platform' 'Win64'
		$InstalledPlatforms = @(Get-PropertyValue $InstalledDescriptor 'SupportedTargetPlatforms')
		$InstalledPlatforms = @($InstalledPlatforms | Where-Object { $null -ne $_ })
		$SupportsWin64 = $SupportsWin64 -and ($InstalledPlatforms.Count -eq 0 -or 'Win64' -in $InstalledPlatforms)
		$SupportsEditor = $SupportsWin64 -and (Test-DescriptorFilter $Plugin 'Target' 'Editor')
		$SupportsGame = $SupportsWin64 -and (Test-DescriptorFilter $Plugin 'Target' 'Game')
		if ($Plugin.Name -in $RuntimePlugins -and (-not $SupportsEditor -or -not $SupportsGame))
		{
			$Failures.Add("Runtime plugin filtered out from required Win64 Editor/Game target: $($Plugin.Name)")
		}
		if ($Plugin.Name -in $EditorPlugins -and (-not $SupportsEditor -or $SupportsGame))
		{
			$Failures.Add("Editor plugin target filter leaks into Game or excludes Editor: $($Plugin.Name)")
		}
		$PluginAudit.Add([ordered]@{
			Name = $Plugin.Name
			InstalledDescriptor = $DescriptorPath
			Win64Editor = $SupportsEditor
			Win64Game = $SupportsGame
			TargetAllowList = @(Get-PropertyValue $Plugin 'TargetAllowList')
			PlatformAllowList = @(Get-PropertyValue $Plugin 'PlatformAllowList')
		})
	}
	$EngineEditorRoot = Join-Path $EngineRoot 'Engine\Source\Editor'
	foreach ($EditorBuildFile in @(Get-ChildItem -LiteralPath $EngineEditorRoot -Filter '*.Build.cs' -Recurse -File))
	{
		$ExternalEditorModules[$EditorBuildFile.Name.Replace('.Build.cs', '')] = $EditorBuildFile.FullName
	}
	foreach ($ModuleName in $ModuleTypes.Keys)
	{
		if ($ModuleTypes[$ModuleName] -eq 'Runtime')
		{
			Find-EditorLeak $ModuleName @($ModuleName) @{}
		}
	}
}
catch
{
	$Failures.Add('Audit execution error: ' + $_.Exception.Message + ' at ' + $_.ScriptStackTrace)
}

$Status = 'PASS'
if ($Failures.Count -gt 0)
{
	$Status = 'FAIL'
}
$Audit = [ordered]@{
	TimestampUtc = [DateTime]::UtcNow.ToString('o')
	Status = $Status
	ProjectRoot = $ProjectRoot
	EngineRoot = $EngineRoot
	EngineAssociation = $EngineAssociation
	LogicalGolemGame = $PrimaryModule
	ExpectedModules = $ExpectedModules
	ModuleTypes = $ModuleTypes
	InstalledEditorModuleCount = $ExternalEditorModules.Count
	DependencyGraph = $DependencyGraph
	InstalledPlugins = $PluginAudit.ToArray()
	Failures = $Failures.ToArray()
	Scope = 'Conservative static literal dependency graph and descriptors; does not replace UBT or runtime verification.'
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[IO.File]::WriteAllText($OutputPath, ($Audit | ConvertTo-Json -Depth 12) + [Environment]::NewLine,
	[Text.UTF8Encoding]::new($false))
Write-Output "$Status module audit: $OutputPath"
foreach ($Failure in $Failures)
{
	Write-Output "FAIL: $Failure"
}
if ($Failures.Count -gt 0)
{
	exit 1
}
exit 0
