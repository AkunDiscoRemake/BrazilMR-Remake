#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BrazilMR — gerador do modelo GLB de demonstração/teste.

Cria um GLB 2.0 válido contendo:
  • mesh (placa curvada "logo" com POSITION/NORMAL/TEXCOORD_0 + índices)
  • material PBR metallic-roughness com textura embutida (PNG gerado aqui)
  • animação de rotação (quaternion keyframes, LINEAR)
  • min/max nos accessors (conforme spec glTF)

Saída:
  app/src/main/assets/models/brazilmr_logo.glb  (demo no ambiente VR)
  app/src/main/cpp/host_tests/test_model.glb    (teste do parser nativo)

Sem dependências externas (PNG via zlib puro).
"""
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# PNG mínimo (RGBA8)
# ---------------------------------------------------------------------------
def png_crc(data: bytes) -> bytes:
    return struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)

def make_png(width: int, height: int, pixel_fn) -> bytes:
    """pixel_fn(x, y) -> (r, g, b, a)"""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filtro: none
        for x in range(width):
            r, g, b, a = pixel_fn(x, y)
            raw += bytes((r, g, b, a))
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))

# ---------------------------------------------------------------------------
# Textura do logo: gradiente escuro + "lente dupla" verde
# ---------------------------------------------------------------------------
def logo_pixel(x, y, w=128, h=128):
    cx, cy = w * 0.5, h * 0.5
    # fundo
    t = y / h
    base = (int(18 + 20 * t), int(24 + 22 * t), int(38 + 30 * t), 255)
    # duas lentes
    for lx in (cx - 26, cx + 26):
        d = ((x - lx) ** 2 + (y - cy) ** 2) ** 0.5
        if d < 16:
            ring = 13 < d < 16
            if ring:
                return (0, 229, 160, 255)
            return (10, 14, 26, 255)
    # faixa
    if cy + 34 < y < cy + 42:
        return (0, 180, 128, 255)
    return base

# ---------------------------------------------------------------------------
# Geometria: placa levemente curvada (grid 8x8), similar a um painel VR
# ---------------------------------------------------------------------------
GRID = 8
W_M, H_M = 0.9, 0.55

def build_mesh():
    positions, normals, uvs = [], [], []
    for j in range(GRID + 1):
        for i in range(GRID + 1):
            u, v = i / GRID, j / GRID
            x = (u - 0.5) * W_M
            y = (0.5 - v) * H_M
            z = -0.10 * (x * x + y * y)  # curvatura suave
            positions += [x, y, z]
            # normal aproximada
            dzdx = -0.20 * x
            dzdy = -0.20 * y
            nlen = (1 + dzdx * dzdx + dzdy * dzdy) ** 0.5
            normals += [dzdx / nlen, dzdy / nlen, 1.0 / nlen]
            uvs += [u, v]
    indices = []
    for j in range(GRID):
        for i in range(GRID):
            a = j * (GRID + 1) + i
            b = a + 1
            c = a + (GRID + 1)
            d = c + 1
            indices += [a, c, b, b, c, d]
    return positions, normals, uvs, indices

# ---------------------------------------------------------------------------
# Montagem do GLB
# ---------------------------------------------------------------------------
def build_glb() -> bytes:
    png = make_png(128, 128, logo_pixel)
    positions, normals, uvs, indices = build_mesh()

    # animação de rotação (yaw ±20° em 4 keyframes, loop)
    import math
    anim_times = [0.0, 1.0, 2.0, 3.0]
    anim_quats = []
    for k in range(4):
        yaw = math.radians(20.0 * math.sin(k * math.pi / 2.0))
        cy, sy = math.cos(yaw / 2), math.sin(yaw / 2)
        anim_quats += [0.0, 0.0, sy, cy]

    # ordena buffers com alinhamento de 4
    chunks = []
    views = []
    offset = 0

    def add_view(data: bytes, target: int = 0):
        nonlocal offset
        pad = (-len(data)) % 4
        data = data + b" " * pad
        idx = len(views)
        views.append({
            "buffer": 0, "byteOffset": offset,
            "byteLength": len(data), "target": target,
        })
        chunks.append(data)
        offset += len(data)
        return idx

    v_indices = add_view(struct.pack(f"<{len(indices)}H", *indices), 34963)
    v_pos = add_view(struct.pack(f"<{len(positions)}f", *positions), 34962)
    v_nrm = add_view(struct.pack(f"<{len(normals)}f", *normals), 34962)
    v_uv = add_view(struct.pack(f"<{len(uvs)}f", *uvs), 34962)
    v_png = add_view(png)
    v_anim_t = add_view(struct.pack(f"<{len(anim_times)}f", *anim_times))
    v_anim_q = add_view(struct.pack(f"<{len(anim_quats)}f", *anim_quats))

    num_verts = len(positions) // 3
    xs = positions[0::3]; ys = positions[1::3]; zs = positions[2::3]

    gltf = {
        "asset": {"version": "2.0", "generator": "BrazilMR gen_test_glb.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "bufferViews": views,
        "nodes": [{
            "name": "BrazilMRLogo",
            "mesh": 0,
            "translation": [0, 0, 0],
            "rotation": [0, 0, 0, 1],
            "scale": [1, 1, 1],
        }],
        "meshes": [{
            "name": "logo_plate",
            "primitives": [{
                "attributes": {"POSITION": 1, "NORMAL": 2, "TEXCOORD_0": 3},
                "indices": 0,
                "material": 0,
                "mode": 4,
            }],
        }],
        "materials": [{
            "name": "logo_mat",
            "pbrMetallicRoughness": {
                "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.2,
                "roughnessFactor": 0.45,
            },
            "doubleSided": True,
        }],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"bufferView": v_png, "mimeType": "image/png", "name": "logo"}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "accessors": [
            {  # 0: indices
                "bufferView": v_indices, "componentType": 5123,
                "count": len(indices), "type": "SCALAR",
            },
            {  # 1: positions
                "bufferView": v_pos, "componentType": 5126,
                "count": num_verts, "type": "VEC3",
                "min": [min(xs), min(ys), min(zs)],
                "max": [max(xs), max(ys), max(zs)],
            },
            {  # 2: normals
                "bufferView": v_nrm, "componentType": 5126,
                "count": num_verts, "type": "VEC3",
            },
            {  # 3: uvs
                "bufferView": v_uv, "componentType": 5126,
                "count": num_verts, "type": "VEC2",
            },
            {  # 4: anim times
                "bufferView": v_anim_t, "componentType": 5126,
                "count": len(anim_times), "type": "SCALAR",
            },
            {  # 5: anim quats
                "bufferView": v_anim_q, "componentType": 5126,
                "count": len(anim_times), "type": "VEC4",
            },
        ],
        "animations": [{
            "name": "spin",
            "channels": [{
                "sampler": 0,
                "target": {"node": 0, "path": "rotation"},
            }],
            "samplers": [{
                "input": 4, "output": 5, "interpolation": "LINEAR",
            }],
        }],
        "buffers": [{"byteLength": offset}],
    }

    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    json_pad = (-len(json_bytes)) % 4
    json_bytes += b" " * json_pad

    binary = b"".join(chunks)
    bin_pad = (-len(binary)) % 4
    binary += b"\x00" * bin_pad

    glb = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(json_bytes) + 8 + len(binary))
    glb += struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    glb += struct.pack("<II", len(binary), 0x004E4942) + binary
    return glb

def main():
    glb = build_glb()
    for out in [
        ROOT / "app/src/main/assets/models/brazilmr_logo.glb",
        ROOT / "app/src/main/cpp/host_tests/test_model.glb",
    ]:
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(glb)
        print(f"ok: {out} ({len(glb)} bytes)")

if __name__ == "__main__":
    sys.exit(main())
