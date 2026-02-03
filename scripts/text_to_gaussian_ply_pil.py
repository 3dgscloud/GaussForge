#!/usr/bin/env python3
"""
Generate 3D Gaussian PLY by rendering text with PIL and using pixel coordinates.
Glyphs come from real fonts; no manual coordinates. Output is GaussForge-compatible (includes f_rest_0..44).

Dependencies: pip install numpy pillow

Usage:
  python3 text_to_gaussian_ply_pil.py -o output_gaussians.ply
  python3 text_to_gaussian_ply_pil.py -o out.ply --step 3 --color-top "#87CEEB" --color-bottom "#FFD700"
  python3 text_to_gaussian_ply_pil.py -o out.ply --step 2 --color "#FFFFFF"   # solid color, no gradient
Default: step=2 (sparse), gradient (blue top / gold bottom), per-point z/scale random (more 3D).
"""

import math
import os
import struct
import sys
from pathlib import Path

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
except ImportError as e:
    print("Please install dependencies: pip install numpy pillow", file=sys.stderr)
    sys.exit(1)

SH_C0 = 0.28209479177387814


def get_text_points(text, font_path=None, font_size=100, step=1):
    """
    Render text with PIL to an image and extract non-zero pixel coordinates as point cloud.
    step: sample every step pixels; 1=all, 2=every other.
    """
    try:
        if font_path and os.path.isfile(font_path):
            font = ImageFont.truetype(font_path, font_size)
        else:
            font = ImageFont.load_default()
            if not font_path:
                print("Warning: No font specified, using default (consider specifying font_path)", file=sys.stderr)
    except Exception as e:
        print(f"Warning: Font load failed {e}, using default", file=sys.stderr)
        font = ImageFont.load_default()

    dummy = Image.new("L", (1, 1))
    draw = ImageDraw.Draw(dummy)
    bbox = draw.textbbox((0, 0), text, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    if w <= 0 or h <= 0:
        w, h = max(1, len(text) * font_size // 2), font_size

    image = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), text, font=font, fill=255)

    data = np.array(image)
    y_idx, x_idx = np.where(data > 128)
    if step > 1:
        idx = np.arange(len(y_idx), dtype=np.intp)[::step]
        y_idx = y_idx[idx]
        x_idx = x_idx[idx]

    x = x_idx.astype(np.float32)
    y = y_idx.astype(np.float32)   # Keep image Y down, consistent with common 3D view (text not flipped)
    x -= x.mean()
    y -= y.mean()
    scale = 100.0
    x /= scale
    y /= scale
    n = len(x)
    # Slight position randomness: xy jitter, z random, to avoid overly uniform layout
    jitter = 0.006
    x += np.random.uniform(-jitter, jitter, size=n).astype(np.float32)
    y += np.random.uniform(-jitter, jitter, size=n).astype(np.float32)
    z = np.random.uniform(-0.004, 0.004, size=n).astype(np.float32)
    return x, y, z


def rgb_to_f_dc(r, g, b):
    r, g, b = max(0, min(1, r)), max(0, min(1, g)), max(0, min(1, b))
    return ((r - 0.5) / SH_C0, (g - 0.5) / SH_C0, (b - 0.5) / SH_C0)


def inverse_sigmoid(x):
    x = max(1e-6, min(1 - 1e-6, x))
    return math.log(x / (1 - x))


def write_ply_binary_gaussforge(
    out_path,
    x, y, z,
    color_rgb=(1.0, 1.0, 1.0),
    color_per_point=None,
    opacity=0.95,
    scale_xy=0.008,
    scale_z=0.0012,
    scale_per_point=None,
):
    """
    Write GaussForge-compatible binary PLY (includes f_rest_0..44).
    color_per_point: (n,3) per-point RGB [0,1]; if None, use color_rgb.
    scale_per_point: (n,3) per-point scale_0,1,2 (log space); if None, use uniform scale_xy/scale_z.
    """
    n = len(x)
    F_REST = 45
    header_lines = [
        "ply", "format binary_little_endian 1.0",
        "comment GaussForge text",
        f"element vertex {n}",
        "property float x", "property float y", "property float z",
        "property float nx", "property float ny", "property float nz",
        "property float f_dc_0", "property float f_dc_1", "property float f_dc_2",
    ]
    for i in range(F_REST):
        header_lines.append(f"property float f_rest_{i}")
    header_lines.extend([
        "property float opacity", "property float scale_0", "property float scale_1", "property float scale_2",
        "property float rot_0", "property float rot_1", "property float rot_2", "property float rot_3",
        "end_header",
    ])
    header = "\n".join(header_lines) + "\n"
    header_bytes = header.encode("ascii")

    opac_val = inverse_sigmoid(opacity)
    qw, qx, qy, qz = 1.0, 0.0, 0.0, 0.0
    nx, ny, nz = 0.0, 0.0, 1.0

    if color_per_point is None:
        r, g, b = rgb_to_f_dc(*color_rgb)
        f_dc = np.zeros((n, 3), dtype=np.float32)
        f_dc[:, 0], f_dc[:, 1], f_dc[:, 2] = r, g, b
    else:
        # Per-point RGB [0,1] -> f_dc
        rgb = np.clip(color_per_point.astype(np.float64), 0, 1)
        f_dc = (rgb - 0.5) / SH_C0
        f_dc = f_dc.astype(np.float32)

    if scale_per_point is None:
        log_sx = math.log(max(1e-8, scale_xy))
        log_sz = math.log(max(1e-8, scale_z))
        scale_per_point = np.array([[log_sx, log_sx, log_sz]] * n, dtype=np.float32)

    with open(out_path, "wb") as f:
        f.write(header_bytes)
        for i in range(n):
            f.write(struct.pack("<3f", float(x[i]), float(y[i]), float(z[i])))
            f.write(struct.pack("<3f", nx, ny, nz))
            f.write(struct.pack("<3f", float(f_dc[i, 0]), float(f_dc[i, 1]), float(f_dc[i, 2])))
            f.write(struct.pack(f"<{F_REST}f", *([0.0] * F_REST)))
            f.write(struct.pack("<f", opac_val))
            f.write(struct.pack("<3f", float(scale_per_point[i, 0]), float(scale_per_point[i, 1]), float(scale_per_point[i, 2])))
            f.write(struct.pack("<4f", qw, qx, qy, qz))
    return n


