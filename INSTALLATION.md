# Installation — v0.1.0

**Tutorial Link:** [MCP Setup Assistant — video installation guide](https://youtu.be/gK2hESlAus0)

## Windows: ready-to-install download

Use `UnrealMCPSetupAssistant-0.1.0-win64.zip`. This includes the precompiled plugin and its source code.

1. Close Unreal Editor and back up your project before installing or upgrading.
2. Extract the ZIP. Copy its inner `UnrealMCPSetupAssistant` folder into your project's `Plugins` folder; create `Plugins` if it does not exist.
3. Confirm the path is `YourProject/Plugins/UnrealMCPSetupAssistant/UnrealMCPSetupAssistant.uplugin`. Avoid an extra nested folder or duplicate installations.
4. Reopen the project. In **Edit > Plugins**, enable **Unreal MCP Setup Assistant**, then restart if requested.
5. Open **Tools > Unreal MCP Setup**. Start the server, verify MCP, select your client, and generate its configuration.

The assistant declares the required Unreal components as dependencies. It does not install an AI client or sign in for you.

### Optional Terminal workflow

To launch a command-line client inside Unreal Terminal, use the assistant's Terminal guidance. Copy each displayed command into a separate **Startup Commands** array entry, in the displayed order, then reopen Terminal. The assistant does not modify these settings for you.

GUI clients can use the project workspace and generated configuration without Unreal Terminal. Follow the client's prompts to enable or approve the MCP connection where required.

**Verify MCP checks Unreal's server and advertised tools, not your AI client's sign-in or connection.** Node.js is only needed by some clients or installation methods, not by Unreal MCP itself.

### If Unreal asks to rebuild

- Confirm you downloaded the **win64** ZIP, not the source ZIP.
- This binary was built with UE 5.8.0, BuildId `55116800`, and tested on Windows. Other patches and custom engine builds may need a matching binary; UE 5.8.2 compatibility has not been verified.
- Do not change engine version metadata to bypass this warning. Use a compatible package or compile from source with the appropriate toolchain.
- Include your exact engine version, operating system, and error message when reporting an issue.

## Developers: source-code download

Use `UnrealMCPSetupAssistant-0.1.0-source.zip`. It intentionally contains no precompiled DLLs.

1. Install Unreal Engine 5.8 and its supported C++ compiler/toolchain. On Windows, use the Visual Studio C++/Unreal development components and Windows SDK supported by your engine installation.
2. Extract `UnrealMCPSetupAssistant` into a C++ project's `Plugins` directory.
3. Regenerate project files and build the project's **Development Editor / Win64** target for Windows.
4. Open the project, enable the plugin, and restart if requested.

Unreal's `BuildPlugin` automation can also build a distributable plugin from the `.uplugin` descriptor. See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

macOS and Linux are not yet validated. Source availability does not mean those platforms are officially supported in v0.1.0.

Epic's native experimental engine plugins are required but are not bundled. Fab submission artifacts are maintained separately from these GitHub downloads.
