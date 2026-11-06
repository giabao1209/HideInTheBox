using UnrealBuildTool;
public class HideInTheBoxTarget : TargetRules
{
    public HideInTheBoxTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("HideInTheBox");
    }
}