def _hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip("#")
    if len(hex_str) >= 6:
        return (
            int(hex_str[0:2], 16) / 255.0,
            int(hex_str[2:4], 16) / 255.0,
            int(hex_str[4:6], 16) / 255.0,
        )
    return (1.0, 1.0, 1.0)


def main():
    text = "GaussForge"
    out_path = Path("output_gaussians.ply")
    font_path = None
    font_size = 150
    step = 2
    color_hex = None
    color_top_hex = "#87CEEB"
    color_bottom_hex = "#FFD700"
    gradient = True

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "-o" and i + 1 < len(args):
            out_path = Path(args[i + 1])
            i += 2
        elif args[i] == "--font" and i + 1 < len(args):
            font_path = args[i + 1]
            i += 2
        elif args[i] == "--size" and i + 1 < len(args):
            font_size = int(args[i + 1])
            i += 2
        elif args[i] == "--step" and i + 1 < len(args):
            step = int(args[i + 1])
            i += 2
        elif args[i] == "--color" and i + 1 < len(args):
            color_hex = args[i + 1]
            gradient = False
            i += 2
        elif args[i] == "--color-top" and i + 1 < len(args):
            color_top_hex = args[i + 1]
            i += 2
        elif args[i] == "--color-bottom" and i + 1 < len(args):
            color_bottom_hex = args[i + 1]
            i += 2
        elif not args[i].startswith("-"):
            text = args[i]
            i += 1
        else:
            i += 1

    if not font_path:
        for candidate in [
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/SFNSText.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        ]:
            if os.path.isfile(candidate):
                font_path = candidate
                break

    x, y, z = get_text_points(text, font_path=font_path, font_size=font_size, step=step)
    if len(x) == 0:
        print("Error: No pixels obtained; check font and text", file=sys.stderr)
        sys.exit(1)

    n0 = len(x)
    # Add layers behind each point for thickness; each layer with slight random offset
    num_layers = 0
    layer_step = 0.1
    jitter_xy = 0.004
    jitter_z = 0.004
    new_x, new_y, new_z = [], [], []
    for i in range(n0):
        new_x.append(x[i])
        new_y.append(y[i])
        new_z.append(z[i])
        for k in range(1, num_layers + 1):
            new_x.append(x[i] + np.random.uniform(-jitter_xy, jitter_xy))
            new_y.append(y[i] + np.random.uniform(-jitter_xy, jitter_xy))
            new_z.append(z[i] - k * layer_step + np.random.uniform(-jitter_z, jitter_z))
    x = np.array(new_x, dtype=np.float32)
    y = np.array(new_y, dtype=np.float32)
    z = np.array(new_z, dtype=np.float32)
    n = len(x)

    # Per-point color: gradient (top->bottom) + slight random
    if gradient:
        r_top, g_top, b_top = _hex_to_rgb(color_top_hex)
        r_bot, g_bot, b_bot = _hex_to_rgb(color_bottom_hex)
        y_min, y_max = float(y.min()), float(y.max())
        y_range = y_max - y_min + 1e-8
        t = ((y - y_min) / y_range).astype(np.float64)
        noise = np.random.uniform(-0.06, 0.06, (n, 3))
        rgb = np.stack([
            (1 - t) * r_top + t * r_bot + noise[:, 0],
            (1 - t) * g_top + t * g_bot + noise[:, 1],
            (1 - t) * b_top + t * b_bot + noise[:, 2],
        ], axis=1)
        color_per_point = np.clip(rgb, 0, 1).astype(np.float32)
        color_rgb = (0.5, 0.5, 0.5)
    else:
        color_per_point = None
        color_rgb = _hex_to_rgb(color_hex or "#FFFFFF")

    # Per-point scale slightly random for more 3D Gaussian look
    scale_xy_base = 0.016
    scale_z_base = 0.04
    scale_xy = scale_xy_base * (0.72 + 0.56 * np.random.rand(n)).astype(np.float32)
    scale_z = scale_z_base * (0.65 + 0.70 * np.random.rand(n)).astype(np.float32)
    scale_per_point = np.stack([
        np.log(np.maximum(scale_xy, 1e-8)),
        np.log(np.maximum(scale_xy, 1e-8)),
        np.log(np.maximum(scale_z, 1e-8)),
    ], axis=1)

    n = write_ply_binary_gaussforge(
        str(out_path), x, y, z,
        color_rgb=color_rgb,
        color_per_point=color_per_point,
        scale_xy=scale_xy_base,
        scale_z=scale_z_base,
        scale_per_point=scale_per_point,
    )
    print(f"Generated {n} Gaussian points -> {out_path} (sparse step={step}, gradient+3D)")


if __name__ == "__main__":
    main()