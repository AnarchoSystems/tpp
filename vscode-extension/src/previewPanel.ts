import * as path from 'path';
import hljs from 'highlight.js';
import * as vscode from 'vscode';
import { LanguageClient } from 'vscode-languageclient/node';
import { TppConfig } from './configScanner';

type PreviewMode = 'editor' | 'webview';

interface RenderMapping {
  sourceUri?: string;
  sourceRange: {
    start: { line: number; character: number };
    end: { line: number; character: number };
  };
  outStart: number;
  outEnd: number;
}

interface PreviewResult {
  output?: string;
  mappings?: RenderMapping[];
  language?: string;
  fileExtension?: string;
  error?: string;
}

const previewLanguageByFileExtension: Record<string, string> = {
  '.bash': 'bash',
  '.c': 'c',
  '.cc': 'cpp',
  '.cpp': 'cpp',
  '.cxx': 'cpp',
  '.css': 'css',
  '.diff': 'diff',
  '.go': 'go',
  '.h': 'cpp',
  '.hh': 'cpp',
  '.hpp': 'cpp',
  '.html': 'html',
  '.java': 'java',
  '.js': 'javascript',
  '.json': 'json',
  '.jsx': 'javascriptreact',
  '.kt': 'kotlin',
  '.kts': 'kotlin',
  '.md': 'markdown',
  '.mjs': 'javascript',
  '.patch': 'diff',
  '.py': 'python',
  '.rs': 'rust',
  '.scss': 'scss',
  '.sh': 'shellscript',
  '.sql': 'sql',
  '.svg': 'xml',
  '.swift': 'swift',
  '.ts': 'typescript',
  '.tsx': 'typescriptreact',
  '.xml': 'xml',
  '.yaml': 'yaml',
  '.yml': 'yaml',
  '.zsh': 'shellscript'
};

const previewFileExtensionByLanguage: Record<string, string> = {
  bash: '.sh',
  c: '.c',
  cpp: '.cpp',
  css: '.css',
  diff: '.diff',
  go: '.go',
  html: '.html',
  java: '.java',
  javascript: '.js',
  javascriptreact: '.jsx',
  json: '.json',
  kotlin: '.kt',
  markdown: '.md',
  plaintext: '.txt',
  python: '.py',
  rust: '.rs',
  scss: '.scss',
  shellscript: '.sh',
  sql: '.sql',
  swift: '.swift',
  typescript: '.ts',
  typescriptreact: '.tsx',
  xml: '.xml',
  yaml: '.yml'
};

function normalizePreviewLanguage(language?: string): string | undefined {
  const normalized = language?.trim().toLowerCase();
  if (!normalized) {
    return undefined;
  }

  return normalized;
}

function normalizePreviewFileExtension(fileExtension?: string): string | undefined {
  const normalized = fileExtension?.trim().toLowerCase();
  if (!normalized) {
    return undefined;
  }

  return normalized.startsWith('.') ? normalized : `.${normalized}`;
}

function resolvePreviewLanguage(languageHint?: string, fileExtensionHint?: string): string | undefined {
  const explicitLanguage = normalizePreviewLanguage(languageHint);
  if (explicitLanguage) {
    return explicitLanguage;
  }

  const normalizedExtension = normalizePreviewFileExtension(fileExtensionHint);
  return normalizedExtension ? previewLanguageByFileExtension[normalizedExtension] : undefined;
}

function resolvePreviewFileExtension(languageHint?: string, fileExtensionHint?: string): string {
  const normalizedExtension = normalizePreviewFileExtension(fileExtensionHint);
  if (normalizedExtension) {
    return normalizedExtension;
  }

  const language = normalizePreviewLanguage(languageHint);
  if (!language) {
    return '.txt';
  }

  return previewFileExtensionByLanguage[language] ?? '.txt';
}

function normalizeSourceUri(uri?: string): string | undefined {
  if (!uri) {
    return undefined;
  }

  try {
    return vscode.Uri.parse(uri).toString(true);
  } catch {
    return uri;
  }
}

