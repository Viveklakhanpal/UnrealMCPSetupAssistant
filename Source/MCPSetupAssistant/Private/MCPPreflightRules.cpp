#include "MCPPreflightRules.h"
#include "GenericPlatform/GenericPlatformHttp.h"

namespace
{
void Add(TArray<FMCPDiagnostic>& Out, EMCPDiagnosticSeverity Severity, const TCHAR* Id, const FString& Summary, const FString& Detail, const FString& Action = {})
{
    Out.Add({Severity, Id, Summary, Detail, Action});
}

class FEndpointRule final : public IMCPPreflightRule
{
public:
    virtual void Evaluate(const FMCPPreflightContext& C, TArray<FMCPDiagnostic>& Out) const override
    {
        const FString Lower = C.EndpointUrl.ToLower();
        if (!(Lower.StartsWith(TEXT("http://")) || Lower.StartsWith(TEXT("https://"))))
        {
            Add(Out, EMCPDiagnosticSeverity::Error, TEXT("endpoint.scheme"), TEXT("Endpoint is not an HTTP URL"), C.EndpointUrl, TEXT("Use an http:// or https:// Streamable HTTP MCP endpoint."));
        }
        else if (!(Lower.Contains(TEXT("127.0.0.1")) || Lower.Contains(TEXT("localhost")) || Lower.Contains(TEXT("[::1]"))))
        {
            Add(Out, EMCPDiagnosticSeverity::Warning, TEXT("endpoint.remote"), TEXT("Endpoint is not loopback"), C.EndpointUrl, TEXT("Confirm the remote server is trusted and uses TLS/authentication."));
        }
    }
};

class FTransportRule final : public IMCPPreflightRule
{
public:
    virtual void Evaluate(const FMCPPreflightContext& C, TArray<FMCPDiagnostic>& Out) const override
    {
        if (!C.bTransportSucceeded)
            Add(Out, EMCPDiagnosticSeverity::Error, TEXT("transport.unreachable"), TEXT("MCP server could not be reached"), TEXT("No usable HTTP response was received."), TEXT("Start the server, verify its port, and check the endpoint in Project Settings > Plugins > MCP Preflight."));
        else if (C.HttpStatus < 200 || C.HttpStatus >= 300)
            Add(Out, EMCPDiagnosticSeverity::Error, TEXT("transport.http_status"), FString::Printf(TEXT("Server returned HTTP %d"), C.HttpStatus), TEXT("The endpoint responded but rejected the MCP initialize request."), TEXT("Verify that the URL points to the Streamable HTTP MCP route."));
    }
};

class FProtocolRule final : public IMCPPreflightRule
{
public:
    virtual void Evaluate(const FMCPPreflightContext& C, TArray<FMCPDiagnostic>& Out) const override
    {
        if (C.bTransportSucceeded && C.HttpStatus >= 200 && C.HttpStatus < 300 && !C.bInitializeSucceeded)
            Add(Out, EMCPDiagnosticSeverity::Error, TEXT("protocol.initialize"), TEXT("Invalid MCP initialize response"), TEXT("The response did not contain a valid JSON-RPC result."), TEXT("Check server logs and MCP protocol compatibility."));
        if (C.bInitializeSucceeded && C.ToolNames.IsEmpty())
            Add(Out, EMCPDiagnosticSeverity::Warning, TEXT("tools.empty"), TEXT("No tools were discovered"), TEXT("The server initialized successfully but tools/list returned no tools."));

        if (C.bInitializeSucceeded && !C.ToolNames.IsEmpty())
        {
            bool bHasListToolsets = false, bHasDescribeToolset = false, bHasCallTool = false;
            for (const FString& Name : C.ToolNames)
            {
                bHasListToolsets |= Name.Equals(TEXT("list_toolsets"), ESearchCase::IgnoreCase);
                bHasDescribeToolset |= Name.Equals(TEXT("describe_toolset"), ESearchCase::IgnoreCase);
                bHasCallTool |= Name.Equals(TEXT("call_tool"), ESearchCase::IgnoreCase);
            }
            if (!(bHasListToolsets && bHasDescribeToolset && bHasCallTool))
                Add(Out, EMCPDiagnosticSeverity::Warning, TEXT("toolsets.discovery"), TEXT("Toolset discovery is incomplete"), TEXT("One or more of list_toolsets, describe_toolset, and call_tool were not advertised."), TEXT("Confirm All Toolsets is enabled, then restart Unreal and verify again."));
        }

        TSet<FString> Seen;
        for (const FString& Name : C.ToolNames)
        {
            if (Name.TrimStartAndEnd().IsEmpty())
                Add(Out, EMCPDiagnosticSeverity::Error, TEXT("tools.name.empty"), TEXT("A tool has no name"), TEXT("Every MCP tool must have a non-empty name."));
            else if (Seen.Contains(Name))
                Add(Out, EMCPDiagnosticSeverity::Warning, TEXT("tools.name.duplicate"), TEXT("Duplicate tool name"), Name, TEXT("Give each registered tool a unique name."));
            Seen.Add(Name);
        }
    }
};
}

FMCPPreflightRuleRegistry::FMCPPreflightRuleRegistry()
{
    Rules.Add(MakeShared<FEndpointRule>());
    Rules.Add(MakeShared<FTransportRule>());
    Rules.Add(MakeShared<FProtocolRule>());
}

void FMCPPreflightRuleRegistry::Evaluate(const FMCPPreflightContext& Context, TArray<FMCPDiagnostic>& OutDiagnostics) const
{
    OutDiagnostics.Reset();
    for (const TSharedRef<IMCPPreflightRule>& Rule : Rules) Rule->Evaluate(Context, OutDiagnostics);
    if (OutDiagnostics.IsEmpty() && Context.bInitializeSucceeded)
        Add(OutDiagnostics, EMCPDiagnosticSeverity::Info, TEXT("preflight.ok"), TEXT("Preflight passed"), TEXT("Connection, initialization, and tool discovery completed without findings."));
}
