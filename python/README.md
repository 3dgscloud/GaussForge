# GaussForge Python Binding

High-performance Gaussian Splatting format conversion library for Python.

## Installation

```bash
pip install gaussforge
```

## Quick Start

```python
import gaussforge

# Create instance
gf = gaussforge.GaussForge()

# Read a PLY file
with open("model.ply", "rb") as f:
    data = f.read()

result = gf.read(data, "ply")
if "error" not in result:
    print(f"Loaded {result['data']['numPoints']} points")

# Convert to another format
converted = gf.convert(data, "ply", "splat")
if "error" not in converted:
    with open("output.splat", "wb") as f:
        f.write(converted["data"])
```

## Supported Formats

- `ply` - Standard PLY format
- `compressed.ply` - Compressed PLY format
- `splat` - Splat format
- `ksplat` - K-Splat format
- `spz` - SPZ compressed format
- `sog` - SOG format

## API Reference

### `GaussForge()`

Create a new GaussForge instance.

### `read(data: bytes, format: str, strict: bool = False) -> dict`

Read Gaussian data from bytes.

- `data`: Raw file data as bytes
- `format`: Input format name
- `strict`: Enable strict validation (default: False)

Returns a dict with `data` key containing the parsed Gaussian data, or `error` key on failure.

### `write(ir: dict, format: str, strict: bool = False) -> dict`

Write Gaussian IR to bytes.

- `ir`: Gaussian intermediate representation dict
- `format`: Output format name
- `strict`: Enable strict validation (default: False)

Returns a dict with `data` key containing the encoded bytes, or `error` key on failure.

### `convert(data: bytes, in_format: str, out_format: str, strict: bool = False) -> dict`

Convert between formats directly.

- `data`: Input file data as bytes
- `in_format`: Input format name
- `out_format`: Output format name
- `strict`: Enable strict validation (default: False)

Returns a dict with `data` key containing the converted bytes, or `error` key on failure.

### `get_model_info(data: bytes, format: str, file_size: int = 0) -> dict`

Get detailed model information.

- `data`: Raw file data as bytes
- `format`: Input format name
- `file_size`: Optional file size for reporting

Returns a dict with `data` key containing model info, or `error` key on failure.

### `get_supported_formats() -> list[str]`

Get list of supported format names.

### `get_version() -> str`

Get library version string.

## Building from Source

```bash
cd python
pip install -e .
```

## License

Apache-2.0
