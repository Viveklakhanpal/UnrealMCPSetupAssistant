# Unreal MCP Setup Assistant

An open-source Unreal Engine 5.8 Editor plugin that turns Epic's native MCP first-run workflow into a guided setup.

## Requirements

- Unreal Engine 5.8 with Epic's experimental Unreal MCP, Toolset Registry, All Toolsets, and Terminal plugins.
- A supported AI client if you want the assistant to generate client configuration.
- Some command-line AI clients require Node.js when launched from Unreal Terminal.

Epic's native experimental plugins are engine components and are not included in this repository. The setup assistant declares them as dependencies so Unreal can enable them with the assistant; the first activation may require an editor restart.

## Installation

Copy the `UnrealMCPSetupAssistant` folder into your project's `Plugins` directory, enable **Unreal MCP Setup Assistant** under **Edit > Plugins**, and restart Unreal Editor when requested.

## Use

1. Enable **Unreal MCP Setup Assistant** and restart when prompted. Its dependencies enable Unreal MCP, Toolset Registry, All Toolsets, and Unreal Terminal.
2. Open **Tools > Unreal MCP Setup**.
3. Start the native server (and optionally enable auto-start).
4. Verify the MCP handshake and toolset discovery.
5. Detect or choose an MCP client and generate its project configuration.

The assistant uses UE 5.8's native server settings and APIs. Verification sends `initialize`, `notifications/initialized`, and `tools/list`, preserves the `Mcp-Session-Id`, and checks the toolset discovery surface.

**Verify MCP checks Unreal's server and advertised tools. It does not verify that an AI client is signed in or that the selected client can connect to Unreal.**

## Supported clients

- Codex
- Claude Code
- Cursor
- VS Code
- Gemini
- Custom command identification (manual configuration only)

Known clients and Node.js are detected from the editor process environment. Detection confirms only that a command is available; it does not validate authentication, account state, or client compatibility.

## Configuration safety

JSON client files are merged through Epic's native UE 5.8 implementation. The assistant refuses to overwrite malformed JSON. For Codex TOML, it recognizes an existing correct Unreal MCP entry, safely appends the entry when absent, and refuses to replace a conflicting `unreal-mcp` section.

## Current validation

The plugin is currently built and tested against Unreal Engine 5.8 on Windows. The implementation uses cross-platform Unreal APIs, but macOS and Linux validation remains pending.
