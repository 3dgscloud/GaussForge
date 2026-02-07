/**
 * GaussForge WASM Base Class
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
    ModelInfoResult,
    ModelInfoOptions,
} from './types';

export interface EmscriptenModule {
    GaussForgeWASM: new () => GaussForgeWASMInstance;
    [key: string]: any;
}

export interface GaussForgeWASMInstance {
    read(data: Uint8Array, format: string, strict: boolean): any;
    write(ir: any, format: string, strict: boolean): any;
    convert(data: Uint8Array, inFmt: string, outFmt: string, strict: boolean): any;
    getModelInfo(data: Uint8Array, format: string, fileSize: number): any;
    getSupportedFormats(): string[];
    getVersion(): string;
    delete(): void;
}

export abstract class GaussForgeBase {
    protected module: EmscriptenModule | null = null;
    protected instance: GaussForgeWASMInstance | null = null;
    protected initPromise: Promise<void> | null = null;

    // To be implemented by subclasses, inject different JS file paths
    protected abstract importWasmModule(): Promise<any>;

    async init(moduleFactory?: (overrides?: any) => EmscriptenModule): Promise<void> {
        if (this.initPromise) return this.initPromise;

        this.initPromise = (async () => {
            try {
                let moduleInstance: any;
                if (moduleFactory) {
                    moduleInstance = moduleFactory();
                } else {
                    const createModule = await this.importWasmModule();
                    const factory = createModule.default;
                    moduleInstance = typeof factory === 'function' ? factory() : factory;
                }

                if (moduleInstance && typeof moduleInstance.then === 'function') {
                    moduleInstance = await moduleInstance;
                }

                this.module = moduleInstance as EmscriptenModule;
                this.instance = new this.module.GaussForgeWASM();
            } catch (error) {
                this.initPromise = null;
                throw new Error(`GaussForge Init Failed: ${error instanceof Error ? error.message : String(error)}`);
            }
        })();
        return this.initPromise;
    }

    protected ensureInitialized(): void {
        if (!this.instance) throw new Error('GaussForge not initialized. Call init() first.');
    }

    async read(data: ArrayBuffer | Uint8Array, format: string, options: ReadOptions = {}): Promise<ReadResult> {
        this.ensureInitialized();
        const input = data instanceof ArrayBuffer ? new Uint8Array(data) : data;
        const result = this.instance!.read(input, format, options.strict || false);
        if (result.error) throw new Error(result.error);
        return {
            data: result.data,
            ...(result.warning && { warning: result.warning })
        } as ReadResult;
    }

    async write(ir: GaussianCloudIR, format: string, options: WriteOptions = {}): Promise<WriteResult> {
        this.ensureInitialized();
        const result = this.instance!.write(ir, format, options.strict || false);
        if (result.error) throw new Error(result.error);
        return {
            data: result.data
        } as WriteResult;
    }

    async convert(data: ArrayBuffer | Uint8Array, inFmt: string, outFmt: string, options: ConvertOptions = {}): Promise<ConvertResult> {
        this.ensureInitialized();
        const input = data instanceof ArrayBuffer ? new Uint8Array(data) : data;
        const result = this.instance!.convert(input, inFmt, outFmt, options.strict || false);
        if (result.error) throw new Error(result.error);
        return {
            data: result.data
        } as ConvertResult;
    }

    getSupportedFormats(): SupportedFormat[] {
        this.ensureInitialized();
        return this.instance!.getSupportedFormats() as SupportedFormat[];
    }

    getVersion(): string {
        this.ensureInitialized();
        return this.instance!.getVersion();
    }

    async getModelInfo(data: ArrayBuffer | Uint8Array, format: string, options: ModelInfoOptions = {}): Promise<ModelInfoResult> {
        this.ensureInitialized();
        const input = data instanceof ArrayBuffer ? new Uint8Array(data) : data;
        const result = this.instance!.getModelInfo(input, format, options.fileSize || 0);
        if (result.error) return { error: result.error } as ModelInfoResult;
        return {
            data: result.data
        } as ModelInfoResult;
    }

    /**
     * Check if a format is supported
     * @param format - Format name to check
     * @returns true if the format is supported, false otherwise
     */
    isFormatSupported(format: string): boolean {
        this.ensureInitialized();
        const formats = this.getSupportedFormats();
        return formats.includes(format as SupportedFormat);
    }

    /**
     * Explicitly destroy C++ instance to prevent memory leaks
     */
    dispose(): void {
        if (this.instance) {
            this.instance.delete();
            this.instance = null;
        }
        this.module = null;
        this.initPromise = null;
    }
}