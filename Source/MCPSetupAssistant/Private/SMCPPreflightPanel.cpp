#include "SMCPPreflightPanel.h"
#include "MCPPreflightClient.h"
#include "MCPPreflightSettings.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolClientConfig.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMCPPreflightPanel"

namespace
{
FSlateColor ReadyColor(bool bReady) { return bReady ? FLinearColor(0.2f, 0.75f, 0.35f) : FLinearColor(0.9f, 0.2f, 0.15f); }

struct FRequiredPluginStatus
{
    bool bAvailable = false;
    bool bEnabled = false;
    bool bLoaded = false;
};

FRequiredPluginStatus GetRequiredPluginStatus(const TCHAR* PluginName, const TCHAR* ModuleName = nullptr)
{
    FRequiredPluginStatus Status;
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
    Status.bAvailable = Plugin.IsValid();
    Status.bEnabled = Status.bAvailable && Plugin->IsEnabled();
    Status.bLoaded = Status.bEnabled && (!ModuleName || FModuleManager::Get().IsModuleLoaded(ModuleName));
    return Status;
}

FText PluginStatusText(const TCHAR* PluginName, const TCHAR* ModuleName = nullptr)
{
    const FRequiredPluginStatus Status = GetRequiredPluginStatus(PluginName, ModuleName);
    if (!Status.bAvailable)
    {
        return LOCTEXT("PluginUnavailable", "Unavailable");
    }
    if (!Status.bEnabled || !Status.bLoaded)
    {
        return LOCTEXT("PluginRestartRequired", "Restart required");
    }
    return LOCTEXT("PluginEnabled", "Enabled");
}

FSlateColor PluginStatusColor(const TCHAR* PluginName, const TCHAR* ModuleName = nullptr)
{
    const FRequiredPluginStatus Status = GetRequiredPluginStatus(PluginName, ModuleName);
    return ReadyColor(Status.bAvailable && Status.bEnabled && Status.bLoaded);
}

bool AreRequiredPluginsReady()
{
    return GetRequiredPluginStatus(TEXT("ModelContextProtocol"), TEXT("ModelContextProtocol")).bLoaded
        && GetRequiredPluginStatus(TEXT("ToolsetRegistry"), TEXT("ToolsetRegistry")).bLoaded
        && GetRequiredPluginStatus(TEXT("AllToolsets")).bLoaded
        && GetRequiredPluginStatus(TEXT("Terminal"), TEXT("Terminal")).bLoaded;
}

bool IsCommandAvailable(const FString& Command)
{
    TArray<FString> PathDirectories;
    FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).ParseIntoArray(PathDirectories, FPlatformMisc::GetPathVarDelimiter(), true);

#if PLATFORM_WINDOWS
    static const TCHAR* Extensions[] = { TEXT(""), TEXT(".exe"), TEXT(".cmd"), TEXT(".bat") };
#else
    static const TCHAR* Extensions[] = { TEXT("") };
#endif

    for (const FString& Directory : PathDirectories)
    {
        for (const TCHAR* Extension : Extensions)
        {
            if (IFileManager::Get().FileExists(*FPaths::Combine(Directory, Command + Extension)))
            {
                return true;
            }
        }
    }
    return false;
}

EModelContextProtocolClient ClientFromName(const FString& Name)
{
    if (Name == TEXT("Cursor")) return EModelContextProtocolClient::Cursor;
    if (Name == TEXT("VS Code")) return EModelContextProtocolClient::VSCode;
    if (Name == TEXT("Gemini")) return EModelContextProtocolClient::Gemini;
    if (Name == TEXT("Codex")) return EModelContextProtocolClient::Codex;
    return EModelContextProtocolClient::ClaudeCode;
}

FString ClientConfigPath(const FString& Name)
{
    FString RelativePath = TEXT(".mcp.json");
    if (Name == TEXT("Cursor")) RelativePath = TEXT(".cursor/mcp.json");
    else if (Name == TEXT("VS Code")) RelativePath = TEXT(".vscode/mcp.json");
    else if (Name == TEXT("Gemini")) RelativePath = TEXT(".gemini/settings.json");
    else if (Name == TEXT("Codex")) RelativePath = TEXT(".codex/config.toml");
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}

