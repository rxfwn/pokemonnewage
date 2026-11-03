"""Vue en perspective d'une heightmap (algorithme « voxel space »), pour les aperçus de relief_vues.py.

Usage : python apercu_vue_sol.py relief.npy prefixe (relief.npy = grille de 3 m écrite par relief_vues.py --test)."""
import sys, math, numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, '/home/claude/pokemonnewage/Tools/Blockout')
import kanto_v1_layout as L

def prepare(h, step):
    gy, gx = np.gradient(h, step)
    slope = np.hypot(gx, gy)
    n = np.dstack([-gx, -gy, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    sun = np.array([0.5, -0.6, 0.62]); sun /= np.linalg.norm(sun)   # (y est, x nord) -> composantes [row(y), col(x)]
    shade = np.clip(n[..., 1] * sun[0] + n[..., 0] * sun[1] + n[..., 2] * sun[2], 0, 1)
    rng = np.random.default_rng(1)
    noise = ndimage.gaussian_filter(rng.standard_normal(h.shape), 6)
    noise /= noise.std()
    grass = np.array([95, 140, 60]) + noise[..., None] * np.array([10, 12, 6])
    forest = np.array([45, 85, 40]) + noise[..., None] * np.array([6, 8, 5])
    rock = np.array([130, 122, 110]) + noise[..., None] * 8
    snow = np.array([235, 238, 242])
    sand = np.array([205, 190, 150])
    col = grass.copy()
    f = np.clip((noise - 0.2) * 2, 0, 1)[..., None] * np.clip((slope[..., None] - 0.08) * 6, 0, 1)
    col = col * (1 - f) + forest * f
    r = np.clip((slope - 0.95) * 2.5, 0, 1)[..., None] + np.clip((h - 430) / 90, 0, 1)[..., None] * 0.6
    r = np.clip(r, 0, 1)
    col = col * (1 - r) + rock * r
    s = np.clip((h - 470) / 50, 0, 1)[..., None] * np.clip(1 - (slope[..., None] - 0.9), 0, 1)
    col = col * (1 - s) + snow * s
    b = np.clip((2.5 - h) / 2, 0, 1)[..., None]
    col = col * (1 - b) + sand * b
    col = col * (0.45 + 0.7 * shade[..., None])
    water = h < 0
    depth = np.clip(-h / 12, 0, 1)[..., None]
    wcol = np.array([60, 140, 160]) * (1 - depth) + np.array([25, 70, 120]) * depth
    col = np.where(water[..., None], wcol, col)
    return np.clip(col, 0, 255), np.maximum(h, 0)

def render(col, hz, step, cam_x, cam_y, eye, yaw_deg, pitch=0.0, fov=75, W=1280, H=720, far=6500):
    yaw = math.radians(yaw_deg)  # 0 = nord (+x), 90 = est (+y)
    rows, cols = hz.shape
    def at(x, y):
        c = np.clip(x / step, 0, cols - 1.001); r = np.clip(y / step, 0, rows - 1.001)
        return r.astype(int), c.astype(int)
    r0, c0 = at(np.array(cam_x), np.array(cam_y))
    cam_h = hz[r0, c0] + eye
    focal = (W / 2) / math.tan(math.radians(fov) / 2)
    horizon = H / 2 + pitch * focal
    ang = yaw + np.arctan((np.arange(W) - W / 2) / focal)
    corr = np.cos(ang - yaw)
    dx, dy = np.cos(ang), np.sin(ang)
    sky_top, sky_bot = np.array([90, 150, 220]), np.array([200, 225, 240])
    t = np.clip(np.arange(H) / horizon, 0, 1)[:, None]
    img = (sky_top * (1 - t) + sky_bot * t)[:, None, :].repeat(W, 1).astype(float)
    ybuf = np.full(W, H, float)
    z = 1.0
    fog_col = np.array([195, 215, 232])
    while z < far:
        x = cam_x + dx * z; y = cam_y + dy * z
        inside = (x >= 0) & (x < cols * step) & (y >= 0) & (y < rows * step)
        r, c = at(x, y)
        hgt = np.where(inside, hz[r, c], 0.0)
        cl = np.where(inside[:, None], col[r, c], np.array([25, 70, 120]))
        sy = horizon - (hgt - cam_h) / (z * corr) * focal
        fog = 1 - math.exp(-z / 3200)
        cl = cl * (1 - fog) + fog_col * fog
        for i in np.nonzero(sy < ybuf)[0]:
            top = max(int(sy[i]), 0)
            img[top:int(ybuf[i]), i] = cl[i]
            ybuf[i] = top
        z += 0.6 + z * 0.006
    return Image.fromarray(img.astype(np.uint8))

if __name__ == '__main__':
    src, out = sys.argv[1], sys.argv[2]
    h = np.load(src).astype(np.float64); step = 3.0
    col, hz = prepare(h, step)
    views = [
        ("route1_vers_jadielle", 144, 505, 3.0, 0, 0.0),
        ("route22_vers_indigo", 110, 404, 3.0, -20, -0.05),
        ("argenta_vers_mont_selenite", 175, 160, 3.0, 80, 0.0),
        ("route24_vers_la_mer", 576, 55, 3.0, 200, 0.05),
        ("vue_avion_du_sud", 400, 900, 700, 0, -0.35),
    ]
    for name, c, r, eye, yaw, pitch in views:
        x, y = L.from_frlg(c, r)
        render(col, hz, step, x, y, eye, yaw, pitch).save(f"{out}_{name}.png")
        print(name)
