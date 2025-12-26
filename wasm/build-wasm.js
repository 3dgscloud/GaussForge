#!/usr/bin/env node

/**
 * WASM build script
 * Compile C++ code to WASM using Emscripten
 */

import { execSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PROJECT_ROOT = path.resolve(__dirname, '..');
const WASM_DIR = path.join(PROJECT_ROOT, 'wasm');
const BUILD_DIR = path.join(PROJECT_ROOT, 'build-wasm');

// Find and set Emscripten path
function findEmscripten() {
    // First check if it's already in PATH
    try {
        execSync('emcc --version', { stdio: 'ignore', env: process.env });
        return { found: true, emsdkPath: process.env.EMSDK || '' };
    } catch (error) {
        // Try to find Emscripten in project directory
        const possiblePaths = [
            path.join(PROJECT_ROOT, '.emsdk'),
            path.join(PROJECT_ROOT, '../emsdk'),
            path.join(process.env.HOME || '', 'emsdk'),
        ];

        for (const emsdkPath of possiblePaths) {
            const emccPath = path.join(emsdkPath, 'upstream/emscripten/emcc');
            if (fs.existsSync(emccPath) || fs.existsSync(emccPath + '.bat')) {
                console.log(`Found Emscripten: ${emsdkPath}`);
                // Set environment variables
                const emscriptenDir = path.join(emsdkPath, 'upstream/emscripten');
                process.env.PATH = emscriptenDir + path.delimiter + process.env.PATH;
                process.env.EMSDK = emsdkPath;
                return { found: true, emsdkPath };
            }
        }

        console.error('Error: Emscripten not found.');
        console.error('Please do one of the following:');
        console.error('  1. Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html');
        console.error('  2. Or manually activate: source .emsdk/emsdk_env.sh');
        console.error('  3. Or set environment variable: export EMSDK=/path/to/emsdk');
        return { found: false, emsdkPath: '' };
    }
}

// Check if Emscripten is installed
function checkEmscripten() {
    const result = findEmscripten();
    return result.found;
}

// Create build directory
function setupBuildDir() {
    if (!fs.existsSync(BUILD_DIR)) {
        fs.mkdirSync(BUILD_DIR, { recursive: true });
    } else {
        // clean 
        fs.rmSync(BUILD_DIR, { recursive: true });
        fs.mkdirSync(BUILD_DIR, { recursive: true });
        console.log(`✓ Cleaned build directory ${BUILD_DIR}`);
    }
}

// Run CMake configuration
function configureCMake(environment) {
    console.log(`Configuring CMake for ${environment} environment...`);
    const emsdkPath = process.env.EMSDK || '';
    let emcmake = 'emcmake';

    if (emsdkPath) {
        const emcmakePath = path.join(emsdkPath, 'upstream/emscripten/emcmake');
        if (fs.existsSync(emcmakePath) || fs.existsSync(emcmakePath + '.bat')) {
            emcmake = emcmakePath;
        }
    }

    try {
        execSync(
            `${emcmake} cmake -B ${BUILD_DIR} -S ${PROJECT_ROOT} -DCMAKE_BUILD_TYPE=Release -DWASM_ENVIRONMENT=${environment}`,
            { stdio: 'inherit', cwd: PROJECT_ROOT, env: process.env }
        );
    } catch (error) {
        console.error('CMake configuration failed:', error.message);
        process.exit(1);
    }
}

// Build WASM
function buildWASM() {
    console.log('Building WASM...');
    try {
        execSync(
            `cmake --build ${BUILD_DIR} --target gauss_forge_wasm`,
            { stdio: 'inherit', cwd: PROJECT_ROOT, env: process.env }
        );
    } catch (error) {
        console.error('WASM build failed:', error.message);
        process.exit(1);
    }
}

// Copy generated files
function copyOutputFiles(outputName) {
    console.log(`Copying output files for ${outputName}...`);
    const jsFile = path.join(BUILD_DIR, 'gauss_forge.js');
    const wasmFile = path.join(BUILD_DIR, 'gauss_forge.wasm');

    if (!fs.existsSync(jsFile)) {
        console.error(`Error: File not found ${jsFile}`);
        process.exit(1);
    }

    const outputJsFile = path.join(WASM_DIR, `${outputName}.js`);
    fs.copyFileSync(jsFile, outputJsFile);
    console.log(`✓ Copied ${jsFile} -> ${outputJsFile}`);

    if (fs.existsSync(wasmFile)) {
        const outputWasmFile = path.join(WASM_DIR, `${outputName}.wasm`);
        fs.copyFileSync(wasmFile, outputWasmFile);
        console.log(`✓ Copied ${wasmFile} -> ${outputWasmFile}`);
    }
}

// Build a specific version
function buildVersion(environment, outputName) {
    console.log(`\n=== Building ${outputName} version (${environment}) ===\n`);
    configureCMake(environment);
    buildWASM();
    copyOutputFiles(outputName);
}

// Main function
function main() {
    console.log('Starting WASM build...\n');

    if (!checkEmscripten()) {
        process.exit(1);
    }

    setupBuildDir();

    // Build Node version
    buildVersion('node', 'gauss_forge.node');

    // Clean and rebuild for Web version
    setupBuildDir();

    // Build Web version
    buildVersion('web,worker', 'gauss_forge.web');

    // clean build directory
    fs.rmSync(BUILD_DIR, { recursive: true });
    console.log(`✓ Cleaned build directory ${BUILD_DIR}`);
    console.log('\n✓ WASM build complete!');
    console.log('Generated files:');
    console.log('  - gauss_forge.node.js (Node.js version)');
    console.log('  - gauss_forge.web.js (Web/Worker version)');
}

main();

