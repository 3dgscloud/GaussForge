import { GaussForgeBase } from './base';
export * from './types';

export class GaussForge extends GaussForgeBase {
    protected async importWasmModule() {
        // @ts-ignore
        return import('./gauss_forge.web.js');
    }
}

let _instance: GaussForge | null = null;
export async function createGaussForge(factory?: any): Promise<GaussForge> {
    if (!_instance) {
        _instance = new GaussForge();
        await _instance.init(factory);
    }
    return _instance;
}

export function destroyGaussForge(): void {
    if (_instance) {
        _instance.dispose();
        _instance = null;
    }
}