function selectMostSpecificMappings(mappings: RenderMapping[]): RenderMapping[] {
  if (mappings.length <= 1) {
    return mappings;
  }

  const sortedMappings = [...mappings].sort((left, right) => {
    if (left.outStart !== right.outStart) {
      return left.outStart - right.outStart;
    }

    return left.outEnd - right.outEnd;
  });

  const selectedMappings: RenderMapping[] = [];
  let groupStart = 0;

  while (groupStart < sortedMappings.length) {
    let groupEnd = groupStart + 1;
    let groupOutEnd = sortedMappings[groupStart].outEnd;

    while (groupEnd < sortedMappings.length && sortedMappings[groupEnd].outStart < groupOutEnd) {
      groupOutEnd = Math.max(groupOutEnd, sortedMappings[groupEnd].outEnd);
      groupEnd++;
    }

    let minSpan = Number.POSITIVE_INFINITY;
    for (let index = groupStart; index < groupEnd; index++) {
      minSpan = Math.min(minSpan, sortedMappings[index].outEnd - sortedMappings[index].outStart);
    }

    for (let index = groupStart; index < groupEnd; index++) {
      const mapping = sortedMappings[index];
      if ((mapping.outEnd - mapping.outStart) === minSpan) {
        selectedMappings.push(mapping);
      }
    }

    groupStart = groupEnd;
  }

  return selectedMappings;
}

function comparePositions(
  left: { line: number; character: number },
  right: { line: number; character: number }
): number {
  if (left.line !== right.line) {
    return left.line - right.line;
  }

  return left.character - right.character;
}

function containsPosition(
  range: RenderMapping['sourceRange'],
  position: { line: number; character: number }
): boolean {
  return comparePositions(range.start, position) <= 0 &&
    comparePositions(position, range.end) < 0;
}

function intersectsSelection(range: RenderMapping['sourceRange'], selection: vscode.Selection): boolean {
  const selectionStart = { line: selection.start.line, character: selection.start.character };
  const selectionEnd = { line: selection.end.line, character: selection.end.character };
  return comparePositions(range.start, selectionEnd) < 0 &&
    comparePositions(selectionStart, range.end) < 0;
}

function dedupeOutputRanges(ranges: Array<{ outStart: number; outEnd: number }>): Array<{ outStart: number; outEnd: number }> {
  const seen = new Set<string>();
  const deduped: Array<{ outStart: number; outEnd: number }> = [];
  for (const range of ranges) {
    if (range.outStart >= range.outEnd) {
      continue;
    }

    const key = `${range.outStart}:${range.outEnd}`;
    if (seen.has(key)) {
      continue;
    }

    seen.add(key);
    deduped.push(range);
  }

  return deduped;
}

function sanitizePreviewFileStem(value: string): string {
  const sanitized = value
    .replace(/[^a-z0-9._-]+/gi, '-')
    .replace(/-+/g, '-')
    .replace(/^-+|-+$/g, '');
  return sanitized || 'preview';
}

function getWorkspacePreviewRoot(context: vscode.ExtensionContext, configPath: string): string {
  const workspaceFolder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(configPath))
    ?? vscode.workspace.workspaceFolders?.[0];
  const workspaceName = sanitizePreviewFileStem(workspaceFolder?.name ?? 'workspace');
  return path.join(context.globalStorageUri.fsPath, 'previews', workspaceName);
}

function getConfiguredPreviewMode(): PreviewMode {
  const configuredMode = vscode.workspace.getConfiguration('tpp').get<string>('previewMode');
  return configuredMode === 'webview' ? 'webview' : 'editor';
}

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function resolveHighlightJsLanguage(language?: string): string | undefined {
  const normalizedLanguage = normalizePreviewLanguage(language);
  if (!normalizedLanguage || normalizedLanguage === 'plaintext') {
    return undefined;
  }

  const aliases: Record<string, string> = {
    javascriptreact: 'jsx',
    shellscript: 'bash',
    typescriptreact: 'tsx'
  };
  const candidate = aliases[normalizedLanguage] ?? normalizedLanguage;
  return hljs.getLanguage(candidate) ? candidate : undefined;
}

function renderWebviewSyntaxHtml(output: string, languageHint?: string, fileExtensionHint?: string): string {
  const previewLanguage = resolvePreviewLanguage(languageHint, fileExtensionHint);
  const highlightLanguage = resolveHighlightJsLanguage(previewLanguage);
  if (!highlightLanguage) {
    return escapeHtml(output);
  }

  try {
    return hljs.highlight(output, { language: highlightLanguage, ignoreIllegals: true }).value;
  } catch {
    return escapeHtml(output);
  }
}

