#include "SMCPPreflightPanel.h"
#include "MCPPreflightClient.h"
#include "MCPPreflightSettings.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolClientConfig.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMCPPreflightPanel"

namespace
{
FText ReadyText(bool bReady) { return bReady ? LOCTEXT("Ready", "Ready") : LOCTEXT("Missing", "Missing"); }
FSlateColor ReadyColor(bool bReady) { return bReady ? FLinearColor(0.2f, 0.75f, 0.35f) : FLinearColor(0.9f, 0.2f, 0.15f); }

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
    if (Name == TEXT("Cursor")) return FPaths::Combine(FPaths::ProjectDir(), TEXT(".cursor/mcp.json"));
    if (Name == TEXT("VS Code")) return FPaths::Combine(FPaths::ProjectDir(), TEXT(".vscode/mcp.json"));
    if (Name == TEXT("Gemini")) return FPaths::Combine(FPaths::ProjectDir(), TEXT(".gemini/settings.json"));
    if (Name == TEXT("Codex")) return FPaths::Combine(FPaths::ProjectDir(), TEXT(".codex/config.toml"));
    return FPaths::Combine(FPaths::ProjectDir(), TEXT(".mcp.json"));
}
}

void SMCPPreflightPanel::Construct(const FArguments&)
{
    Client = MakeShared<FMCPPreflightClient>();
    for (const TCHAR* Name : { TEXT("Codex"), TEXT("Claude Code"), TEXT("Cursor"), TEXT("VS Code"), TEXT("Gemini") }) ClientOptions.Add(MakeShared<FString>(Name));
    SelectedClient = ClientOptions[0];

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
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("NativeMCP", "Unreal MCP"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("Enabled", "Enabled")).ColorAndOpacity(ReadyColor(true))]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("AllToolsets", "All Toolsets + Toolset Registry"))] + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("Enabled2", "Enabled")).ColorAndOpacity(ReadyColor(true))]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 0)
                    [SNew(STextBlock).Text(LOCTEXT("DependencyHint", "These are enabled as dependencies of this setup assistant. A restart may be requested when the assistant is first installed.")).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
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

bool SMCPPreflightPanel::CanGenerateConfiguration() const { return State == EState::Passed && SelectedClient.IsValid(); }

FReply SMCPPreflightPanel::GenerateConfiguration()
{
    if (!SelectedClient.IsValid()) return FReply::Handled();
    const bool bWritten = UE::ModelContextProtocol::WriteClientConfiguration(ClientFromName(*SelectedClient), UE::ModelContextProtocol::GetServerPortNumber(), UE::ModelContextProtocol::GetServerUrlPath(), FPaths::ProjectDir());
    LastGeneratedPath = ClientConfigPath(*SelectedClient);
    SetupMessage = bWritten ? FString::Printf(TEXT("Setup complete. %s configuration created at %s"), **SelectedClient, *LastGeneratedPath) : FString::Printf(TEXT("Configuration was not written. The file may already exist: %s"), *LastGeneratedPath);
    return FReply::Handled();
}

FReply SMCPPreflightPanel::CopyEndpoint() { FPlatformApplicationMisc::ClipboardCopy(*GetEndpoint()); SetupMessage = TEXT("MCP address copied to the clipboard."); return FReply::Handled(); }
FReply SMCPPreflightPanel::OpenProjectFolder() { FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())); return FReply::Handled(); }

TSharedRef<SWidget> SMCPPreflightPanel::MakeClientWidget(TSharedPtr<FString> Item) const { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); }
void SMCPPreflightPanel::OnClientSelected(TSharedPtr<FString> Item, ESelectInfo::Type) { SelectedClient = Item; if (SelectedClientText.IsValid() && Item.IsValid()) SelectedClientText->SetText(FText::FromString(*Item)); }

FText SMCPPreflightPanel::GetPrimaryActionText() const
{
    if (!IsServerRunning()) return LOCTEXT("PrimaryStart", "Start MCP Server");
    if (State != EState::Passed) return LOCTEXT("PrimaryVerify", "Verify MCP Connection");
    if (LastGeneratedPath.IsEmpty()) return LOCTEXT("PrimaryGenerate", "Generate Client Configuration");
    return LOCTEXT("PrimaryComplete", "Setup Complete");
}

FReply SMCPPreflightPanel::RunPrimaryAction()
{
    if (!IsServerRunning()) return StartServer();
    if (State != EState::Passed) return RunPreflight();
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
