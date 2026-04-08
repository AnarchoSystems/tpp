import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';

export interface TppPreviewEntry {
  name: string;
  template: string;
  input: string | object;
}

export interface TppConfig {
  configPath: string;
  previews?: TppPreviewEntry[];
}

/**
 * Find all tpp-config.json files within the given workspace folders.
 * Searches up to 8 levels deep, skipping hidden dirs, node_modules, and build/.
 */
export async function findTppConfigs(
  workspaceFolders: readonly vscode.WorkspaceFolder[]
): Promise<TppConfig[]> {
  const configs: TppConfig[] = [];

  async function scanDir(dirPath: string, depth: number): Promise<void> {
    if (depth <= 0) return;
    let entries: fs.Dirent[];
    try {
      entries = fs.readdirSync(dirPath, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      if (entry.name.startsWith('.') || entry.name === 'node_modules' || entry.name === 'build') {
        continue;
      }
      const fullPath = path.join(dirPath, entry.name);
      if (entry.isFile() && entry.name === 'tpp-config.json') {
        try {
          const raw = fs.readFileSync(fullPath, 'utf-8');
          const cfg = JSON.parse(raw) as Partial<TppConfig>;
          configs.push({ configPath: fullPath, previews: cfg.previews });
        } catch {
          // skip unparseable configs
        }
      } else if (entry.isDirectory()) {
        await scanDir(fullPath, depth - 1);
      }
    }
  }

  for (const folder of workspaceFolders) {
    await scanDir(folder.uri.fsPath, 8);
  }
  return configs;
}