bool IsValidJsonFile(const FString& FilePath)
{
    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *FilePath))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
    return FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
}

enum class ECodexConfigState : uint8 { MissingEntry, AlreadyConfigured, ConflictingEntry, Unreadable };

ECodexConfigState InspectCodexConfiguration(const FString& FilePath, const FString& ExpectedUrl, FString& OutContents)
{
    if (!FFileHelper::LoadFileToString(OutContents, *FilePath))
    {
        return ECodexConfigState::Unreadable;
    }

    const FString Header = TEXT("[mcp_servers.unreal-mcp]");
    const int32 SectionStart = OutContents.Find(Header, ESearchCase::IgnoreCase);
    if (SectionStart == INDEX_NONE)
    {
        return ECodexConfigState::MissingEntry;
    }

    const int32 NextSection = OutContents.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromStart, SectionStart + Header.Len());
    const FString Section = NextSection == INDEX_NONE
        ? OutContents.Mid(SectionStart)
        : OutContents.Mid(SectionStart, NextSection - SectionStart);
    return Section.Contains(FString::Printf(TEXT("url = \"%s\""), *ExpectedUrl), ESearchCase::IgnoreCase)
        ? ECodexConfigState::AlreadyConfigured
        : ECodexConfigState::ConflictingEntry;
}
}

