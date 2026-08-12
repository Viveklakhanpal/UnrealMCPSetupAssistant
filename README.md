# Unreal MCP Setup Assistant v0.1

An Unreal Engine 5.8 Editor plugin that turns the native MCP first-run workflow into a guided setup.

## Use

1. Enable **Unreal MCP Setup Assistant** in Edit > Plugins and restart when prompted. Its dependencies enable Unreal MCP, All Toolsets, and the Toolset Registry.
2. Open **Tools > Unreal MCP Setup**.
3. Start the native server (and optionally enable auto-start).
4. Verify the MCP handshake and toolset discovery.
5. Choose an MCP client and generate Epic's official project configuration.

The assistant uses UE 5.8's native server settings and APIs. Verification sends `initialize`, `notifications/initialized`, and `tools/list`, preserves the `Mcp-Session-Id`, and checks the toolset discovery surface. Configuration generation is delegated to Epic's native implementation for Codex, Claude Code, Cursor, VS Code, and Gemini.
