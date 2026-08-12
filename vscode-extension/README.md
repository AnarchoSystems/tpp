# tpp Template Language Support

Language support for `.tpp` template files used by the `tpp` toolchain.

Documentation for using `tpp` itself can be found [here](https://github.com/AnarchoSystems/tpp/blob/main/docs/usage.md).

## What the extension provides

- Diagnostics and editor features backed by `tpp-lsp`.
- Render preview commands for template output inspection.
- `tpp-config.json` schema validation.
- Bundled language-server binaries for supported release platforms.

## Bundled platforms

Release builds of the extension currently bundle `tpp-lsp` for:

- `linux-x64`
- `linux-arm64`
- `darwin-x64`
- `darwin-arm64`
- `win32-x64`

If no bundled binary matches the local environment, the extension falls back to:

1. `tpp.lspServerPath`, if configured.
2. `build/bin/tpp-lsp` in the active workspace.

## Commands

- `tpp: Open Render Preview`
- `tpp: Toggle Preview Mode`

## Settings

- `tpp.lspServerPath`: Override the language server binary path.
- `tpp.trace.server`: Trace LSP communication in the output panel.
- `tpp.previewMode`: Choose `editor` or `webview` preview behavior.

## Source and issues

- Repository: https://github.com/AnarchoSystems/tpp
- Issues: https://github.com/AnarchoSystems/tpp/issues