void SMCPPreflightPanel::Construct(const FArguments&)
{
    Client = MakeShared<FMCPPreflightClient>();
    for (const TCHAR* Name : { TEXT("Codex"), TEXT("Claude Code"), TEXT("Cursor"), TEXT("VS Code"), TEXT("Gemini"), TEXT("Custom...") }) ClientOptions.Add(MakeShared<FString>(Name));
    SelectedClient = ClientOptions[0];
    DetectionMessage = TEXT("Installed clients have not been checked yet.");

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(16)
            [SNew(STextBlock).Text(LOCTEXT("Title", "Unreal MCP Setup Assistant")).TextStyle(FAppStyle::Get(), "LargeText")]
            + SVerticalBox::Slot().AutoHeight().Padding(16, 0, 16, 14)
            [SNew(STextBlock).Text(LOCTEXT("Subtitle", "Enable the native Unreal MCP workflow, verify the connection, and create your client configuration.")).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]

            + SVerticalBox::Slot().AutoHeight().Padding(16, 4)
            [
                SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Step1", "1. Required Unreal plugins")).TextStyle(FAppStyle::Get(), "HeadingExtraSmall")]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("NativeMCP", "Unreal MCP"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([] { return PluginStatusText(TEXT("ModelContextProtocol"), TEXT("ModelContextProtocol")); }).ColorAndOpacity_Lambda([] { return PluginStatusColor(TEXT("ModelContextProtocol"), TEXT("ModelContextProtocol")); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("ToolsetRegistry", "Toolset Registry"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([] { return PluginStatusText(TEXT("ToolsetRegistry"), TEXT("ToolsetRegistry")); }).ColorAndOpacity_Lambda([] { return PluginStatusColor(TEXT("ToolsetRegistry"), TEXT("ToolsetRegistry")); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("AllToolsets", "All Toolsets"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([] { return PluginStatusText(TEXT("AllToolsets")); }).ColorAndOpacity_Lambda([] { return PluginStatusColor(TEXT("AllToolsets")); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("Terminal", "Unreal Terminal"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([] { return PluginStatusText(TEXT("Terminal"), TEXT("Terminal")); }).ColorAndOpacity_Lambda([] { return PluginStatusColor(TEXT("Terminal"), TEXT("Terminal")); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 0)
                    [SNew(STextBlock).Text_Lambda([] { return AreRequiredPluginsReady() ? LOCTEXT("DependenciesReady", "All required Unreal components are active.") : LOCTEXT("DependencyHint", "The assistant enables these components automatically. Restart Unreal Editor to finish activation; unavailable components require Unreal Engine 5.8."); }).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 4)
            [
                SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Step2", "2. Native MCP server")).TextStyle(FAppStyle::Get(), "HeadingExtraSmall")]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(GetEndpoint()); })]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this] { return IsServerRunning() ? LOCTEXT("Running", "Running") : LOCTEXT("Stopped", "Stopped"); }).ColorAndOpacity_Lambda([this] { return ReadyColor(IsServerRunning()); })]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[SNew(SButton).Text(LOCTEXT("StartServer", "Start Server")).IsEnabled_Lambda([this] { return !IsServerRunning(); }).OnClicked(this, &SMCPPreflightPanel::StartServer)]
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[SNew(SButton).Text(LOCTEXT("AutoStart", "Enable Auto-start")).IsEnabled_Lambda([] { return !UE::ModelContextProtocol::ShouldAutoStartServer(); }).OnClicked(this, &SMCPPreflightPanel::EnableAutoStart)]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("CopyEndpoint", "Copy Address")).OnClicked(this, &SMCPPreflightPanel::CopyEndpoint)]
                    ]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 4)
            [
                SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Step3", "3. Verify connection")).TextStyle(FAppStyle::Get(), "HeadingExtraSmall")]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [SNew(STextBlock).Text(LOCTEXT("VerifyHint", "Runs the MCP initialize handshake and tools/list against Unreal's active endpoint." )).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [SNew(STextBlock).Text(LOCTEXT("VerifyBoundary", "This verifies Unreal's MCP server and advertised tools. It does not verify AI-client sign-in or the client's connection to Unreal.")).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [SNew(SButton).Text(LOCTEXT("Verify", "Verify MCP")).IsEnabled_Lambda([this] { return IsServerRunning() && CanRun(); }).OnClicked(this, &SMCPPreflightPanel::RunPreflight)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[SAssignNew(ResultsBox, SVerticalBox)]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 4)
            [
                SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Step4", "4. Create client configuration")).TextStyle(FAppStyle::Get(), "HeadingExtraSmall")]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [SNew(STextBlock).Text(LOCTEXT("NodeRequirement", "Prerequisite: Some command-line AI clients require Node.js when launched from Unreal Terminal.")).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [SNew(SButton).Text(LOCTEXT("DetectClients", "Detect Installed Clients")).OnClicked(this, &SMCPPreflightPanel::DetectInstalledClients)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [SNew(STextBlock).Text_Lambda([this] { return FText::FromString(DetectionMessage); }).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ChooseClient", "MCP client"))]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [
                            SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&ClientOptions).InitiallySelectedItem(SelectedClient)
                            .OnGenerateWidget(this, &SMCPPreflightPanel::MakeClientWidget).OnSelectionChanged(this, &SMCPPreflightPanel::OnClientSelected)
                            [SAssignNew(SelectedClientText, STextBlock).Text(FText::FromString(*SelectedClient))]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [
                        SNew(SBox)
                        .Visibility_Lambda([this] { return SelectedClient.IsValid() && *SelectedClient == TEXT("Custom...") ? EVisibility::Visible : EVisibility::Collapsed; })
                        [SAssignNew(CustomCommandTextBox, SEditableTextBox).HintText(LOCTEXT("CustomCommandHint", "Custom client command, for example: my-ai-cli")).OnTextChanged(this, &SMCPPreflightPanel::OnCustomCommandChanged)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[SNew(SButton).Text(LOCTEXT("Generate", "Generate Configuration")).IsEnabled(this, &SMCPPreflightPanel::CanGenerateConfiguration).OnClicked(this, &SMCPPreflightPanel::GenerateConfiguration)]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("OpenFolder", "Open Project Folder")).OnClicked(this, &SMCPPreflightPanel::OpenProjectFolder)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [SNew(STextBlock).Text_Lambda([this] { return FText::FromString(SetupMessage); }).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(16, 12)
            [SNew(SButton).HAlign(HAlign_Center).Text(this, &SMCPPreflightPanel::GetPrimaryActionText).OnClicked(this, &SMCPPreflightPanel::RunPrimaryAction)]
        ]
    ];
    RefreshSetupState();
    RebuildResults();
}

