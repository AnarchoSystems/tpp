import * as vscode from 'vscode';
import { LanguageClient } from 'vscode-languageclient/node';
import { TppConfig } from './configScanner';

interface RenderMapping {
  sourceRange: {
    start: { line: number; character: number };
    end:   { line: number; character: number };
  };
  outStart: number;
  outEnd:   number;
}

interface PreviewResult {
  output?: string;
  mappings?: RenderMapping[];
  error?: string;
}

export class PreviewPanel {
  public static currentPanel: PreviewPanel | undefined;
  private static readonly viewType = 'tppPreview';

  private readonly _panel: vscode.WebviewPanel;
  private readonly _client: LanguageClient;
  private readonly _config: TppConfig;
  private readonly _previewIndex: number;
  private readonly _context: vscode.ExtensionContext;
  private _disposables: vscode.Disposable[] = [];

  private _lastOutput: string = '';
  private _lastMappings: RenderMapping[] = [];

  public static createOrShow(
    context: vscode.ExtensionContext,
    client: LanguageClient,
    config: TppConfig,
    previewIndex: number
  ): void {
    if (PreviewPanel.currentPanel) {
      PreviewPanel.currentPanel._panel.reveal(vscode.ViewColumn.Beside);
      PreviewPanel.currentPanel._refresh();
      return;
    }

    const panel = vscode.window.createWebviewPanel(
      PreviewPanel.viewType,
      'tpp Preview',
      vscode.ViewColumn.Beside,
      { enableScripts: true, retainContextWhenHidden: true }
    );

    PreviewPanel.currentPanel = new PreviewPanel(panel, client, config, previewIndex, context);
  }

  private constructor(
    panel: vscode.WebviewPanel,
    client: LanguageClient,
    config: TppConfig,
    previewIndex: number,
    context: vscode.ExtensionContext
  ) {
    this._panel = panel;
    this._client = client;
    this._config = config;
    this._previewIndex = previewIndex;
    this._context = context;

    this._panel.webview.html = this._getLoadingHtml();
    this._refresh();

    this._panel.onDidDispose(() => this._dispose(), null, this._disposables);

    // Re-render when a tpp template/types file changes (uses LSP dirty buffer — live)
    vscode.workspace.onDidChangeTextDocument(e => {
      const uri = e.document.uri.toString();
      if (uri.endsWith('.tpp') || uri.endsWith('.tpp.types')) {
        this._refresh();
      }
    }, null, this._disposables);

    // Re-render when tpp-config.json is saved (LSP reads it from disk, so on-save only)
    vscode.workspace.onDidSaveTextDocument(e => {
      if (e.uri.toString().endsWith('.json')) {
        this._refresh();
      }
    }, null, this._disposables);
  }

  public onCursorMove(uri: string, pos: vscode.Position): void {
    if (this._lastMappings.length === 0) return;

    // Find all mappings whose sourceRange contains (pos.line, pos.character)
    const highlighted: Array<{ outStart: number; outEnd: number }> = [];
    for (const m of this._lastMappings) {
      const { start, end } = m.sourceRange;
      if (
        pos.line >= start.line && pos.line <= end.line &&
        (pos.line !== start.line || pos.character >= start.character) &&
        (pos.line !== end.line   || pos.character <= end.character)
      ) {
        highlighted.push({ outStart: m.outStart, outEnd: m.outEnd });
      }
    }

    this._panel.webview.postMessage({ type: 'highlight', ranges: highlighted });
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
        this._panel.webview.html = this._getErrorHtml(result.error);
        return;
      }

      this._lastOutput   = result.output   ?? '';
      this._lastMappings = result.mappings  ?? [];
      this._panel.webview.html = this._getPreviewHtml(this._lastOutput);
    } catch (err) {
      this._panel.webview.html = this._getErrorHtml(String(err));
    }
  }

  private _getLoadingHtml(): string {
    return `<!DOCTYPE html><html><body><p>Rendering…</p></body></html>`;
  }

  private _getErrorHtml(msg: string): string {
    const escaped = msg.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return `<!DOCTYPE html>
<html>
<head><meta charset="UTF-8">
<style>body { font-family: var(--vscode-editor-font-family); color: var(--vscode-errorForeground); }</style>
</head>
<body><h3>tpp render error</h3><pre>${escaped}</pre></body>
</html>`;
  }

  private _getPreviewHtml(output: string): string {
    const escaped = output.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return `<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body {
    font-family: var(--vscode-editor-font-family);
    font-size: var(--vscode-editor-font-size);
    color: var(--vscode-editor-foreground);
    background: var(--vscode-editor-background);
    margin: 0;
    padding: 8px;
    white-space: pre;
  }
  .highlight {
    background: var(--vscode-editor-selectionHighlightBackground,
                     rgba(173, 214, 255, 0.3));
    border-radius: 2px;
  }
</style>
</head>
<body id="content">${escaped}</body>
<script>
  (function() {
    const vscode = acquireVsCodeApi();
    const rawText = document.getElementById('content').innerText;

    // Build a char-index → DOM text-node map for highlighting.
    // We collect all text nodes in order.
    function collectTextNodes(root) {
      const nodes = [];
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
      let node;
      while ((node = walker.nextNode())) nodes.push(node);
      return nodes;
    }

    let textNodes = [];
    let offsets = [];   // cumulative char offset at the start of each text node

    function buildIndex() {
      textNodes = collectTextNodes(document.body);
      offsets = [];
      let cur = 0;
      for (const n of textNodes) {
        offsets.push(cur);
        cur += n.textContent.length;
      }
    }

    function findNodeAndOffset(charIdx) {
      // Binary search
      let lo = 0, hi = textNodes.length - 1;
      while (lo < hi) {
        const mid = (lo + hi + 1) >> 1;
        if (offsets[mid] <= charIdx) lo = mid;
        else hi = mid - 1;
      }
      return { node: textNodes[lo], offset: charIdx - offsets[lo] };
    }

    function applyHighlights(ranges) {
      // Remove previous highlights by rebuilding the body HTML from source.
      document.body.innerHTML = ${JSON.stringify(escaped)};
      buildIndex();
      if (!ranges || ranges.length === 0) return;

      // Collect all (start, end) pairs, sort by start
      const sorted = [...ranges].sort((a, b) => a.outStart - b.outStart);

      // We'll reconstruct the content with <mark> spans.
      // Work backwards so character offsets stay valid.
      for (let i = sorted.length - 1; i >= 0; i--) {
        const { outStart, outEnd } = sorted[i];
        if (outStart >= outEnd) continue;
        try {
          const range = document.createRange();
          const { node: ns, offset: os } = findNodeAndOffset(outStart);
          const { node: ne, offset: oe } = findNodeAndOffset(outEnd);
          range.setStart(ns, os);
          range.setEnd(ne, oe);
          const mark = document.createElement('mark');
          mark.className = 'highlight';
          range.surroundContents(mark);
          // Rebuild index after DOM mutation
          buildIndex();
        } catch (_) {}
      }
    }

    buildIndex();

    window.addEventListener('message', event => {
      const msg = event.data;
      if (msg.type === 'highlight') {
        applyHighlights(msg.ranges);
      }
    });
  })();
</script>
</html>`;
  }

  private _dispose(): void {
    PreviewPanel.currentPanel = undefined;
    this._panel.dispose();
    while (this._disposables.length) {
      const x = this._disposables.pop();
      if (x) x.dispose();
    }
  }
}
