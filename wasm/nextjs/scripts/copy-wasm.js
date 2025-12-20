import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

// copy gauss_forge.wasm to public/wasm/gauss_forge.wasm
const __dirname = path.dirname(fileURLToPath(import.meta.url));
// Resolve to node_modules/@gaussforge/wasm/gauss_forge.wasm
const wasmPath = path.resolve(__dirname, '..', 'node_modules', '@gaussforge', 'wasm', 'gauss_forge.wasm');
const destDir = path.join(__dirname, '..', 'public', 'wasm');
const destPath = path.join(destDir, 'gauss_forge.wasm');

if (!fs.existsSync(wasmPath)) {
    console.error(`Error: WASM file not found at ${wasmPath}`);
    console.error('Please make sure @gaussforge/wasm is installed correctly.');
    process.exit(1);
}

fs.mkdirSync(destDir, { recursive: true });
fs.copyFileSync(wasmPath, destPath);
console.log(`Copied ${wasmPath} to ${destPath}`);