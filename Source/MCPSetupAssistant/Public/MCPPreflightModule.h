#pragma once

#include "Modules/ModuleManager.h"

class FMCPPreflightModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<class SDockTab> SpawnTab(const class FSpawnTabArgs& Args);
    static const FName TabName;
};
