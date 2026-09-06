// Editor-only asset authoring dependencies; generated assets use stock engine classes.
using UnrealBuildTool;
public class NwiAuthoring : ModuleRules
{
    public NwiAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "KismetCompiler",
            "BlueprintGraph", "UMG", "UMGEditor", "SlateCore", "InputCore"
        });
    }
}
