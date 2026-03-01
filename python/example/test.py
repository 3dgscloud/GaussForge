#!/usr/bin/env python3
"""
Local test script - Test gaussforge package using tiny_gauss.ply

Usage:
    1. Build package: pip install -e ..
    2. Run test: python test.py
"""

import os
import sys
from pathlib import Path

# Add parent directory to path for development testing
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from gaussforge import GaussForge, get_version


def main():
    print("=" * 60)
    print("Testing gaussforge Python package")
    print("=" * 60)

    test_file = Path(__file__).parent / "tiny_gauss.ply"

    # Check if test file exists
    if not test_file.exists():
        print(f"Error: Test file does not exist: {test_file}")
        sys.exit(1)

    print(f"\nTest file: {test_file}")

    try:
        # 1. Initialize
        print("\n1. Initializing GaussForge...")
        gf = GaussForge()
        print("   Initialization successful")

        # 2. Print version
        print("\n2. Getting version...")
        version = gf.get_version()
        print(f"   Version: {version}")

        # 3. Get supported formats
        print("\n3. Getting supported formats...")
        formats = gf.get_supported_formats()
        print(f"   Supported formats: {', '.join(formats)}")

        # 4. Read test file
        print("\n4. Reading test file...")
        with open(test_file, "rb") as f:
            input_data = f.read()
        print(f"   File size: {len(input_data)} bytes")

        read_result = gf.read(input_data, "ply")
        if "error" in read_result:
            print(f"   Read failed: {read_result['error']}")
            sys.exit(1)

        print("   Read successful")
        data = read_result["data"]
        print(f"   Number of points: {data['numPoints']}")
        print(f"   SH degree: {data['meta']['shDegree']}")

        # 5. Test model info
        print("\n5. Testing model info...")
        info_result = gf.get_model_info(input_data, "ply", len(input_data))
        if "error" in info_result:
            print(f"   Model info failed: {info_result['error']}")
        else:
            info = info_result["data"]
            print("   Model info retrieved")
            print("   Basic info:")
            print(f"      - Points: {info['basic']['numPoints']}")
            if "fileSize" in info["basic"]:
                print(f"      - File size: {info['basic']['fileSize']} bytes")
            if "sourceFormat" in info["basic"]:
                print(f"      - Source format: {info['basic']['sourceFormat']}")

            print("   Rendering:")
            print(f"      - SH degree: {info['rendering']['shDegree']}")
            print(f"      - Antialiased: {info['rendering']['antialiased']}")

            if "bounds" in info:
                print("   Bounds:")
                print(f"      - X: {info['bounds']['x']}")
                print(f"      - Y: {info['bounds']['y']}")
                print(f"      - Z: {info['bounds']['z']}")

            if "scaleStats" in info:
                print("   Scale stats:")
                print(f"      - Min: {info['scaleStats']['min']}")
                print(f"      - Max: {info['scaleStats']['max']}")
                print(f"      - Avg: {info['scaleStats']['avg']}")

            if "alphaStats" in info:
                print("   Alpha stats:")
                print(f"      - Min: {info['alphaStats']['min']}")
                print(f"      - Max: {info['alphaStats']['max']}")
                print(f"      - Avg: {info['alphaStats']['avg']}")

            print("   Data sizes:")
            for key, value in info["sizes"].items():
                print(f"      - {key}: {value}")

            if "extraAttrs" in info:
                print("   Extra attributes:")
                for name, size in info["extraAttrs"].items():
                    print(f"      - {name}: {size}")

        # 6. Test format conversion
        print("\n6. Testing format conversion...")
        output_formats = ["splat", "ksplat", "spz", "ply", "compressed.ply", "sog"]

        for out_format in output_formats:
            if out_format not in formats:
                print(f"   Skipping {out_format} (not supported)")
                continue

            try:
                convert_result = gf.convert(input_data, "ply", out_format)
                if "error" in convert_result:
                    print(f"   ply -> {out_format} failed: {convert_result['error']}")
                else:
                    output_size = len(convert_result["data"])
                    print(f"   ply -> {out_format}: {output_size} bytes")

                    # Save conversion result
                    output_file = Path(__file__).parent / f"output.{out_format}"
                    with open(output_file, "wb") as f:
                        f.write(convert_result["data"])
                    print(f"      Saved to: {output_file}")
            except Exception as e:
                print(f"   ply -> {out_format} failed: {e}")

        print("\n" + "=" * 60)
        print("All tests completed!")
        print("=" * 60)

    except Exception as e:
        print(f"\nTest failed: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