export class PreviewPanel {
  public static currentPanel: PreviewPanel | undefined;
  private static readonly webviewType = 'tppPreview';
  private static _isReopeningCurrent = false;

  private readonly _client: LanguageClient;
  private readonly _config: TppConfig;
  private readonly _previewIndex: number;
  private readonly _context: vscode.ExtensionContext;
  private readonly _mode: PreviewMode;
  private readonly _disposables: vscode.Disposable[] = [];
  private readonly _decorationType = vscode.window.createTextEditorDecorationType({
    backgroundColor: new vscode.ThemeColor('editor.selectionHighlightBackground'),
    borderColor: new vscode.ThemeColor('editor.selectionHighlightBorder'),
    borderStyle: 'solid',
    borderWidth: '1px',
    rangeBehavior: vscode.DecorationRangeBehavior.ClosedClosed,
  });

  private _lastMappings: RenderMapping[] = [];
  private _activeHighlightRanges: Array<{ outStart: number; outEnd: number }> = [];
  private _panel: vscode.WebviewPanel | undefined;
  private _previewUri: vscode.Uri | undefined;
  private _isDisposed = false;
  private _viewColumn: vscode.ViewColumn;

  public static async createOrShow(
    context: vscode.ExtensionContext,
    client: LanguageClient,
    config: TppConfig,
    previewIndex: number,
    viewColumn?: vscode.ViewColumn
  ): Promise<void> {
    const previewMode = getConfiguredPreviewMode();

    if (PreviewPanel.currentPanel && !PreviewPanel.currentPanel._matches(config, previewIndex, previewMode)) {
      PreviewPanel.currentPanel._dispose();
    }

    if (PreviewPanel.currentPanel) {
      await PreviewPanel.currentPanel._show();
      await PreviewPanel.currentPanel._refresh();
      return;
    }

    const previewPanel = new PreviewPanel(client, config, previewIndex, context, previewMode, viewColumn);
    PreviewPanel.currentPanel = previewPanel;
    await previewPanel._refresh();
  }

  public static async reopenCurrent(): Promise<void> {
    if (PreviewPanel._isReopeningCurrent || !PreviewPanel.currentPanel) {
      return;
    }

    PreviewPanel._isReopeningCurrent = true;
    const currentPanel = PreviewPanel.currentPanel;
    const { _context, _client, _config, _previewIndex } = currentPanel;
    const viewColumn = currentPanel._currentViewColumn();

    try {
      await currentPanel._closePreviewTabs();
      currentPanel._dispose();
      await PreviewPanel.createOrShow(_context, _client, _config, _previewIndex, viewColumn);
    } finally {
      PreviewPanel._isReopeningCurrent = false;
    }
  }

  private constructor(
    client: LanguageClient,
    config: TppConfig,
    previewIndex: number,
    context: vscode.ExtensionContext,
    mode: PreviewMode,
    viewColumn?: vscode.ViewColumn
  ) {
    this._client = client;
    this._config = config;
    this._previewIndex = previewIndex;
    this._context = context;
    this._mode = mode;
    this._viewColumn = viewColumn ?? vscode.ViewColumn.Beside;

    if (this._mode === 'webview') {
      this._panel = vscode.window.createWebviewPanel(
        PreviewPanel.webviewType,
        this._previewTitle(),
        this._viewColumn,
        { enableScripts: true, retainContextWhenHidden: true }
      );
      this._panel.webview.html = this._getLoadingHtml();
      this._disposables.push(this._panel.onDidDispose(() => this._dispose()));
    }

    this._disposables.push(
      this._decorationType,
      vscode.workspace.onDidChangeTextDocument(e => {
        const uri = e.document.uri.toString();
        if (this._previewUri && uri === this._previewUri.toString()) {
          return;
        }

        if (uri.endsWith('.tpp') || uri.endsWith('.tpp.types')) {
          void this._refresh();
        }
      }),
      vscode.workspace.onDidSaveTextDocument(e => {
        if (e.uri.toString().endsWith('.json')) {
          void this._refresh();
        }
      })
    );

    if (this._mode === 'editor') {
      this._disposables.push(
        vscode.workspace.onDidCloseTextDocument(e => {
          if (this._previewUri && e.uri.toString() === this._previewUri.toString()) {
            this._dispose();
          }
        }),
        vscode.window.onDidChangeVisibleTextEditors(() => {
          if (this._previewEditor()) {
            this._applyDecorations();
          }
        })
      );
    }
  }

