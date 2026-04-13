import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const extensionRoot = path.resolve(__dirname, '..');
const versionPath = path.resolve(extensionRoot, '..', 'version.json');
const packagePath = path.resolve(extensionRoot, 'package.json');

const versionJson = JSON.parse(fs.readFileSync(versionPath, 'utf8'));
const packageJson = JSON.parse(fs.readFileSync(packagePath, 'utf8'));

const expectedVersion = `${versionJson.major}.${versionJson.minor}.${versionJson.patch}`;
if (packageJson.version !== expectedVersion) {
    packageJson.version = expectedVersion;
    fs.writeFileSync(packagePath, `${JSON.stringify(packageJson, null, 2)}\n`);
    console.log(`Updated package.json version to ${expectedVersion}`);
} else {
    console.log(`package.json version already matches ${expectedVersion}`);
}