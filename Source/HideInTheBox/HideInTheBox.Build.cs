using UnrealBuildTool;
public class HideInTheBox : ModuleRules
{
    public HideInTheBox(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ApplicationCore", "GameplayAbilities", "GameplayTags", "GameplayTasks" });
    }
}
