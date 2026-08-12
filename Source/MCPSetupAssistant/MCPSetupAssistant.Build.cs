using UnrealBuildTool;

public class MCPSetupAssistant : ModuleRules
{
    public MCPSetupAssistant(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "JsonUtilities" });
        PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "DeveloperSettings", "InputCore", "ModelContextProtocol", "ModelContextProtocolEngine", "Projects", "Slate", "SlateCore", "ToolMenus", "UnrealEd" });
    }
}
