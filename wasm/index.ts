/**
 * GaussForge WASM TypeScript wrapper library
 */

import type {
    GaussianCloudIR,
    ReadResult,
    WriteResult,
    ConvertResult,
    SupportedFormat,
    ReadOptions,
    WriteOptions,
    ConvertOptions,
} from './types';

// Emscripten module type definition
interface EmscriptenModule {
    GaussForgeWASM: new () => GaussForgeWASMInstance;
    [key: string]: any; // Allow other Emscripten properties
}

interface GaussForgeWASMInstance {
    read(
        data: ArrayBuffer | Uint8Array,
        format: string,
        strict?: boolean
    ): ReadResult | { error: string };
    write(
        ir: GaussianCloudIR,
        format: string,
        strict?: boolean
    ): WriteResult | { error: string };
    convert(
        data: ArrayBuffer | Uint8Array,
        inFormat: string,
        outFormat: string,
        strict?: boolean
    ): ConvertResult | { error: string };
    getSupportedFormats(): string[];
}

// Module loader type
type ModuleFactory = (moduleOverrides?: any) => EmscriptenModule;

/**
 * GaussForge WASM wrapper class
 */
export class GaussForge {
    private module: EmscriptenModule | null = null;
    private instance: GaussForgeWASMInstance | null = null;
    private initPromise: Promise<void> | null = null;

    /**
     * Initialize WASM module
     * @param moduleFactory Emscripten module factory function
     */
    async init(moduleFactory?: ModuleFactory): Promise<void> {
        if (this.initPromise) {
            return this.initPromise;
        }

        this.initPromise = (async () => {
            try {
                let moduleInstance: EmscriptenModule;

                if (moduleFactory) {
                    moduleInstance = moduleFactory();
                } else {
                    // Default load from package root (in npm package) or current directory (during development)
                    // Dynamically import Emscripten-generated module
                    // @ts-ignore - May not exist during development, will have error at runtime
                    // In npm package, gauss_forge.js is in package root, need to find from dist/ up
                    const wasmPath = typeof __dirname !== 'undefined'
                        ? require('path').join(__dirname, '..', 'gauss_forge.js')  // CommonJS
                        : new URL('../gauss_forge.js', import.meta.url).href;      // ES Module
                    const createModule = await import(wasmPath);

                    // Emscripten module may be a function or object
                    if (typeof createModule.default === 'function') {
                        moduleInstance = createModule.default();
                    } else if (createModule.default && typeof (createModule.default as any).then === 'function') {
                        // If it's a Promise, wait for it to complete
                        moduleInstance = await (createModule.default as any);
                    } else {
                        moduleInstance = createModule.default as EmscriptenModule;
                    }
                }

                // Wait for module to fully initialize (if module is a Promise)
                if (moduleInstance && 'then' in moduleInstance && typeof (moduleInstance as any).then === 'function') {
                    moduleInstance = await (moduleInstance as any);
                }

                if (!moduleInstance || !moduleInstance.GaussForgeWASM) {
                    throw new Error('Failed to initialize WASM module: GaussForgeWASM not found');
                }

                this.module = moduleInstance;
                this.instance = new this.module.GaussForgeWASM();
            } catch (error) {
                this.initPromise = null;
                const errorMessage = error instanceof Error ? error.message : String(error);
                throw new Error(`Failed to initialize GaussForge: ${errorMessage}`);
            }
        })();

        return this.initPromise;
    }

    /**
     * Ensure module is initialized
     */
    private ensureInitialized(): void {
        if (!this.instance) {
            throw new Error('GaussForge not initialized. Call init() first.');
        }
    }

    /**
     * Read Gaussian splatting data
     * @param data Input data (ArrayBuffer or Uint8Array)
     * @param format File format
     * @param options Read options
     * @returns Read result
     */
    async read(
        data: ArrayBuffer | Uint8Array,
        format: SupportedFormat | string,
        options: ReadOptions = {}
    ): Promise<ReadResult> {
        this.ensureInitialized();

        const dataArray = data instanceof ArrayBuffer ? new Uint8Array(data) : data;
        const result = this.instance!.read(dataArray, format, options.strict || false);

        if ('error' in result) {
            throw new Error(result.error);
        }

        return result as ReadResult;
    }

    /**
     * Write Gaussian splatting data
     * @param ir Gaussian splatting intermediate representation
     * @param format Output format
     * @param options Write options
     * @returns Write result
     */
    async write(
        ir: GaussianCloudIR,
        format: SupportedFormat | string,
        options: WriteOptions = {}
    ): Promise<WriteResult> {
        this.ensureInitialized();

        const result = this.instance!.write(ir, format, options.strict || false);

        if ('error' in result) {
            throw new Error(result.error);
        }

        return result as WriteResult;
    }

    /**
     * Convert file format
     * @param data Input data
     * @param inFormat Input format
     * @param outFormat Output format
     * @param options Convert options
     * @returns Convert result
     */
    async convert(
        data: ArrayBuffer | Uint8Array,
        inFormat: SupportedFormat | string,
        outFormat: SupportedFormat | string,
        options: ConvertOptions = {}
    ): Promise<ConvertResult> {
        this.ensureInitialized();

        const dataArray = data instanceof ArrayBuffer ? new Uint8Array(data) : data;
        const result = this.instance!.convert(
            dataArray,
            inFormat,
            outFormat,
            options.strict || false
        );

        if ('error' in result) {
            throw new Error(result.error);
        }

        return result as ConvertResult;
    }

    /**
     * Get list of supported formats
     * @returns Array of supported formats
     */
    getSupportedFormats(): SupportedFormat[] {
        this.ensureInitialized();
        return this.instance!.getSupportedFormats() as SupportedFormat[];
    }

    /**
     * Check if format is supported
     * @param format Format name
     * @returns Whether the format is supported
     */
    isFormatSupported(format: string): boolean {
        return this.getSupportedFormats().includes(format as SupportedFormat);
    }
}

// Default exported singleton instance
let defaultInstance: GaussForge | null = null;

/**
 * Get default GaussForge instance
 * @param moduleFactory Optional module factory function
 * @returns GaussForge instance
 */
export async function createGaussForge(
    moduleFactory?: ModuleFactory
): Promise<GaussForge> {
    if (!defaultInstance) {
        defaultInstance = new GaussForge();
        await defaultInstance.init(moduleFactory);
    }
    return defaultInstance;
}

// Export types
export * from './types';