  public onSelectionChange(uri: string, selections: readonly vscode.Selection[]): void {
    if (this._lastMappings.length === 0) {
      this.clearHighlights();
      return;
    }

    const normalizedUri = normalizeSourceUri(uri);
    const hasSourceAwareMappings = this._lastMappings.some(mapping => !!normalizeSourceUri(mapping.sourceUri));
    const activeSelections = selections.length > 0 ? selections : [new vscode.Selection(0, 0, 0, 0)];

    const highlighted = dedupeOutputRanges(activeSelections.flatMap(selection => {
      const matchedMappings: RenderMapping[] = [];
      for (const mapping of this._lastMappings) {
        const mappingUri = normalizeSourceUri(mapping.sourceUri);
        if (mappingUri) {
          if (mappingUri !== normalizedUri) {
            continue;
          }
        } else if (hasSourceAwareMappings) {
          continue;
        }

        const matches = selection.isEmpty
          ? containsPosition(mapping.sourceRange, { line: selection.active.line, character: selection.active.character })
          : intersectsSelection(mapping.sourceRange, selection);

        if (matches) {
          matchedMappings.push(mapping);
        }
      }

      return selectMostSpecificMappings(matchedMappings)
        .map(mapping => ({ outStart: mapping.outStart, outEnd: mapping.outEnd }));
    }));

    this._activeHighlightRanges = highlighted;

    if (this._mode === 'webview') {
      this._postWebviewHighlights();
      return;
    }

    this._applyDecorations();
  }

  public clearHighlights(): void {
    this._activeHighlightRanges = [];

    if (this._mode === 'webview') {
      this._postWebviewHighlights();
      return;
    }

    const editor = this._previewEditor();
    if (editor) {
      editor.setDecorations(this._decorationType, []);
    }
  }

  private async _refresh(): Promise<void> {
    try {
      const result = await this._client.sendRequest<PreviewResult>(
        'tpp/renderPreview',
        {
          configPath: 'file://' + this._config.configPath,
          previewIndex: this._previewIndex
        }
      );

      if (result.error) {
        this._lastMappings = [];
        await this._showError(result.error);
        this.clearHighlights();
        return;
      }

      this._lastMappings = result.mappings ?? [];
      await this._showPreview(result.output ?? '', result.language, result.fileExtension);
      this._syncWithActiveTemplateSelection();
    } catch (err) {
      this._lastMappings = [];
      await this._showError(String(err));
      this.clearHighlights();
    }
  }

  private async _showPreview(output: string, languageHint?: string, fileExtensionHint?: string): Promise<void> {
    if (this._mode === 'webview') {
      this._updateWebview(output, languageHint, fileExtensionHint);
      return;
    }

    await this._updatePreviewDocument(output, languageHint, fileExtensionHint);
  }

  private async _showError(errorMessage: string): Promise<void> {
    if (this._mode === 'webview') {
      if (this._panel) {
        this._panel.webview.html = this._getErrorHtml(errorMessage);
      }
      return;
    }

    await this._updatePreviewDocument(`tpp render error\n\n${errorMessage}\n`, 'plaintext', '.txt');
  }

  private async _updatePreviewDocument(output: string, languageHint?: string, fileExtensionHint?: string): Promise<void> {
    const previewUri = this._buildPreviewUri(languageHint, fileExtensionHint);
    const previewLanguage = resolvePreviewLanguage(languageHint, fileExtensionHint) ?? 'plaintext';

    await vscode.workspace.fs.createDirectory(vscode.Uri.file(path.dirname(previewUri.fsPath)));

    let document = this._previewDocument(previewUri);
    if (document) {
      if (document.getText() !== output) {
        const fullRange = new vscode.Range(
          document.positionAt(0),
          document.positionAt(document.getText().length)
        );
        const edit = new vscode.WorkspaceEdit();
        edit.replace(document.uri, fullRange, output);
        await vscode.workspace.applyEdit(edit);
        if (document.isDirty) {
          await document.save();
        }
      }
    } else {
      await vscode.workspace.fs.writeFile(previewUri, new TextEncoder().encode(output));
      document = await vscode.workspace.openTextDocument(previewUri);
    }

    this._previewUri = previewUri;

    if (document.languageId !== previewLanguage) {
      document = await vscode.languages.setTextDocumentLanguage(document, previewLanguage);
    }

    if (!this._previewEditor(previewUri)) {
      const editor = await vscode.window.showTextDocument(document, {
        viewColumn: this._viewColumn,
        preserveFocus: true,
        preview: false,
      });
      this._viewColumn = editor.viewColumn ?? this._viewColumn;
    }

    this._applyDecorations();
  }

