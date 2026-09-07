// Editor-only dependencies; generated assets use standard Engine reflection and the native handoff.
using UnrealBuildTool;
public class NwiAuthoring : ModuleRules
{
    public NwiAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "KismetCompiler",
            "BlueprintGraph", "UMG", "UMGEditor", "SlateCore", "Slate", "SlateNullRenderer", "InputCore"
        });
    }
}