SMCPPreflightPanel::~SMCPPreflightPanel() { if (Client.IsValid()) Client->Cancel(); }

FString SMCPPreflightPanel::GetEndpoint() const
{
    return FString::Printf(TEXT("http://127.0.0.1:%u%s"), UE::ModelContextProtocol::GetServerPortNumber(), *UE::ModelContextProtocol::GetServerUrlPath());
}

bool SMCPPreflightPanel::IsServerRunning() const
{
    IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
    return Module && Module->GetServer() && Module->GetServer()->IsServerRunning();
}

FReply SMCPPreflightPanel::StartServer()
{
    IModelContextProtocolModule::GetChecked().StartServer(UE::ModelContextProtocol::GetServerPortNumber(), UE::ModelContextProtocol::GetServerUrlPath());
    SetupMessage = IsServerRunning() ? TEXT("Native Unreal MCP server started.") : TEXT("The server could not be started. Check the Output Log for LogModelContextProtocol messages.");
    return FReply::Handled();
}

FReply SMCPPreflightPanel::EnableAutoStart()
{
    UModelContextProtocolSettings* Settings = GetMutableDefault<UModelContextProtocolSettings>();
    Settings->bAutoStartServer = true;
    Settings->SaveConfig();
    SetupMessage = TEXT("Auto-start enabled for this project. The MCP server will start with the editor.");
    return FReply::Handled();
}

FReply SMCPPreflightPanel::RunPreflight()
{
    State = EState::Running;
    Diagnostics.Reset();
    RebuildResults();
    Client->Run(GetEndpoint(), GetDefault<UMCPPreflightSettings>()->RequestTimeoutSeconds, FMCPPreflightComplete::CreateSP(this, &SMCPPreflightPanel::HandleComplete));
    return FReply::Handled();
}

void SMCPPreflightPanel::HandleComplete(const FMCPPreflightContext& Result)
{
    Context = Result;
    RuleRegistry.Evaluate(Context, Diagnostics);
    bool bError = false, bWarning = false;
    for (const FMCPDiagnostic& D : Diagnostics) { bError |= D.Severity == EMCPDiagnosticSeverity::Error; bWarning |= D.Severity == EMCPDiagnosticSeverity::Warning; }
    State = bError ? EState::Failed : (bWarning ? EState::Warnings : EState::Passed);
    SetupMessage = State == EState::Passed ? TEXT("MCP handshake succeeded. You can now generate a client configuration.") : TEXT("Verification found an issue. Follow the guidance shown above.");
    RebuildResults();
}

void SMCPPreflightPanel::RefreshSetupState()
{
    Context.EndpointUrl = GetEndpoint();
    SetupMessage = TEXT("Complete the steps above. Configuration is generated using Epic's native UE 5.8 implementation.");
}

bool SMCPPreflightPanel::CanGenerateConfiguration() const { return IsServerRunning() && State == EState::Passed && SelectedClient.IsValid() && *SelectedClient != TEXT("Custom..."); }

FReply SMCPPreflightPanel::DetectInstalledClients()
{
    struct FClientCommand { const TCHAR* Name; const TCHAR* Command; };
    static const FClientCommand KnownClients[] =
    {
        { TEXT("Codex"), TEXT("codex") },
        { TEXT("Claude Code"), TEXT("claude") },
        { TEXT("Cursor"), TEXT("cursor") },
        { TEXT("VS Code"), TEXT("code") },
        { TEXT("Gemini"), TEXT("gemini") }
    };

    TArray<FString> DetectedClients;
    for (const FClientCommand& ClientCommand : KnownClients)
    {
        if (IsCommandAvailable(ClientCommand.Command))
        {
            DetectedClients.Add(ClientCommand.Name);
        }
    }

    const bool bNodeFound = IsCommandAvailable(TEXT("node"));
    const FString ClientsText = DetectedClients.IsEmpty() ? TEXT("No supported AI client commands found") : FString::Join(DetectedClients, TEXT(", "));
    DetectionMessage = FString::Printf(TEXT("Node.js: %s  |  Detected clients: %s"), bNodeFound ? TEXT("Found") : TEXT("Not found"), *ClientsText);

    if (!DetectedClients.IsEmpty())
    {
        const FString& RecommendedClient = DetectedClients[0];
        const TSharedPtr<FString>* Option = ClientOptions.FindByPredicate([&RecommendedClient](const TSharedPtr<FString>& Item) { return Item.IsValid() && *Item == RecommendedClient; });
        if (Option)
        {
            OnClientSelected(*Option, ESelectInfo::Direct);
        }
    }
    return FReply::Handled();
}