  private _updateWebview(output: string, languageHint?: string, fileExtensionHint?: string): void {
    if (!this._panel) {
      return;
    }

    this._panel.title = this._previewTitle();
    this._panel.webview.html = this._getPreviewHtml(output, languageHint, fileExtensionHint);
  }

  private _previewTitle(): string {
    const previewEntry = this._config.previews?.[this._previewIndex];
    const previewName = previewEntry?.name || previewEntry?.template || `Preview ${this._previewIndex + 1}`;
    const suffix = this._mode === 'webview' ? 'webview' : 'editor';
    return `tpp Preview: ${previewName} (${suffix})`;
  }

  private _matches(config: TppConfig, previewIndex: number, mode: PreviewMode): boolean {
    return this._config.configPath === config.configPath &&
      this._previewIndex === previewIndex &&
      this._mode === mode;
  }

  private _buildPreviewUri(languageHint?: string, fileExtensionHint?: string): vscode.Uri {
    const previewDir = getWorkspacePreviewRoot(this._context, this._config.configPath);
    const previewEntry = this._config.previews?.[this._previewIndex];
    const previewStem = sanitizePreviewFileStem(
      `${path.basename(path.dirname(this._config.configPath))}-${previewEntry?.name || previewEntry?.template || `preview-${this._previewIndex + 1}`}`
    );
    const extension = resolvePreviewFileExtension(languageHint, fileExtensionHint);
    return vscode.Uri.file(path.join(previewDir, `${previewStem}${extension}`));
  }

  private _previewTabs(uri: vscode.Uri | undefined = this._previewUri): vscode.Tab[] {
    if (!uri) {
      return [];
    }

    const target = uri.toString();
    return vscode.window.tabGroups.all
      .flatMap(group => group.tabs)
      .filter(tab => tab.input instanceof vscode.TabInputText && tab.input.uri.toString() === target);
  }

  private async _closePreviewTabs(): Promise<void> {
    const tabs = this._previewTabs();
    if (tabs.length > 0) {
      await vscode.window.tabGroups.close(tabs);
    }
  }

  private async _showPreviewEditor(): Promise<void> {
    if (!this._previewUri) {
      return;
    }

    const visibleEditor = this._previewEditor(this._previewUri);
    if (visibleEditor) {
      this._viewColumn = visibleEditor.viewColumn ?? this._viewColumn;
      await vscode.window.showTextDocument(visibleEditor.document, {
        viewColumn: visibleEditor.viewColumn,
        preserveFocus: true,
        preview: false,
      });
      return;
    }

    const document = this._previewDocument(this._previewUri) ?? await vscode.workspace.openTextDocument(this._previewUri);
    const editor = await vscode.window.showTextDocument(document, {
      viewColumn: this._viewColumn,
      preserveFocus: true,
      preview: false,
    });
    this._viewColumn = editor.viewColumn ?? this._viewColumn;
  }

  private async _show(): Promise<void> {
    if (this._mode === 'webview') {
      if (!this._panel) {
        return;
      }

      try {
        this._panel.reveal(this._viewColumn, true);
        this._viewColumn = this._panel.viewColumn ?? this._viewColumn;
      } catch {
        this._dispose();
      }
      return;
    }

    await this._showPreviewEditor();
  }

  private _currentViewColumn(): vscode.ViewColumn {
    if (this._mode === 'webview') {
      return this._panel?.viewColumn ?? this._viewColumn;
    }

    return this._previewEditor()?.viewColumn ?? this._viewColumn;
  }

  private _postWebviewHighlights(): void {
    if (!this._panel) {
      return;
    }

    try {
      void this._panel.webview.postMessage({ type: 'highlight', ranges: this._activeHighlightRanges });
    } catch {
      this._dispose();
    }
  }

