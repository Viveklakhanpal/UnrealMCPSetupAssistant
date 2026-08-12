# Contributing

Thanks for helping improve Unreal MCP onboarding.

## Product boundary

The core plugin is intentionally limited to native Unreal MCP setup, server startup, connection verification, toolset discovery, and official client configuration generation. AI chat, scene editing, custom MCP servers, API keys, and client installation are out of scope.

## Development

- Use Unreal Engine 5.8.
- Keep editor logic cross-platform through Unreal APIs.
- Avoid shell- or registry-specific behavior in the core module.
- Build the `UnrealEditor` target and run `MCPSetupAssistant` automation tests before submitting a change.
- Explain user-facing behavior changes in `CHANGELOG.md`.
- Never overwrite an existing client configuration when it cannot be parsed safely.
- Distinguish server verification from AI-client authentication and connectivity in user-facing language.

Please keep pull requests focused and include reproduction steps for fixes.