FReply SMCPPreflightPanel::GenerateConfiguration()
{
    if (!SelectedClient.IsValid()) return FReply::Handled();
    if (*SelectedClient == TEXT("Custom..."))
    {
        SetupMessage = TEXT("Automatic configuration is not available for custom clients. Copy the MCP address and add it using the client's own configuration instructions.");
        return FReply::Handled();
    }
    const FString TargetPath = ClientConfigPath(*SelectedClient);
    const bool bAlreadyExists = IFileManager::Get().FileExists(*TargetPath);
    if (bAlreadyExists && *SelectedClient == TEXT("Codex"))
    {
        const FString ExpectedUrl = GetEndpoint();
        FString ExistingContents;
        switch (InspectCodexConfiguration(TargetPath, ExpectedUrl, ExistingContents))
        {
        case ECodexConfigState::AlreadyConfigured:
            LastGeneratedPath = TargetPath;
            SetupMessage = FString::Printf(TEXT("Setup complete. Codex is already configured for Unreal MCP at %s"), *TargetPath);
            return FReply::Handled();
        case ECodexConfigState::MissingEntry:
        {
            FString Separator;
            if (!ExistingContents.IsEmpty() && !ExistingContents.EndsWith(TEXT("\n"))) Separator = TEXT("\n");
            const FString UnrealSection = FString::Printf(TEXT("%s\n[mcp_servers.unreal-mcp]\nurl = \"%s\"\n"), *Separator, *ExpectedUrl);
            if (FFileHelper::SaveStringToFile(UnrealSection, *TargetPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
            {
                LastGeneratedPath = TargetPath;
                SetupMessage = FString::Printf(TEXT("Setup complete. Unreal MCP was added to the existing Codex configuration at %s"), *TargetPath);
            }
            else
            {
                SetupMessage = FString::Printf(TEXT("The Codex configuration could not be updated: %s. Check file permissions."), *TargetPath);
            }
            return FReply::Handled();
        }
        case ECodexConfigState::ConflictingEntry:
            SetupMessage = FString::Printf(TEXT("The existing Unreal MCP entry points to a different address and was not changed. Expected: %s  |  File: %s"), *ExpectedUrl, *TargetPath);
            return FReply::Handled();
        default:
            SetupMessage = FString::Printf(TEXT("The existing Codex configuration could not be read and was not changed: %s"), *TargetPath);
            return FReply::Handled();
        }
    }
    if (bAlreadyExists && !IsValidJsonFile(TargetPath))
    {
        SetupMessage = FString::Printf(TEXT("Existing configuration contains invalid JSON and was not changed: %s. Repair or back up the file before trying again."), *TargetPath);
        return FReply::Handled();
    }
    const bool bWritten = UE::ModelContextProtocol::WriteClientConfiguration(ClientFromName(*SelectedClient), UE::ModelContextProtocol::GetServerPortNumber(), UE::ModelContextProtocol::GetServerUrlPath(), FPaths::ProjectDir());
    if (bWritten) LastGeneratedPath = TargetPath;
    SetupMessage = bWritten
        ? FString::Printf(TEXT("Setup complete. %s configuration %s at %s"), **SelectedClient, bAlreadyExists ? TEXT("updated") : TEXT("created"), *TargetPath)
        : FString::Printf(TEXT("Configuration could not be written: %s. Check file permissions and the Output Log."), *TargetPath);
    return FReply::Handled();
}

FReply SMCPPreflightPanel::CopyEndpoint() { FPlatformApplicationMisc::ClipboardCopy(*GetEndpoint()); SetupMessage = TEXT("MCP address copied to the clipboard."); return FReply::Handled(); }
FReply SMCPPreflightPanel::OpenProjectFolder() { FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())); return FReply::Handled(); }

