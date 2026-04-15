import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';
import {
  LanguageClient,
  LanguageClientOptions,
  RevealOutputChannelOn,
  ServerOptions,
  TransportKind
} from 'vscode-languageclient/node';
import { PreviewPanel } from './previewPanel';
import { findTppConfigs } from './configScanner';

let client: LanguageClient | undefined;
let outputChannel: vscode.OutputChannel | undefined;
const DEFAULT_LSP_PATH = 'build/bin/tpp-lsp';

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  outputChannel = vscode.window.createOutputChannel('tpp Language Server');
  outputChannel.appendLine('[tpp] Extension activating...');

  // ── Resolve LSP binary path ────────────────────────────────────────────────
  const config = vscode.workspace.getConfiguration('tpp');
  const configuredLspPath = config.get<string>('lspServerPath');
  let lspPath = configuredLspPath?.trim() || DEFAULT_LSP_PATH;

  if (!lspPath) {
    lspPath = DEFAULT_LSP_PATH;
  }

 if (!path.isAbsolute(lspPath)) {
    // Workspace-relative
    const folders = vscode.workspace.workspaceFolders;
    if (folders && folders.length > 0) {
      lspPath = path.join(folders[0].uri.fsPath, lspPath);
    }
  }

  outputChannel!.appendLine(`[tpp] LSP binary path: ${lspPath}`);

  if (!fs.existsSync(lspPath)) {
    outputChannel!.appendLine('[tpp] ERROR: LSP binary not found.');
    vscode.window.showWarningMessage(
      `tpp: LSP binary not found at "${lspPath}". Build tpp-lsp or override the "tpp.lspServerPath" setting.`
    );
    return;
  }

  // ── Start Language Client ─────────────────────────────────────────────────
  const serverOptions: ServerOptions = {
    run:   { command: lspPath, transport: TransportKind.stdio },
    debug: { command: lspPath, transport: TransportKind.stdio }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'tpp' },
      { scheme: 'file', language: 'tpp-types' }
    ],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{tpp,tpp.types}')
    },
    outputChannel: outputChannel,
    revealOutputChannelOn: RevealOutputChannelOn.Error
  };

  outputChannel!.appendLine('[tpp] Starting language client...');
  client = new LanguageClient('tpp', 'tpp Language Server', serverOptions, clientOptions);
  try {
    await client.start();
    outputChannel!.appendLine('[tpp] Language client started successfully.');
  } catch (e) {
    outputChannel!.appendLine(`[tpp] ERROR: Language client failed to start: ${e}`);
    vscode.window.showErrorMessage(`tpp: Language server failed to start: ${e}`);
    return;
  }

  // ── Register preview command ───────────────────────────────────────────────
  const openPreviewCmd = vscode.commands.registerCommand('tpp.openPreview', async () => {
    const folders = vscode.workspace.workspaceFolders ?? [];
    const configs = await findTppConfigs(folders);

    if (configs.length === 0) {
      vscode.window.showInformationMessage('tpp: No tpp-config.json found in workspace.');
      return;
    }

    // If there's only one config with one preview, open it directly.
    if (configs.length === 1 && configs[0].previews && configs[0].previews.length === 1) {
      PreviewPanel.createOrShow(context, client!, configs[0], 0);
      return;
    }

    // Otherwise, present a quick-pick.
    const items: vscode.QuickPickItem[] = [];
    for (const cfg of configs) {
      if (!cfg.previews) continue;
      for (let i = 0; i < cfg.previews.length; i++) {
        const previewName = cfg.previews[i].name || cfg.previews[i].template;
        const folderName = path.basename(path.dirname(cfg.configPath));
        items.push({
          label: `${folderName}/${previewName}`,
          detail: cfg.configPath,
        });
      }
    }
    if (items.length === 0) {
      vscode.window.showInformationMessage('tpp: No previews defined in tpp-config.json.');
      return;
    }

    const picked = await vscode.window.showQuickPick(items, { title: 'Select tpp Preview' });
    if (!picked) return;

    // Find the config + index
    for (const cfg of configs) {
      if (!cfg.previews) continue;
      for (let i = 0; i < cfg.previews.length; i++) {
        const previewName = cfg.previews[i].name || cfg.previews[i].template;
        const folderName = path.basename(path.dirname(cfg.configPath));
        if (`${folderName}/${previewName}` === picked.label) {
          PreviewPanel.createOrShow(context, client!, cfg, i);
          return;
        }
      }
    }
  });

  // ── Track cursor for preview highlight ────────────────────────────────────
  const cursorChangeDisposable = vscode.window.onDidChangeTextEditorSelection(e => {
    if (PreviewPanel.currentPanel) {
      const pos = e.selections[0]?.active;
      if (pos) PreviewPanel.currentPanel.onCursorMove(e.textEditor.document.uri.toString(), pos);
    }
  });

  context.subscriptions.push(openPreviewCmd, cursorChangeDisposable);
}

export async function deactivate(): Promise<void> {
  if (client) {
    await client.stop();
  }
}
