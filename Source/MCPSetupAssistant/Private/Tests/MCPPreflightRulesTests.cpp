#include "MCPPreflightRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPSetupEndpointRuleTest, "MCPSetupAssistant.Rules.InvalidEndpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMCPSetupEndpointRuleTest::RunTest(const FString&)
{
    FMCPPreflightContext Context;
    Context.EndpointUrl = TEXT("not-an-http-endpoint");
    TArray<FMCPDiagnostic> Diagnostics;
    FMCPPreflightRuleRegistry().Evaluate(Context, Diagnostics);
    TestTrue(TEXT("Invalid endpoint produces an endpoint finding"), Diagnostics.ContainsByPredicate([](const FMCPDiagnostic& D) { return D.RuleId == TEXT("endpoint.scheme"); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPSetupHealthyRuleTest, "MCPSetupAssistant.Rules.HealthyNativeServer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMCPSetupHealthyRuleTest::RunTest(const FString&)
{
    FMCPPreflightContext Context;
    Context.EndpointUrl = TEXT("http://127.0.0.1:8000/mcp");
    Context.bTransportSucceeded = true;
    Context.HttpStatus = 200;
    Context.bInitializeSucceeded = true;
    Context.bToolsListSucceeded = true;
    Context.ToolNames = { TEXT("list_toolsets"), TEXT("describe_toolset"), TEXT("call_tool") };
    TArray<FMCPDiagnostic> Diagnostics;
    FMCPPreflightRuleRegistry().Evaluate(Context, Diagnostics);
    TestTrue(TEXT("Healthy native server produces the pass finding"), Diagnostics.ContainsByPredicate([](const FMCPDiagnostic& D) { return D.RuleId == TEXT("preflight.ok"); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMCPSetupToolsListFailureTest, "MCPSetupAssistant.Rules.ToolsListFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMCPSetupToolsListFailureTest::RunTest(const FString&)
{
    FMCPPreflightContext Context;
    Context.EndpointUrl = TEXT("http://127.0.0.1:8000/mcp");
    Context.bTransportSucceeded = true;
    Context.HttpStatus = 200;
    Context.bInitializeSucceeded = true;
    Context.ToolsListHttpStatus = 500;
    TArray<FMCPDiagnostic> Diagnostics;
    FMCPPreflightRuleRegistry().Evaluate(Context, Diagnostics);
    TestTrue(TEXT("Failed tools/list produces a distinct error"), Diagnostics.ContainsByPredicate([](const FMCPDiagnostic& D) { return D.RuleId == TEXT("tools.list.failed") && D.Severity == EMCPDiagnosticSeverity::Error; }));
    return true;
}

#endif
