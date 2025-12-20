'use client';

import { useState, useEffect } from 'react';
import { createGaussForge, type GaussForge, type SupportedFormat } from '@gaussforge/wasm';

export default function Home() {
    // 1. 存储 GaussForge 实例
    const [gf, setGf] = useState<GaussForge | null>(null);
    const [supportedFormats, setSupportedFormats] = useState<SupportedFormat[]>([]);
    const [selectedFile, setSelectedFile] = useState<File | null>(null);
    const [fileInfo, setFileInfo] = useState<{
        name: string;
        size: number;
        format: string;
    } | null>(null);
    const [readResult, setReadResult] = useState<{
        numPoints: number;
        shDegree: number;
        sourceFormat?: string;
    } | null>(null);
    const [convertStatus, setConvertStatus] = useState<{
        type: 'idle' | 'converting' | 'success' | 'error';
        message: string;
        data?: Uint8Array;
    }>({ type: 'idle', message: '' });
    const [inFormat, setInFormat] = useState<SupportedFormat>('ply');
    const [outFormat, setOutFormat] = useState<SupportedFormat>('splat');

    // 2. 组件加载时初始化 WASM
    useEffect(() => {
        async function initWasm() {
            try {
                const instance = await createGaussForge();
                setGf(instance);
                setSupportedFormats(instance.getSupportedFormats());
            } catch (err) {
                console.error("WASM Initialization failed:", err);
            }
        }
        initWasm();
    }, []);

    const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
        const file = event.target.files?.[0];
        if (!file) return;

        setSelectedFile(file);
        setFileInfo({
            name: file.name,
            size: file.size,
            format: file.name.split('.').pop() || 'unknown',
        });

        const ext = file.name.split('.').pop()?.toLowerCase();
        const validFormats: SupportedFormat[] = ['ply', 'splat', 'ksplat', 'spz'];
        if (validFormats.includes(ext as SupportedFormat)) {
            setInFormat(ext as SupportedFormat);
        }

        setReadResult(null);
        setConvertStatus({ type: 'idle', message: '' });
    };

    const handleRead = async () => {
        if (!selectedFile || !gf) {
            alert(!gf ? 'WASM module is still loading...' : 'Please select a file first');
            return;
        }

        try {
            const arrayBuffer = await selectedFile.arrayBuffer();
            // 使用实例方法 gf.read
            const result = await gf.read(arrayBuffer, inFormat);

            setReadResult({
                numPoints: result.data.numPoints,
                shDegree: result.data.meta.shDegree,
                sourceFormat: result.data.meta.sourceFormat,
            });
        } catch (error) {
            alert(`Read failed: ${error instanceof Error ? error.message : String(error)}`);
        }
    };

    const handleConvert = async () => {
        if (!selectedFile || !gf) {
            alert(!gf ? 'WASM module is still loading...' : 'Please select a file first');
            return;
        }

        if (inFormat === outFormat) {
            alert('Input and output formats cannot be the same');
            return;
        }

        setConvertStatus({ type: 'converting', message: 'Converting...' });

        try {
            const arrayBuffer = await selectedFile.arrayBuffer();
            // 使用实例方法 gf.convert
            const result = await gf.convert(
                arrayBuffer,
                inFormat,
                outFormat
            );

            setConvertStatus({
                type: 'success',
                message: `Conversion successful! Output size: ${result.data.length} bytes`,
                data: result.data,
            });
        } catch (error) {
            setConvertStatus({
                type: 'error',
                message: `Conversion failed: ${error instanceof Error ? error.message : String(error)}`,
            });
        }
    };

    const handleDownload = () => {
        if (convertStatus.data) {
            const blob = new Blob([convertStatus.data], { type: 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `converted.${outFormat}`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }
    };

    return (
        <div className="container" style={{ padding: '2rem', maxWidth: '800px', margin: '0 auto', fontFamily: 'sans-serif' }}>
            <h1>🧪 GaussForge WASM Demo</h1>
            <p className="subtitle">Test Gaussian Splatting format conversion in Next.js</p>

            {!gf && <div className="status info">Loading WASM Module...</div>}

            {/* File Selection Section */}
            <section className="section" style={{ marginBottom: '2rem', border: '1px solid #ccc', padding: '1rem' }}>
                <h2>1. Select File</h2>
                <div className="file-input">
                    <label htmlFor="file-input">Choose a Gaussian Splatting file: </label>
                    <input
                        id="file-input"
                        type="file"
                        accept=".ply,.splat,.ksplat,.spz"
                        onChange={handleFileSelect}
                    />
                </div>

                {fileInfo && (
                    <div className="info-grid" style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '1rem', marginTop: '1rem' }}>
                        <div className="info-item">
                            <label style={{ display: 'block', fontWeight: 'bold' }}>File Name</label>
                            <span>{fileInfo.name}</span>
                        </div>
                        <div className="info-item">
                            <label style={{ display: 'block', fontWeight: 'bold' }}>File Size</label>
                            <span>{(fileInfo.size / 1024).toFixed(2)} KB</span>
                        </div>
                        <div className="info-item">
                            <label style={{ display: 'block', fontWeight: 'bold' }}>Detected Format</label>
                            <span>{fileInfo.format}</span>
                        </div>
                    </div>
                )}

                {fileInfo && (
                    <div className="select-group" style={{ marginTop: '1rem' }}>
                        <label>Input Format: </label>
                        <select
                            value={inFormat}
                            onChange={(e) => setInFormat(e.target.value as SupportedFormat)}
                        >
                            {supportedFormats.map((format) => (
                                <option key={format} value={format}>
                                    {format}
                                </option>
                            ))}
                        </select>
                        <button className="button" onClick={handleRead} style={{ marginLeft: '1rem' }}>
                            Read File
                        </button>
                    </div>
                )}

                {readResult && (
                    <div className="status success" style={{ marginTop: '1rem', color: 'green', backgroundColor: '#e6fffa', padding: '1rem' }}>
                        <strong>File Read Successfully!</strong>
                        <div className="info-grid" style={{ display: 'flex', gap: '2rem', marginTop: '1rem' }}>
                            <div className="info-item">
                                <label style={{ display: 'block', fontWeight: 'bold' }}>Number of Points</label>
                                <span>{readResult.numPoints.toLocaleString()}</span>
                            </div>
                            <div className="info-item">
                                <label style={{ display: 'block', fontWeight: 'bold' }}>SH Degree</label>
                                <span>{readResult.shDegree}</span>
                            </div>
                        </div>
                    </div>
                )}
            </section>

            {/* Format Conversion Section */}
            {fileInfo && (
                <section className="section" style={{ border: '1px solid #ccc', padding: '1rem' }}>
                    <h2>2. Format Conversion</h2>
                    <div className="select-group">
                        <label>Convert from: </label>
                        <select
                            value={inFormat}
                            onChange={(e) => setInFormat(e.target.value as SupportedFormat)}
                        >
                            {supportedFormats.map((format) => (
                                <option key={format} value={format}>
                                    {format}
                                </option>
                            ))}
                        </select>
                        <label style={{ margin: '0 0.5rem' }}>to:</label>
                        <select
                            value={outFormat}
                            onChange={(e) => setOutFormat(e.target.value as SupportedFormat)}
                        >
                            {supportedFormats.map((format) => (
                                <option key={format} value={format}>
                                    {format}
                                </option>
                            ))}
                        </select>
                        <button
                            className="button"
                            onClick={handleConvert}
                            disabled={convertStatus.type === 'converting'}
                            style={{ marginLeft: '1rem' }}
                        >
                            {convertStatus.type === 'converting' ? 'Converting...' : 'Convert'}
                        </button>
                    </div>

                    {convertStatus.type !== 'idle' && (
                        <div
                            className={`status`}
                            style={{
                                marginTop: '1rem',
                                padding: '1rem',
                                color: convertStatus.type === 'error' ? 'red' : 'inherit',
                                backgroundColor: convertStatus.type === 'success' ? '#e6fffa' : '#f7fafc'
                            }}
                        >
                            {convertStatus.message}
                        </div>
                    )}

                    {convertStatus.type === 'success' && convertStatus.data && (
                        <div className="actions" style={{ marginTop: '1rem' }}>
                            <button onClick={handleDownload}>
                                Download Converted File
                            </button>
                        </div>
                    )}
                </section>
            )}
        </div>
    );
}