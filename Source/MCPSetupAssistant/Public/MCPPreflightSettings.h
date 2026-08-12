#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MCPPreflightSettings.generated.h"

UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="Unreal MCP Setup Assistant"))
class MCPSETUPASSISTANT_API UMCPPreflightSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMCPPreflightSettings();

    UPROPERTY(Config, EditAnywhere, Category="Verification", meta=(ClampMin="1.0", ClampMax="60.0", Units="s"))
    float RequestTimeoutSeconds;

    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