TSharedRef<SWidget> SMCPPreflightPanel::MakeClientWidget(TSharedPtr<FString> Item) const { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); }
void SMCPPreflightPanel::OnClientSelected(TSharedPtr<FString> Item, ESelectInfo::Type)
{
    SelectedClient = Item;
    LastGeneratedPath.Reset();
    if (SelectedClientText.IsValid() && Item.IsValid()) SelectedClientText->SetText(FText::FromString(*Item));
    if (Item.IsValid() && *Item == TEXT("Custom...")) SetupMessage = TEXT("Enter a custom client command. Automatic configuration generation is available only for supported clients in UE 5.8.");
}

void SMCPPreflightPanel::OnCustomCommandChanged(const FText& Text) { CustomCommand = Text.ToString().TrimStartAndEnd(); }

FText SMCPPreflightPanel::GetPrimaryActionText() const
{
    if (!IsServerRunning()) return LOCTEXT("PrimaryStart", "Start MCP Server");
    if (State != EState::Passed) return LOCTEXT("PrimaryVerify", "Verify MCP Connection");
    if (SelectedClient.IsValid() && *SelectedClient == TEXT("Custom...")) return LOCTEXT("PrimaryCustom", "Custom Client Selected");
    if (LastGeneratedPath.IsEmpty()) return LOCTEXT("PrimaryGenerate", "Generate Client Configuration");
    return LOCTEXT("PrimaryComplete", "Setup Complete");
}

FReply SMCPPreflightPanel::RunPrimaryAction()
{
    if (!IsServerRunning()) return StartServer();
    if (State != EState::Passed) return RunPreflight();
    if (SelectedClient.IsValid() && *SelectedClient == TEXT("Custom..."))
    {
        SetupMessage = TEXT("Copy the MCP address and follow the custom client's instructions to add an HTTP MCP server.");
        return FReply::Handled();
    }
    if (LastGeneratedPath.IsEmpty()) return GenerateConfiguration();
    return OpenProjectFolder();
}

void SMCPPreflightPanel::RebuildResults()
{
    if (!ResultsBox.IsValid()) return;
    ResultsBox->ClearChildren();
    if (State == EState::NotRun || State == EState::Running)
    {
        ResultsBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(State == EState::Running ? LOCTEXT("RunningBody", "Contacting Unreal MCP and discovering tools...") : LOCTEXT("NotRunBody", "Not verified yet." )).ColorAndOpacity(FSlateColor::UseSubduedForeground())];
        return;
    }
    ResultsBox->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[SNew(STextBlock).Text(FText::Format(LOCTEXT("ServerSummary", "Handshake: {0}  |  Registered MCP tools: {1}"), Context.bInitializeSucceeded ? LOCTEXT("Success", "Successful") : LOCTEXT("Failed", "Failed"), FText::AsNumber(Context.ToolNames.Num())))];
    for (const FMCPDiagnostic& D : Diagnostics)
    {
        const FLinearColor Color = D.Severity == EMCPDiagnosticSeverity::Error ? FLinearColor(0.9f, 0.2f, 0.15f) : D.Severity == EMCPDiagnosticSeverity::Warning ? FLinearColor(1.f, 0.65f, 0.1f) : FLinearColor(0.2f, 0.75f, 0.35f);
        ResultsBox->AddSlot().AutoHeight().Padding(0, 3)[SNew(STextBlock).Text(FText::FromString(D.Summary + (D.Action.IsEmpty() ? FString() : TEXT(" — ") + D.Action))).AutoWrapText(true).ColorAndOpacity(Color)];
    }
}

FText SMCPPreflightPanel::GetStatusText() const { return FText::GetEmpty(); }
FSlateColor SMCPPreflightPanel::GetStatusColor() const { return FSlateColor::UseForeground(); }
bool SMCPPreflightPanel::CanRun() const { return Client.IsValid() && !Client->IsRunning(); }

#undef LOCTEXT_NAMESPACE
