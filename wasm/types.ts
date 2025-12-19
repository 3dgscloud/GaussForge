/**
 * TypeScript type definitions
 */

export interface GaussMetadata {
    shDegree: number;
    sourceFormat?: string;
}

export interface GaussianCloudIR {
    numPoints: number;
    positions: Float32Array;
    scales: Float32Array;
    rotations: Float32Array;
    alphas: Float32Array;
    colors: Float32Array;
    sh: Float32Array;
    extras: Record<string, Float32Array>;
    meta: GaussMetadata;
}

export interface ReadResult {
    data: GaussianCloudIR;
    warning?: string;
    error?: string;
}

export interface WriteResult {
    data: Uint8Array;
    error?: string;
}

export interface ConvertResult {
    data: Uint8Array;
    warning?: string;
    error?: string;
}

export type SupportedFormat = 'ply' | 'compressed.ply' | 'splat' | 'ksplat' | 'spz';

export interface ReadOptions {
    strict?: boolean;
}

export interface WriteOptions {
    strict?: boolean;
}

export interface ConvertOptions {
    strict?: boolean;
}

