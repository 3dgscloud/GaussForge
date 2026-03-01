"""
GaussForge - High-performance Gaussian Splatting format conversion library.

This library provides efficient conversion between various Gaussian Splatting
formats including PLY, SPZ, SPLAT, KSPLAT, SOG, and compressed PLY.

Example:
    import gaussforge

    # Read a PLY file
    with open("model.ply", "rb") as f:
        data = f.read()

    gf = gaussforge.GaussForge()
    result = gf.read(data, "ply")

    if "error" in result:
        print(f"Error: {result['error']}")
    else:
        print(f"Loaded {result['data']['numPoints']} points")

    # Convert to another format
    converted = gf.convert(data, "ply", "splat")
    if "error" not in converted:
        with open("output.splat", "wb") as f:
            f.write(converted["data"])
"""

from gaussforge._core import GaussForge, get_version

__version__ = get_version()
__all__ = ["GaussForge", "get_version", "__version__"]
