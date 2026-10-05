using UnrealBuildTool;
public class HideInTheBoxEditorTarget : TargetRules
{
    public HideInTheBoxEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("HideInTheBox");
    }
}
