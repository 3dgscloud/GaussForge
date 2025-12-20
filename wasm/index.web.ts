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
    getVersion(): string;
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
                let moduleInstance: any; // 先使用 any 接收原始实例

                if (moduleFactory) {
                    moduleInstance = moduleFactory();
                } else {
                    // 1. 动态导入生成的 JS
                    // @ts-ignore
                    const createModule = await import('./gauss_forge.web.js');

                    // 2. 这里的 createModule.default 通常是工厂函数
                    const factory = createModule.default;

                    if (typeof factory === 'function') {
                        // 执行工厂函数，它通常返回一个 Promise
                        const result = factory();
                        // 检查是否返回了 Promise (Emscripten MODULARIZE=1 的标准行为)
                        moduleInstance = (result && typeof result.then === 'function')
                            ? await result
                            : result;
                    } else {
                        moduleInstance = factory;
                    }
                }

                // 3. 再次确保如果 moduleInstance 本身还是个 Promise，则等待它
                if (moduleInstance && typeof moduleInstance.then === 'function') {
                    moduleInstance = await moduleInstance;
                }

                // 4. 此时 moduleInstance 应该是真正的 EmscriptenModule 了
                const finalModule = moduleInstance as EmscriptenModule;

                if (!finalModule || !finalModule.GaussForgeWASM) {
                    throw new Error('Failed to initialize WASM module: GaussForgeWASM not found');
                }

                this.module = finalModule;
                this.instance = new this.module.GaussForgeWASM();
            } catch (error) {
                this.initPromise = null;
                const errorMessage = error instanceof Error ? error.message : String(error);
                throw new Error(`Failed to initialize GaussForge: ${errorMessage}`);
            }
        })();

        return this.initPromise;
    }

    private cloneFromWasm<T>(data: T): T {
        if (!data || typeof data !== 'object') return data;

        // 如果是 Uint8Array，执行 slice() 或 new Uint8Array() 
        // 这一步会分配 JS 引擎管理的内存，不再受 WASM 内存扩容影响
        if (data instanceof Uint8Array) {
            return new Uint8Array(data) as any;
        }

        if (Array.isArray(data)) {
            return data.map(item => this.cloneFromWasm(item)) as any;
        }

        const copy: any = {};
        for (const key in data) {
            copy[key] = this.cloneFromWasm((data as any)[key]);
        }
        return copy;
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

        return this.cloneFromWasm(result) as ReadResult;
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

        return this.cloneFromWasm(result) as WriteResult;
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

        return this.cloneFromWasm(result) as ConvertResult;
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

    /**
     * Get library version
     * @returns Version string
     */
    getVersion(): string {
        this.ensureInitialized();
        return this.instance!.getVersion();
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

