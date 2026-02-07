#!/usr/bin/env node

/**
 * Local test script - Test @gaussforge/wasm package using tiny_gauss.ply
 * 
 * Usage:
 *   1. Install local package: npm install
 *   2. Run test: npm test
 * 
 * Note: Package is installed locally from parent directory (wasm/)
 */

import { createGaussForge } from '@gaussforge/wasm';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const TEST_FILE = path.join(__dirname, 'tiny_gauss.ply');

async function test() {
    console.log('🧪 Testing @gaussforge/wasm local package\n');
    console.log(`📁 Test file: ${TEST_FILE}\n`);

    try {
        // 1. Check if test file exists
        if (!fs.existsSync(TEST_FILE)) {
            console.error(`❌ Error: Test file does not exist: ${TEST_FILE}`);
            process.exit(1);
        }

        // 2. Initialize
        console.log('1️⃣  Initializing GaussForge...');
        const gaussForge = await createGaussForge();
        console.log('   ✅ Initialization successful\n');

        console.log('0️⃣  Printing version...');
        const version = gaussForge.getVersion();
        console.log(`   ✅ Version: ${version}\n`);

        // 3. Get supported formats
        console.log('2️⃣  Getting supported formats...');
        const formats = gaussForge.getSupportedFormats();
        console.log(`   ✅ Supported formats: ${formats.join(', ')}\n`);

        // 4. Read test file
        console.log('3️⃣  Reading test file...');
        const inputData = fs.readFileSync(TEST_FILE);
        console.log(`   📦 File size: ${inputData.length} bytes`);

        const readResult = await gaussForge.read(inputData, 'ply');
        console.log('   ✅ Read successful');
        console.log(`   📊 Number of points: ${readResult.data.numPoints}`);
        console.log(`   📊 SH degree: ${readResult.data.meta.shDegree}`);
        console.log(`   📊 Source format: ${readResult.data.meta.sourceFormat || 'Unknown'}\n`);

        // 4.5. Test model info
        console.log('4️⃣  Testing model info...');
        const infoResult = await gaussForge.getModelInfo(inputData, 'ply', inputData.length);
        if (infoResult.error) {
            console.log(`   ❌ Model info failed: ${infoResult.error}\n`);
        } else {
            const info = infoResult.data;
            console.log('   ✅ Model info retrieved');
            console.log(`   📊 Basic info:`);
            console.log(`      - Points: ${info.basic.numPoints}`);
            if (info.basic.fileSize) console.log(`      - File size: ${info.basic.fileSize} bytes`);
            if (info.basic.sourceFormat) console.log(`      - Source format: ${info.basic.sourceFormat}`);
            console.log(`   🎨 Rendering:`);
            console.log(`      - SH degree: ${info.rendering.shDegree}`);
            console.log(`      - Antialiased: ${info.rendering.antialiased ? 'Yes' : 'No'}`);
            if (info.meta.handedness !== 'Unknown') {
                console.log(`   📐 Metadata:`);
                console.log(`      - Handedness: ${info.meta.handedness}`);
                console.log(`      - Up axis: ${info.meta.upAxis}`);
                console.log(`      - Unit: ${info.meta.unit}`);
                console.log(`      - Color space: ${info.meta.colorSpace}`);
            }
            if (info.bounds) {
                console.log(`   📦 Bounds:`);
                console.log(`      - X: [${info.bounds.x[0]}, ${info.bounds.x[1]}]`);
                console.log(`      - Y: [${info.bounds.y[0]}, ${info.bounds.y[1]}]`);
                console.log(`      - Z: [${info.bounds.z[0]}, ${info.bounds.z[1]}]`);
            }
            if (info.scaleStats) {
                console.log(`   📏 Scale stats:`);
                console.log(`      - Min: ${info.scaleStats.min}`);
                console.log(`      - Max: ${info.scaleStats.max}`);
                console.log(`      - Avg: ${info.scaleStats.avg}`);
            }
            if (info.alphaStats) {
                console.log(`   🔍 Alpha stats:`);
                console.log(`      - Min: ${info.alphaStats.min}`);
                console.log(`      - Max: ${info.alphaStats.max}`);
                console.log(`      - Avg: ${info.alphaStats.avg}`);
            }
            console.log(`   💾 Data sizes:`);
            console.log(`      - Positions: ${info.sizes.positions}`);
            console.log(`      - Scales: ${info.sizes.scales}`);
            console.log(`      - Rotations: ${info.sizes.rotations}`);
            console.log(`      - Alphas: ${info.sizes.alphas}`);
            console.log(`      - Colors: ${info.sizes.colors}`);
            console.log(`      - SH: ${info.sizes.sh}`);
            console.log(`      - Total: ${info.sizes.total}`);
            if (info.extraAttrs) {
                console.log(`   ➕ Extra attributes:`);
                for (const [name, size] of Object.entries(info.extraAttrs)) {
                    console.log(`      - ${name}: ${size}`);
                }
            }
            console.log('');
        }

        // 5. Test format conversion
        console.log('4️⃣  Testing format conversion...');
        const outputFormats = ['splat', 'ksplat', 'spz', 'ply', 'compressed.ply', 'sog'];

        for (const outFormat of outputFormats) {
            if (!formats.includes(outFormat)) {
                console.log(`   ⏭️  Skipping ${outFormat} (not supported)`);
                continue;
            }

            try {
                const convertResult = await gaussForge.convert(inputData, 'ply', outFormat);
                console.log(`   ✅ ply -> ${outFormat}: ${convertResult.data.length} bytes`);

                // Optionally: Save conversion result
                const outputFile = path.join(__dirname, `output.${outFormat}`);
                fs.writeFileSync(outputFile, convertResult.data);
                console.log(`      💾 Saved to: ${outputFile}`);
            } catch (error) {
                console.log(`   ❌ ply -> ${outFormat} failed: ${error.message}`);
            }
        }

        console.log('\n🎉 All tests completed!\n');

    } catch (error) {
        console.error('\n❌ Test failed:', error.message);
        console.error(error.stack);
        process.exit(1);
    }
}

test();