  private _previewDocument(uri: vscode.Uri | undefined = this._previewUri): vscode.TextDocument | undefined {
    if (!uri) {
      return undefined;
    }

    return vscode.workspace.textDocuments.find(document => document.uri.toString() === uri.toString());
  }

  private _previewEditor(uri: vscode.Uri | undefined = this._previewUri): vscode.TextEditor | undefined {
    if (!uri) {
      return undefined;
    }

    return vscode.window.visibleTextEditors.find(editor => editor.document.uri.toString() === uri.toString());
  }

  private _applyDecorations(): void {
    if (this._mode !== 'editor') {
      return;
    }

    const editor = this._previewEditor();
    if (!editor) {
      return;
    }

    const documentLength = editor.document.getText().length;
    const ranges = this._activeHighlightRanges.flatMap(range => {
      const outStart = Math.max(0, Math.min(range.outStart, documentLength));
      const outEnd = Math.max(outStart, Math.min(range.outEnd, documentLength));
      if (outStart >= outEnd) {
        return [];
      }

      return [new vscode.Range(editor.document.positionAt(outStart), editor.document.positionAt(outEnd))];
    });

    editor.setDecorations(this._decorationType, ranges);
  }

  private _syncWithActiveTemplateSelection(): void {
    const activeEditor = vscode.window.activeTextEditor;
    if (!activeEditor) {
      this.clearHighlights();
      return;
    }

    const document = activeEditor.document;
    const isTemplateDocument = document.languageId === 'tpp' ||
      document.languageId === 'tpp-types' ||
      document.uri.path.endsWith('.tpp') ||
      document.uri.path.endsWith('.tpp.types');

    if (!isTemplateDocument || activeEditor.selections.length === 0) {
      this.clearHighlights();
      return;
    }

    this.onSelectionChange(document.uri.toString(true), activeEditor.selections);
  }

  private _dispose(): void {
    if (this._isDisposed) {
      return;
    }

    this._isDisposed = true;
    const panel = this._panel;
    this._panel = undefined;
    void this._closePreviewTabs();
    this.clearHighlights();
    if (PreviewPanel.currentPanel === this) {
      PreviewPanel.currentPanel = undefined;
    }
    while (this._disposables.length) {
      const disposable = this._disposables.pop();
      disposable?.dispose();
    }

    if (panel) {
      try {
        panel.dispose();
      } catch {
        // Ignore duplicate disposal when VS Code already tore down the webview.
      }
    }
  }

  private _getLoadingHtml(): string {
    return '<!DOCTYPE html><html><body><p>Rendering…</p></body></html>';
  }

  private _getErrorHtml(message: string): string {
    const escaped = escapeHtml(message);
    return `<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body {
    font-family: var(--vscode-editor-font-family);
    color: var(--vscode-errorForeground);
    padding: 12px;
  }

  pre {
    white-space: pre-wrap;
  }
</style>
</head>
<body>
  <h3>tpp render error</h3>
  <pre>${escaped}</pre>
</body>
</html>`;
  }

