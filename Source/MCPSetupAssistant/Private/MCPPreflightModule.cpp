#include "MCPPreflightModule.h"
#include "SMCPPreflightPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "MCPPreflightModule"

const FName FMCPPreflightModule::TabName(TEXT("MCPPreflight"));

void FMCPPreflightModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateRaw(this, &FMCPPreflightModule::SpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Unreal MCP Setup"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Set up, verify, and configure Unreal's native MCP server."));
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMCPPreflightModule::RegisterMenus));
}

void FMCPPreflightModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FMCPPreflightModule::RegisterMenus()
{
    FToolMenuOwnerScoped Owner(this);
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
    FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Programming"));
    Section.AddMenuEntry(TEXT("OpenMCPPreflight"), LOCTEXT("MenuLabel", "Unreal MCP Setup"), LOCTEXT("MenuTooltip", "Open the guided Unreal MCP setup assistant."), FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(TabName); })));
}

TSharedRef<SDockTab> FMCPPreflightModule::SpawnTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SMCPPreflightPanel)];
}

IMPLEMENT_MODULE(FMCPPreflightModule, MCPSetupAssistant)

#undef LOCTEXT_NAMESPACE
