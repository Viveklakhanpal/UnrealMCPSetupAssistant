#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "MCPPreflightRules.h"

class FMCPPreflightClient;
class SVerticalBox;
class STextBlock;
class SEditableTextBox;

class SMCPPreflightPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMCPPreflightPanel) {}
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual ~SMCPPreflightPanel() override;

private:
    FReply RunPreflight();
    FReply StartServer();
    FReply EnableAutoStart();
    FReply GenerateConfiguration();
    FReply DetectInstalledClients();
    FReply CopyEndpoint();
    FReply OpenProjectFolder();
    FReply CopyTerminalCommand(int32 CommandIndex);
    FReply OpenTerminalSettings();
    void HandleComplete(const FMCPPreflightContext& Result);
    void RefreshSetupState();
    void RebuildResults();
    FString GetEndpoint() const;
    FString GetClientLaunchCommand() const;
    TArray<FString> GetTerminalCommands() const;
    bool IsServerRunning() const;
    bool CanGenerateConfiguration() const;
    FText GetPrimaryActionText() const;
    FReply RunPrimaryAction();
    TSharedRef<SWidget> MakeClientWidget(TSharedPtr<FString> Item) const;
    void OnClientSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
    void OnCustomCommandChanged(const FText& Text);
    FText GetStatusText() const;
    FSlateColor GetStatusColor() const;
    bool CanRun() const;

    TSharedPtr<FMCPPreflightClient> Client;
    TSharedPtr<SVerticalBox> ResultsBox;
    TSharedPtr<STextBlock> SelectedClientText;
    TSharedPtr<SEditableTextBox> CustomCommandTextBox;
    TArray<TSharedPtr<FString>> ClientOptions;
    TSharedPtr<FString> SelectedClient;
    FString LastGeneratedPath;
    FString SetupMessage;
    FString DetectionMessage;
    FString CustomCommand;
    FMCPPreflightRuleRegistry RuleRegistry;
    FMCPPreflightContext Context;
    TArray<FMCPDiagnostic> Diagnostics;
    enum class EState : uint8 { NotRun, Running, Passed, Warnings, Failed } State = EState::NotRun;
};
