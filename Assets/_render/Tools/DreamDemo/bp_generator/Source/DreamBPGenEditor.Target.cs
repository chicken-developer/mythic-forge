using UnrealBuildTool;

public class DreamBPGenEditorTarget : TargetRules
{
	public DreamBPGenEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DreamBPGen");
	}
}
