using UnrealBuildTool;
public class HideInTheBoxEditorTarget : TargetRules
{
    public HideInTheBoxEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("HideInTheBox");
    }
}
