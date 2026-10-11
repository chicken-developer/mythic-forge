using UnrealBuildTool;
public class ThanhABlockoutEditor : ModuleRules
{
    public ThanhABlockoutEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Json", "Landscape",
            "MeshDescription", "StaticMeshDescription", "AssetRegistry", "RenderCore",
            "RHI", "ImageWrapper", "TDKKingdomMap", "PhysicsCore", "GeometryCore", "DeveloperSettings", "Foliage"
        });
    }
}
