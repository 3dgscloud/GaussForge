# GaussForge WASM

WebAssembly version of GaussForge, providing TypeScript wrapper library for use in browsers and Node.js.

## Features

- Support for multiple Gaussian splatting formats: PLY, Compressed PLY, Splat, KSplat, SPZ
- Format conversion functionality
- TypeScript type support
- Browser and Node.js compatible

## Installation

```bash
npm install @gaussforge/wasm
```

## Usage

### Browser

```typescript
import { createGaussForge } from '@gaussforge/wasm';

async function handleFileUpload(file: File) {
    // Initialize
    const gaussForge = await createGaussForge();
    
    // Read file
    const fileData = await file.arrayBuffer();
    const result = await gaussForge.read(fileData, 'ply');
    console.log(`Loaded ${result.data.numPoints} points`);
    
    // Convert format
    const converted = await gaussForge.convert(fileData, 'ply', 'splat');
    
    // Download the result
    const blob = new Blob([converted.data], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'output.splat';
    a.click();
    URL.revokeObjectURL(url);
}

// Usage with file input
const fileInput = document.querySelector('input[type="file"]');
fileInput.addEventListener('change', (e) => {
    const file = (e.target as HTMLInputElement).files?.[0];
    if (file) {
        handleFileUpload(file);
    }
});
```

### Node.js

```typescript
import { createGaussForge } from '@gaussforge/wasm';
import fs from 'fs';

async function convertFile() {
    const gaussForge = await createGaussForge();
    
    // Read file
    const inputData = fs.readFileSync('input.ply');
    const result = await gaussForge.read(inputData, 'ply');
    console.log(`Loaded ${result.data.numPoints} points`);
    
    // Convert format
    const converted = await gaussForge.convert(inputData, 'ply', 'splat');
    
    // Save file
    fs.writeFileSync('output.splat', converted.data);
}

convertFile().catch(console.error);
```

## API

### `createGaussForge(moduleFactory?)`

Create and initialize a GaussForge instance.

### `read(data, format, options?)`

Read Gaussian splatting data.

- `data`: `ArrayBuffer | Uint8Array` - Input data
- `format`: `SupportedFormat | string` - File format
- `options.strict`: `boolean` - Whether to use strict mode

Returns: `Promise<ReadResult>`

### `write(ir, format, options?)`

Write Gaussian splatting data.

- `ir`: `GaussianCloudIR` - Gaussian splatting intermediate representation
- `format`: `SupportedFormat | string` - Output format
- `options.strict`: `boolean` - Whether to use strict mode

Returns: `Promise<WriteResult>`

### `convert(data, inFormat, outFormat, options?)`

Convert file format.

- `data`: `ArrayBuffer | Uint8Array` - Input data
- `inFormat`: `SupportedFormat | string` - Input format
- `outFormat`: `SupportedFormat | string` - Output format
- `options.strict`: `boolean` - Whether to use strict mode

Returns: `Promise<ConvertResult>`

### `getSupportedFormats()`

Get the list of supported formats.

Returns: `SupportedFormat[]`

### `isFormatSupported(format)`

Check if a format is supported.

- `format`: `string` - Format name to check

Returns: `boolean`

## Supported Formats

- `ply` - PLY format
- `compressed.ply` - Compressed PLY format
- `splat` - Splat format
- `ksplat` - KSplat format
- `spz` - SPZ format

## Development

### Build WASM

```bash
cd wasm
npm install
npm run build:wasm
```

### Build TypeScript

```bash
npm run build:ts
```

### Full Build

```bash
npm run build
```

## Error Handling

All methods may throw errors. It's recommended to wrap API calls in try-catch blocks:

```typescript
try {
    const result = await gaussForge.read(fileData, 'ply');
    // Handle success
} catch (error) {
    console.error('Error reading file:', error.message);
    // Handle error
}
```

## Requirements

- Emscripten SDK (for building WASM)
- Node.js 18+ (for development)
- TypeScript 5+ (for development)

