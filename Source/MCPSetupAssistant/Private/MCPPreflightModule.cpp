#include "MCPPreflightModule.h"
#include "SMCPPreflightPanel.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "MCPPreflightModule"

const FName FMCPPreflightModule::TabName(TEXT("MCPPreflight"));

void FMCPPreflightModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateRaw(this, &FMCPPreflightModule::SpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Unreal MCP Setup"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Set up, verify, and configure Unreal's native MCP server."));
}

void FMCPPreflightModule::ShutdownModule()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

TSharedRef<SDockTab> FMCPPreflightModule::SpawnTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SMCPPreflightPanel)];
}

IMPLEMENT_MODULE(FMCPPreflightModule, MCPSetupAssistant)

#undef LOCTEXT_NAMESPACE
