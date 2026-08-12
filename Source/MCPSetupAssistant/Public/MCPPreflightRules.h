#pragma once

#include "CoreMinimal.h"

enum class EMCPDiagnosticSeverity : uint8 { Info, Warning, Error };

struct FMCPDiagnostic
{
    EMCPDiagnosticSeverity Severity = EMCPDiagnosticSeverity::Info;
    FString RuleId;
    FString Summary;
    FString Detail;
    FString Action;
};

struct FMCPPreflightContext
{
    FString EndpointUrl;
    bool bTransportSucceeded = false;
    int32 HttpStatus = 0;
    bool bInitializeSucceeded = false;
    bool bToolsListSucceeded = false;
    int32 ToolsListHttpStatus = 0;
    FString ServerName;
    FString ServerVersion;
    TArray<FString> ToolNames;
};

class IMCPPreflightRule
{
public:
    virtual ~IMCPPreflightRule() = default;
    virtual void Evaluate(const FMCPPreflightContext& Context, TArray<FMCPDiagnostic>& OutDiagnostics) const = 0;
};

class FMCPPreflightRuleRegistry
{
public:
    FMCPPreflightRuleRegistry();
    void Evaluate(const FMCPPreflightContext& Context, TArray<FMCPDiagnostic>& OutDiagnostics) const;

private:
    TArray<TSharedRef<IMCPPreflightRule>> Rules;
};
