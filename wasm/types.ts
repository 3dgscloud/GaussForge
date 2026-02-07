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

export type SupportedFormat = 'ply' | 'compressed.ply' | 'splat' | 'ksplat' | 'spz' | 'sog';


export interface ReadOptions {
    strict?: boolean;
}

export interface WriteOptions {
    strict?: boolean;
}

export interface ConvertOptions {
    strict?: boolean;
}

// Model Info types
export interface FloatStats {
    min: number;
    max: number;
    avg: number;
}

export interface BoundingBox {
    x: [number, number];
    y: [number, number];
    z: [number, number];
}

export interface ModelInfoBasic {
    numPoints: number;
    fileSize?: number;
    sourceFormat?: string;
}

export interface ModelInfoRendering {
    shDegree: number;
    antialiased: boolean;
}

export interface ModelInfoMeta {
    handedness: string;
    upAxis: string;
    unit: string;
    colorSpace: string;
}

export interface ModelInfoSizes {
    positions: string;
    scales: string;
    rotations: string;
    alphas: string;
    colors: string;
    sh: string;
    total: string;
}

export interface ModelInfo {
    basic: ModelInfoBasic;
    rendering: ModelInfoRendering;
    meta: ModelInfoMeta;
    bounds?: BoundingBox;
    scaleStats?: FloatStats;
    alphaStats?: FloatStats;
    sizes: ModelInfoSizes;
    extraAttrs?: Record<string, string>;
}

export interface ModelInfoResult {
    data: ModelInfo;
    error?: string;
}

export interface ModelInfoOptions {
    fileSize?: number;
}

