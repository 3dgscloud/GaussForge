# GaussForge Next.js Demo

This is a Next.js demo application for testing the GaussForge WASM library.

## Setup

1. Install dependencies:
```bash
npm install
```

Note: This demo uses the local `@gaussforge/wasm` package from the parent directory (via `file:..` in package.json).

2. Make sure the WASM package is built:
```bash
cd ..
npm run build
```

3. Run the development server:
```bash
npm run dev
```

4. Open [http://localhost:3000](http://localhost:3000) in your browser.

## Features

- Initialize GaussForge WASM library
- Upload and read Gaussian Splatting files
- Convert between different formats (PLY, Splat, KSplat, SPZ, Compressed PLY)
- Download converted files

## Testing

1. Click "Initialize GaussForge" to load the WASM module
2. Select a Gaussian Splatting file (`.ply`, `.splat`, `.ksplat`, or `.spz`)
3. Click "Read File" to see file information
4. Choose input and output formats, then click "Convert"
5. Download the converted file

## Note

This demo uses the latest Next.js (v15) with the App Router. The WASM module is loaded dynamically in the browser using ESM imports.

