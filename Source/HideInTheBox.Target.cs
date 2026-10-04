using UnrealBuildTool;
public class HideInTheBoxTarget : TargetRules
{
    public HideInTheBoxTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("HideInTheBox");
    }
}
