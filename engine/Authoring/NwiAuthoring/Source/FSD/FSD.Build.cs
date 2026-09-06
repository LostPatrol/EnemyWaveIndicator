// Editor/cook-only reflection stubs; the shipping game supplies /Script/FSD implementations.
using UnrealBuildTool;
public class FSD : ModuleRules
{
    public FSD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}
