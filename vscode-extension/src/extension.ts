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
const DEFAULT_LSP_PATH = '';
type PreviewMode = 'editor' | 'webview';

function getBundledLanguageServerPlatformCandidates(): string[] {
  return [
    `${process.platform}-${process.arch}`,
    process.platform
  ];
}

function getLanguageServerBinaryName(): string {
  return process.platform === 'win32' ? 'tpp-lsp.exe' : 'tpp-lsp';
}

function getBundledLanguageServerCandidates(context: vscode.ExtensionContext): string[] {
  const binaryName = getLanguageServerBinaryName();
  return getBundledLanguageServerPlatformCandidates().map(platformFolder =>
    context.asAbsolutePath(path.join('resources', 'lsp', platformFolder, binaryName))
  );
}

function resolveWorkspaceRelativePath(candidatePath: string): string {
  if (path.isAbsolute(candidatePath)) {
    return candidatePath;
  }

  const workspaceFolders = vscode.workspace.workspaceFolders;
  if (workspaceFolders && workspaceFolders.length > 0) {
    return path.join(workspaceFolders[0].uri.fsPath, candidatePath);
  }

  return path.resolve(candidatePath);
}

function resolveLanguageServerPath(context: vscode.ExtensionContext): { path: string; source: string } | null {
  const config = vscode.workspace.getConfiguration('tpp');
  const configuredLspPath = config.get<string>('lspServerPath')?.trim();
  const explicitPath = configuredLspPath && configuredLspPath.length > 0
    ? resolveWorkspaceRelativePath(configuredLspPath)
    : undefined;

  const candidates: Array<{ path: string; source: string }> = [];

  if (explicitPath) {
    candidates.push({ path: explicitPath, source: 'configured setting' });
  }

  for (const bundledPath of getBundledLanguageServerCandidates(context)) {
    candidates.push({ path: bundledPath, source: 'bundled extension asset' });
  }

  const workspaceBinaryPath = resolveWorkspaceRelativePath(path.join('build', 'bin', getLanguageServerBinaryName()));
  candidates.push({ path: workspaceBinaryPath, source: 'workspace build output' });

  for (const candidate of candidates) {
    if (fs.existsSync(candidate.path)) {
      return candidate;
    }
  }

  return null;
}

function getCurrentPreviewMode(): PreviewMode {
  const configuredMode = vscode.workspace.getConfiguration('tpp').get<string>('previewMode');
  return configuredMode === 'webview' ? 'webview' : 'editor';
}

async function updatePreviewMode(nextMode: PreviewMode): Promise<void> {
  const configuration = vscode.workspace.getConfiguration('tpp');
  const inspected = configuration.inspect<string>('previewMode');

  if (inspected?.workspaceFolderValue !== undefined && vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
    await configuration.update('previewMode', nextMode, vscode.ConfigurationTarget.WorkspaceFolder);
    return;
  }

  if (inspected?.workspaceValue !== undefined) {
    await configuration.update('previewMode', nextMode, vscode.ConfigurationTarget.Workspace);
    return;
  }

  await configuration.update('previewMode', nextMode, vscode.ConfigurationTarget.Global);
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  outputChannel = vscode.window.createOutputChannel('tpp Language Server');
  outputChannel.appendLine('[tpp] Extension activating...');

  // ── Resolve LSP binary path ────────────────────────────────────────────────
  const resolvedLanguageServer = resolveLanguageServerPath(context);
  if (!resolvedLanguageServer) {
    const bundledCandidates = getBundledLanguageServerCandidates(context).join(', ');
    const workspaceBinaryPath = resolveWorkspaceRelativePath(path.join('build', 'bin', getLanguageServerBinaryName()));
    outputChannel!.appendLine('[tpp] ERROR: No language server binary found.');
    outputChannel!.appendLine(`[tpp] Checked bundled paths: ${bundledCandidates}`);
    outputChannel!.appendLine(`[tpp] Checked workspace path: ${workspaceBinaryPath}`);
    vscode.window.showWarningMessage(
      'tpp: No language server binary was found. Install a packaged extension build or build tpp-lsp in the workspace.'
    );
    return;
  }

  const lspPath = resolvedLanguageServer.path;
  outputChannel!.appendLine(`[tpp] LSP binary path: ${lspPath}`);
  outputChannel!.appendLine(`[tpp] LSP source: ${resolvedLanguageServer.source}`);

  // ── Start Language Client ─────────────────────────────────────────────────
  const serverOptions: ServerOptions = {
    run:   { command: lspPath, transport: TransportKind.stdio },
    debug: { command: lspPath, transport: TransportKind.stdio }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'tpp' }
    ],
    synchronize: {
      fileEvents: [
        vscode.workspace.createFileSystemWatcher('**/*.tpp'),
        vscode.workspace.createFileSystemWatcher('**/*.json')
      ]
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
      await PreviewPanel.createOrShow(context, client!, configs[0], 0);
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
          await PreviewPanel.createOrShow(context, client!, cfg, i);
          return;
        }
      }
    }
  });

  const togglePreviewModeCmd = vscode.commands.registerCommand('tpp.togglePreviewMode', async () => {
    const currentMode = getCurrentPreviewMode();
    const nextMode: PreviewMode = currentMode === 'editor' ? 'webview' : 'editor';
    await updatePreviewMode(nextMode);
    await PreviewPanel.reopenCurrent();
    vscode.window.showInformationMessage(`tpp: Preview mode switched to ${nextMode}.`);
  });

  const previewModeChangeDisposable = vscode.workspace.onDidChangeConfiguration(async e => {
    if (!e.affectsConfiguration('tpp.previewMode')) {
      return;
    }

    await PreviewPanel.reopenCurrent();
  });

  // ── Track cursor for preview highlight ────────────────────────────────────
  const cursorChangeDisposable = vscode.window.onDidChangeTextEditorSelection(e => {
    if (PreviewPanel.currentPanel) {
      const document = e.textEditor.document;
      const isTemplateDocument = document.languageId === 'tpp' ||
        document.uri.path.endsWith('.tpp');

      if (!isTemplateDocument || e.selections.length === 0) {
        PreviewPanel.currentPanel.clearHighlights();
        return;
      }

      PreviewPanel.currentPanel.onSelectionChange(document.uri.toString(true), e.selections);
    }
  });

  context.subscriptions.push(openPreviewCmd, togglePreviewModeCmd, previewModeChangeDisposable, cursorChangeDisposable);
}

export async function deactivate(): Promise<void> {
  if (client) {
    await client.stop();
  }
}
