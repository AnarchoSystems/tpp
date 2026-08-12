import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { execFileSync } from 'child_process';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const extensionRoot = path.resolve(__dirname, '..');
const repoRoot = path.resolve(extensionRoot, '..');
const buildDir = path.resolve(repoRoot, 'build');
const binaryName = process.platform === 'win32' ? 'tpp-lsp.exe' : 'tpp-lsp';
const sourceBinaryPath = path.join(buildDir, 'bin', binaryName);
const localOutputDir = path.resolve(extensionRoot, 'out', 'lsp');
const bundledOutputDir = path.resolve(extensionRoot, 'resources', 'lsp', `${process.platform}-${process.arch}`);
const bundledOutputPath = path.join(bundledOutputDir, binaryName);

function run(command, args) {
  execFileSync(command, args, {
    cwd: repoRoot,
    stdio: 'inherit'
  });
}

if (!fs.existsSync(sourceBinaryPath)) {
  run('cmake', ['-S', repoRoot, '-B', buildDir]);
  run('cmake', ['--build', buildDir, '--parallel', '4', '--target', 'tpp-lsp']);
}

if (!fs.existsSync(sourceBinaryPath)) {
  throw new Error(`Expected built language server at ${sourceBinaryPath}`);
}

fs.mkdirSync(localOutputDir, { recursive: true });
fs.mkdirSync(bundledOutputDir, { recursive: true });
fs.copyFileSync(sourceBinaryPath, path.join(localOutputDir, binaryName));
fs.copyFileSync(sourceBinaryPath, bundledOutputPath);

console.log(`Copied ${sourceBinaryPath} to ${path.join(localOutputDir, binaryName)}`);
console.log(`Copied ${sourceBinaryPath} to ${bundledOutputPath}`);