  private _getPreviewHtml(output: string, languageHint?: string, fileExtensionHint?: string): string {
    const renderedHtml = renderWebviewSyntaxHtml(output, languageHint, fileExtensionHint);
    const previewLanguage = resolvePreviewLanguage(languageHint, fileExtensionHint) ?? 'plaintext';
    return `<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body {
    margin: 0;
    padding: 8px;
    background: var(--vscode-editor-background);
    color: var(--vscode-editor-foreground);
    font-family: var(--vscode-editor-font-family);
    font-size: var(--vscode-editor-font-size);
  }

  .meta {
    margin-bottom: 8px;
    color: var(--vscode-descriptionForeground);
    font-size: 0.9em;
  }

  pre {
    margin: 0;
    white-space: pre-wrap;
    word-break: break-word;
  }

  code {
    display: block;
    color: inherit;
    font-family: inherit;
    font-size: inherit;
  }

  .hljs {
    background: transparent;
    color: var(--vscode-editor-foreground);
  }

  .hljs-comment,
  .hljs-quote {
    color: var(--vscode-descriptionForeground);
  }

  .hljs-keyword,
  .hljs-selector-tag,
  .hljs-literal,
  .hljs-section,
  .hljs-link,
  .hljs-type,
  .hljs-built_in {
    color: var(--vscode-symbolIconClassForeground, #569cd6);
  }

  .hljs-string,
  .hljs-regexp,
  .hljs-addition,
  .hljs-attribute,
  .hljs-meta-string {
    color: var(--vscode-testing-iconPassed, #ce9178);
  }

  .hljs-number,
  .hljs-symbol,
  .hljs-bullet,
  .hljs-variable,
  .hljs-template-variable {
    color: var(--vscode-symbolIconVariableForeground, #b5cea8);
  }

  .hljs-title,
  .hljs-title.class_,
  .hljs-title.function_ {
    color: var(--vscode-symbolIconFunctionForeground, #dcdcaa);
  }

  mark.highlight {
    background: var(--vscode-editor-selectionHighlightBackground, rgba(173, 214, 255, 0.3));
    border: 1px solid var(--vscode-editor-selectionHighlightBorder, transparent);
    border-radius: 2px;
    padding: 0;
  }
</style>
</head>
<body>
  <div class="meta">Mode: webview · Language hint: ${escapeHtml(previewLanguage)}</div>
  <pre><code id="content" class="hljs">${renderedHtml}</code></pre>
  <script>
    (function() {
      const content = document.getElementById('content');
      const baseHtml = ${JSON.stringify(renderedHtml)};
      let textNodes = [];
      let offsets = [];

      function collectTextNodes(root) {
        const nodes = [];
        const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        let node;
        while ((node = walker.nextNode())) {
          nodes.push(node);
        }
        return nodes;
      }

      function buildIndex() {
        textNodes = collectTextNodes(content);
        offsets = [];
        let currentOffset = 0;
        for (const node of textNodes) {
          offsets.push(currentOffset);
          currentOffset += node.textContent.length;
        }
      }

      function restoreBaseHtml() {
        content.innerHTML = baseHtml;
        buildIndex();
      }

      function findNodeAndOffset(charIndex) {
        let low = 0;
        let high = textNodes.length - 1;
        while (low < high) {
          const mid = (low + high + 1) >> 1;
          if (offsets[mid] <= charIndex) {
            low = mid;
          } else {
            high = mid - 1;
          }
        }

        return {
          node: textNodes[low],
          offset: charIndex - offsets[low]
        };
      }

      function applyHighlights(ranges) {
        restoreBaseHtml();
        if (!ranges || ranges.length === 0 || textNodes.length === 0) {
          return;
        }

        const sorted = [...ranges].sort((left, right) => right.outStart - left.outStart);
        for (let index = sorted.length - 1; index >= 0; index--) {
          const rangeSpec = sorted[index];
          if (rangeSpec.outStart >= rangeSpec.outEnd) {
            continue;
          }

          try {
            const end = Math.min(rangeSpec.outEnd, content.textContent.length);
            const start = Math.max(0, Math.min(rangeSpec.outStart, end));
            let nodeIndex = textNodes.length - 1;
            while (nodeIndex >= 0 && offsets[nodeIndex] >= end) {
              nodeIndex -= 1;
            }

            while (nodeIndex >= 0) {
              const nodeStart = offsets[nodeIndex];
              const node = textNodes[nodeIndex];
              const nodeEnd = nodeStart + node.textContent.length;
              if (nodeEnd <= start) {
                break;
              }

              const localStart = Math.max(0, start - nodeStart);
              const localEnd = Math.min(node.textContent.length, end - nodeStart);
              if (localStart < localEnd) {
                let target = node;
                if (localStart > 0) {
                  target = target.splitText(localStart);
                }
                if (localEnd - localStart < target.textContent.length) {
                  target.splitText(localEnd - localStart);
                }

                const mark = document.createElement('mark');
                mark.className = 'highlight';
                target.parentNode.insertBefore(mark, target);
                mark.appendChild(target);
              }

              nodeIndex -= 1;
            }
            buildIndex();
          } catch {
            restoreBaseHtml();
            return;
          }
        }
      }

      restoreBaseHtml();
      window.addEventListener('message', event => {
        if (event.data?.type === 'highlight') {
          applyHighlights(event.data.ranges);
        }
      });
    })();
  </script>
</body>
</html>`;
  }
}
