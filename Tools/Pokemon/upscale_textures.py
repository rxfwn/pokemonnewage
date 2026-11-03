"""Agrandit les textures des modèles Pokémon avec Real-ESRGAN (modèle « anime 6B », adapté aux textures dessinées).

Les modèles du dépôt Pokemon-3D-api ont des textures de 64 à 1024 px : en gros plan (évolution, Pokédex) on voit
les pixels. Chaque texture est agrandie ×4 (deux passes si elle reste sous TARGET), puis plafonnée à MAX_SIZE.
La transparence (alpha) est agrandie à part, en bicubique.

    pip install torch pillow numpy
    curl -L -o RealESRGAN_x4plus_anime_6B.pth \
        https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth
    python Tools/Pokemon/upscale_textures.py <poids .pth> <dossier d'entrée> <dossier de sortie>

Utilisé aussi directement par LGPE/lgpe_to_glb.py (option --upscale <poids>) pour les modèles Let's Go.
"""

import os
import sys

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image

TARGET = 2048
MAX_SIZE = 4096
TILE = 256


class ResidualDenseBlock(nn.Module):
    def __init__(self, nf=64, gc=32):
        super().__init__()
        self.conv1 = nn.Conv2d(nf, gc, 3, 1, 1)
        self.conv2 = nn.Conv2d(nf + gc, gc, 3, 1, 1)
        self.conv3 = nn.Conv2d(nf + 2 * gc, gc, 3, 1, 1)
        self.conv4 = nn.Conv2d(nf + 3 * gc, gc, 3, 1, 1)
        self.conv5 = nn.Conv2d(nf + 4 * gc, nf, 3, 1, 1)
        self.lrelu = nn.LeakyReLU(0.2, True)

    def forward(self, x):
        x1 = self.lrelu(self.conv1(x))
        x2 = self.lrelu(self.conv2(torch.cat((x, x1), 1)))
        x3 = self.lrelu(self.conv3(torch.cat((x, x1, x2), 1)))
        x4 = self.lrelu(self.conv4(torch.cat((x, x1, x2, x3), 1)))
        x5 = self.conv5(torch.cat((x, x1, x2, x3, x4), 1))
        return x5 * 0.2 + x


class RRDB(nn.Module):
    def __init__(self, nf, gc=32):
        super().__init__()
        self.rdb1 = ResidualDenseBlock(nf, gc)
        self.rdb2 = ResidualDenseBlock(nf, gc)
        self.rdb3 = ResidualDenseBlock(nf, gc)

    def forward(self, x):
        return self.rdb3(self.rdb2(self.rdb1(x))) * 0.2 + x


class RRDBNet(nn.Module):
    def __init__(self, num_block=6, nf=64):
        super().__init__()
        self.conv_first = nn.Conv2d(3, nf, 3, 1, 1)
        self.body = nn.Sequential(*[RRDB(nf) for _ in range(num_block)])
        self.conv_body = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_up1 = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_up2 = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_hr = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_last = nn.Conv2d(nf, 3, 3, 1, 1)
        self.lrelu = nn.LeakyReLU(0.2, True)

    def forward(self, x):
        feat = self.conv_first(x)
        feat = feat + self.conv_body(self.body(feat))
        feat = self.lrelu(self.conv_up1(F.interpolate(feat, scale_factor=2, mode="nearest")))
        feat = self.lrelu(self.conv_up2(F.interpolate(feat, scale_factor=2, mode="nearest")))
        return self.conv_last(self.lrelu(self.conv_hr(feat)))


def load_model(weights):
    model = RRDBNet()
    state = torch.load(weights, map_location="cpu")
    model.load_state_dict(state.get("params_ema", state.get("params", state)), strict=True)
    return model.eval()


@torch.no_grad()
def upscale_rgb(model, rgb):
    """rgb : tableau HxWx3 float 0-1. Par tuiles (mémoire), avec marge pour éviter les coutures. Les bords
    sont prolongés en boucle : les textures se répètent souvent (UV qui débordent)."""
    h, w, _ = rgb.shape
    pad = 16
    src = torch.from_numpy(rgb.transpose(2, 0, 1)).unsqueeze(0)
    src = F.pad(src, (pad, pad, pad, pad), mode="circular" if min(h, w) > pad else "replicate")
    out = torch.zeros(1, 3, h * 4, w * 4)
    for y in range(0, h, TILE):
        for x in range(0, w, TILE):
            th, tw = min(TILE, h - y), min(TILE, w - x)
            tile = src[:, :, y:y + th + 2 * pad, x:x + tw + 2 * pad]
            res = model(tile)
            out[:, :, y * 4:(y + th) * 4, x * 4:(x + tw) * 4] = res[:, :, pad * 4:(pad + th) * 4, pad * 4:(pad + tw) * 4]
    return out.squeeze(0).clamp(0, 1).numpy().transpose(1, 2, 0)


def upscale_image(model, img):
    has_alpha = img.mode in ("RGBA", "LA") or "transparency" in img.info
    img = img.convert("RGBA" if has_alpha else "RGB")
    alpha = img.getchannel("A") if has_alpha else None
    rgb = np.asarray(img.convert("RGB"), dtype=np.float32) / 255.0
    while True:
        rgb = upscale_rgb(model, rgb)
        if max(rgb.shape[:2]) >= TARGET or max(rgb.shape[:2]) * 4 > MAX_SIZE * 2:
            break
    out = Image.fromarray((rgb * 255.0 + 0.5).astype(np.uint8))
    scale = min(1.0, MAX_SIZE / max(out.size))
    if scale < 1.0:
        out = out.resize((round(out.width * scale), round(out.height * scale)), Image.LANCZOS)
    if alpha is not None:
        out.putalpha(alpha.resize(out.size, Image.BICUBIC))
    return out


def main():
    weights, src_dir, dst_dir = sys.argv[1:4]
    torch.set_num_threads(os.cpu_count() or 4)
    model = load_model(weights)
    os.makedirs(dst_dir, exist_ok=True)
    for name in sorted(os.listdir(src_dir)):
        if not name.lower().endswith(".png"):
            continue
        dst = os.path.join(dst_dir, name)
        if os.path.exists(dst):
            continue
        img = Image.open(os.path.join(src_dir, name))
        out = upscale_image(model, img)
        out.save(dst, optimize=True)
        print(f"{name} : {img.size[0]}x{img.size[1]} -> {out.size[0]}x{out.size[1]}", flush=True)


if __name__ == "__main__":
    main()
