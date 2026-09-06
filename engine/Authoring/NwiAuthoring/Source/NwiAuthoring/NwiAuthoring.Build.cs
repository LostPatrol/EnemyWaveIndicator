// Editor-only generator dependencies; FSD declarations compile references to the game's existing API.
using UnrealBuildTool;
public class NwiAuthoring : ModuleRules
{
    public NwiAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "KismetCompiler", "FSD",
            "BlueprintGraph", "UMG", "UMGEditor", "SlateCore", "InputCore"
        });
    }
}
