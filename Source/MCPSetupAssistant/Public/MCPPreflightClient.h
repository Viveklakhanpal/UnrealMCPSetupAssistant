#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "MCPPreflightRules.h"

DECLARE_DELEGATE_OneParam(FMCPPreflightComplete, const FMCPPreflightContext&);

class FMCPPreflightClient : public TSharedFromThis<FMCPPreflightClient>
{
public:
    void Run(const FString& Endpoint, float TimeoutSeconds, FMCPPreflightComplete Completion);
    void Cancel();
    bool IsRunning() const { return bRunning; }

private:
    void SendInitialize();
    void OnInitialize(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    void SendInitialized();
    void SendToolsList();
    void OnToolsList(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> NewRequest(const FString& JsonBody) const;
    void Finish();

    FString EndpointUrl;
    FString SessionId;
    float Timeout = 10.0f;
    bool bRunning = false;
    FMCPPreflightContext Context;
    FMCPPreflightComplete OnComplete;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
};
