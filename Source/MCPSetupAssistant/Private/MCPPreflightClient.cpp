#include "MCPPreflightClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FJsonObject> ParseJsonRpc(const FString& Body)
{
    FString Json = Body;
    // Streamable HTTP may return a single SSE data event.
    if (Body.Contains(TEXT("data:")))
    {
        TArray<FString> Lines;
        Body.ParseIntoArrayLines(Lines);
        for (const FString& Line : Lines)
            if (Line.StartsWith(TEXT("data:"))) { Json = Line.Mid(5).TrimStartAndEnd(); break; }
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
}
}

void FMCPPreflightClient::Run(const FString& Endpoint, float TimeoutSeconds, FMCPPreflightComplete Completion)
{
    Cancel();
    EndpointUrl = Endpoint.TrimStartAndEnd();
    SessionId.Reset();
    Timeout = TimeoutSeconds;
    OnComplete = MoveTemp(Completion);
    Context = {};
    Context.EndpointUrl = EndpointUrl;
    bRunning = true;
    SendInitialize();
}

void FMCPPreflightClient::Cancel()
{
    if (ActiveRequest.IsValid()) ActiveRequest->CancelRequest();
    ActiveRequest.Reset();
    bRunning = false;
    OnComplete.Unbind();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FMCPPreflightClient::NewRequest(const FString& JsonBody) const
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(EndpointUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json, text/event-stream"));
    if (!SessionId.IsEmpty()) Request->SetHeader(TEXT("Mcp-Session-Id"), SessionId);
    Request->SetTimeout(Timeout);
    Request->SetContentAsString(JsonBody);
    return Request;
}

void FMCPPreflightClient::SendInitialize()
{
    ActiveRequest = NewRequest(TEXT("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"unreal-mcp-preflight\",\"version\":\"0.1.0\"}}}"));
    ActiveRequest->OnProcessRequestComplete().BindSP(AsShared(), &FMCPPreflightClient::OnInitialize);
    if (!ActiveRequest->ProcessRequest()) Finish();
}

void FMCPPreflightClient::OnInitialize(FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    Context.bTransportSucceeded = bConnectedSuccessfully && Response.IsValid();
    Context.HttpStatus = Response.IsValid() ? Response->GetResponseCode() : 0;
    if (!Context.bTransportSucceeded || Context.HttpStatus < 200 || Context.HttpStatus >= 300) { Finish(); return; }
    SessionId = Response->GetHeader(TEXT("Mcp-Session-Id"));
    const TSharedPtr<FJsonObject> Root = ParseJsonRpc(Response->GetContentAsString());
    const TSharedPtr<FJsonObject>* Result = nullptr;
    Context.bInitializeSucceeded = Root.IsValid() && Root->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid();
    if (!Context.bInitializeSucceeded) { Finish(); return; }
    const TSharedPtr<FJsonObject>* ServerInfo = nullptr;
    if ((*Result)->TryGetObjectField(TEXT("serverInfo"), ServerInfo) && ServerInfo && ServerInfo->IsValid())
    {
        (*ServerInfo)->TryGetStringField(TEXT("name"), Context.ServerName);
        (*ServerInfo)->TryGetStringField(TEXT("version"), Context.ServerVersion);
    }
    SendInitialized();
}

void FMCPPreflightClient::SendInitialized()
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Notification = NewRequest(TEXT("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}"));
    Notification->OnProcessRequestComplete().BindLambda([Self = AsShared()](FHttpRequestPtr, FHttpResponsePtr, bool) { if (Self->bRunning) Self->SendToolsList(); });
    ActiveRequest = Notification;
    if (!Notification->ProcessRequest()) SendToolsList();
}

void FMCPPreflightClient::SendToolsList()
{
    ActiveRequest = NewRequest(TEXT("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}"));
    ActiveRequest->OnProcessRequestComplete().BindSP(AsShared(), &FMCPPreflightClient::OnToolsList);
    if (!ActiveRequest->ProcessRequest()) Finish();
}

void FMCPPreflightClient::OnToolsList(FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    Context.ToolsListHttpStatus = Response.IsValid() ? Response->GetResponseCode() : 0;
    if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        const TSharedPtr<FJsonObject> Root = ParseJsonRpc(Response->GetContentAsString());
        const TSharedPtr<FJsonObject>* Result = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
        if (Root.IsValid() && Root->TryGetObjectField(TEXT("result"), Result) && Result && (*Result)->TryGetArrayField(TEXT("tools"), Tools) && Tools)
        {
            Context.bToolsListSucceeded = true;
            for (const TSharedPtr<FJsonValue>& Value : *Tools)
            {
                const TSharedPtr<FJsonObject> Tool = Value->AsObject();
                FString Name;
                if (Tool.IsValid() && Tool->TryGetStringField(TEXT("name"), Name)) Context.ToolNames.Add(Name);
            }
        }
    }
    Finish();
}

void FMCPPreflightClient::Finish()
{
    ActiveRequest.Reset();
    bRunning = false;
    if (OnComplete.IsBound()) { const FMCPPreflightComplete Callback = OnComplete; OnComplete.Unbind(); Callback.Execute(Context); }
}
