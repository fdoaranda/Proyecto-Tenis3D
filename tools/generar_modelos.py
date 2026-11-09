# -*- coding: utf-8 -*-
"""
Generador de modelos (.obj/.mtl) y texturas (.png) del proyecto
Escenario Deportivo 3D Interactivo - Tenis.

Uso (desde la carpeta del proyecto):  python tools/generar_modelos.py
Escribe todo en la carpeta Models/.
Unidades: 1 unidad = 1 metro. Convencion OBJ: v = 1 es la parte superior de la imagen.
"""
import math, os, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Models')
os.makedirs(OUT, exist_ok=True)
random.seed(7)
np.random.seed(7)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def font(sz):
    try:
        return ImageFont.truetype(FONT, sz)
    except Exception:
        return ImageFont.load_default()


# ----------------------------------------------------------------------------
# Utilidades de malla
# ----------------------------------------------------------------------------
def norm(v):
    v = np.array(v, dtype=float)
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


class Mesh:
    def __init__(self, name):
        self.name = name
        self.v, self.vt, self.vn = [], [], []
        self.groups = {}       # material -> lista de caras
        self.mats = {}         # material -> archivo de textura

    def material(self, name, tex):
        self.mats[name] = tex

    def _add(self, mat, pts, uvs, nrm):
        face = []
        for p, uv in zip(pts, uvs):
            self.v.append(p)
            self.vt.append(uv)
            self.vn.append(nrm)
            i = len(self.v)
            face.append(i)
        self.groups.setdefault(mat, []).append(face)

    def quad(self, mat, p0, p1, p2, p3, uv=(0, 0, 1, 1), n=None):
        """p0..p3 en sentido antihorario visto desde afuera. uv=(u0,v0,u1,v1)."""
        p = [np.array(x, dtype=float) for x in (p0, p1, p2, p3)]
        if n is None:
            n = norm(np.cross(p[1] - p[0], p[2] - p[0]))
        u0, v0, u1, v1 = uv
        uvs = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
        self._add(mat, [p[0], p[1], p[2]], [uvs[0], uvs[1], uvs[2]], n)
        self._add(mat, [p[0], p[2], p[3]], [uvs[0], uvs[2], uvs[3]], n)

    def write(self):
        base = os.path.join(OUT, self.name)
        with open(base + '.obj', 'w') as f:
            f.write('# generado por tools/generar_modelos.py\n')
            f.write('mtllib %s.mtl\n' % self.name)
            for p in self.v:
                f.write('v %.5f %.5f %.5f\n' % tuple(p))
            for t in self.vt:
                f.write('vt %.5f %.5f\n' % tuple(t))
            for n in self.vn:
                f.write('vn %.5f %.5f %.5f\n' % tuple(n))
            for m, faces in self.groups.items():
                f.write('usemtl %s\n' % m)
                for fc in faces:
                    f.write('f ' + ' '.join('%d/%d/%d' % (i, i, i) for i in fc) + '\n')
        with open(base + '.mtl', 'w') as f:
            for m, tex in self.mats.items():
                f.write('newmtl %s\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\nNs 8\n' % m)
                f.write('map_Kd %s\n\n' % tex)


def box(mesh, mat, c, s, uv=(0, 0, 1, 1), faces='all', uvf=None):
    """Caja centrada en c con tamano s=(sx,sy,sz). uvf: dict cara->uv."""
    cx, cy, cz = c
    hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
    P = lambda x, y, z: (cx + x * hx, cy + y * hy, cz + z * hz)
    defs = {
        '+z': ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)),
        '-z': ((1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)),
        '+x': ((1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)),
        '-x': ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)),
        '+y': ((-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)),
        '-y': ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),
    }
    for k, pts in defs.items():
        if faces != 'all' and k not in faces:
            continue
        m = mat[k] if isinstance(mat, dict) else mat
        u = uvf.get(k, uv) if uvf else uv
        mesh.quad(m, *[P(*p) for p in pts], uv=u)


def basis(axis):
    axis = norm(axis)
    t = np.array([1.0, 0, 0]) if abs(axis[0]) < 0.9 else np.array([0, 1.0, 0])
    a = norm(np.cross(axis, t))
    b = np.cross(axis, a)
    return a, b


def cyl(mesh, mat, p0, p1, r0, r1=None, segs=14, uv=(0, 0, 1, 1), caps=True):
    """Cilindro/cono truncado de p0 a p1."""
    if r1 is None:
        r1 = r0
    p0 = np.array(p0, dtype=float)
    p1 = np.array(p1, dtype=float)
    ax = p1 - p0
    a, b = basis(ax)
    u0, v0, u1, v1 = uv
    for i in range(segs):
        t0 = 2 * math.pi * i / segs
        t1 = 2 * math.pi * (i + 1) / segs
        d0 = math.cos(t0) * a + math.sin(t0) * b
        d1 = math.cos(t1) * a + math.sin(t1) * b
        q = [p0 + d0 * r0, p0 + d1 * r0, p1 + d1 * r1, p1 + d0 * r1]
        ua = u0 + (u1 - u0) * i / segs
        ub = u0 + (u1 - u0) * (i + 1) / segs
        n0, n1 = d0, d1
        mesh._add(mat, [q[0], q[1], q[2]], [(ua, v0), (ub, v0), (ub, v1)], norm(n0 + n1))
        mesh._add(mat, [q[0], q[2], q[3]], [(ua, v0), (ub, v1), (ua, v1)], norm(n0 + n1))
        if caps:
            nu = (u0 + u1) / 2
            nv = (v0 + v1) / 2
            mesh._add(mat, [p0, p0 + d1 * r0, p0 + d0 * r0], [(nu, nv)] * 3, -norm(ax))
            mesh._add(mat, [p1, p1 + d0 * r1, p1 + d1 * r1], [(nu, nv)] * 3, norm(ax))


def sphere(mesh, mat, c, r, lat=10, lon=16, uv=(0, 0, 1, 1), full=True):
    """Esfera/elipsoide. r puede ser escalar o tupla (rx,ry,rz)."""
    if not isinstance(r, (tuple, list)):
        r = (r, r, r)
    c = np.array(c, dtype=float)
    u0, v0, u1, v1 = uv

    def pt(i, j):
        th = math.pi * i / lat
        ph = 2 * math.pi * j / lon
        d = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)])
        return c + d * np.array(r), d, (u0 + (u1 - u0) * j / lon, v1 - (v1 - v0) * i / lat)

    for i in range(lat):
        if not full and i >= lat // 2:
            break
        for j in range(lon):
            a, na, ua = pt(i, j)
            b, nb, ub = pt(i, j + 1)
            cc, nc, uc = pt(i + 1, j + 1)
            d, nd, ud = pt(i + 1, j)
            mesh._add(mat, [a, d, cc], [ua, ud, uc], norm(na + nd + nc))
            mesh._add(mat, [a, cc, b], [ua, uc, ub], norm(na + nc + nb))


def ring(mesh, mat, c, a, b, tr, segs=36, tseg=8, uv=(0, 0, 1, 1)):
    """Tubo de radio tr siguiendo una elipse (a,b) en el plano XY (marco de raqueta)."""
    c = np.array(c, dtype=float)
    u0, v0, u1, v1 = uv
    for i in range(segs):
        th0 = 2 * math.pi * i / segs
        th1 = 2 * math.pi * (i + 1) / segs
        e0 = c + np.array([a * math.cos(th0), b * math.sin(th0), 0])
        e1 = c + np.array([a * math.cos(th1), b * math.sin(th1), 0])
        r0 = norm([math.cos(th0) * b, math.sin(th0) * a, 0])
        r1 = norm([math.cos(th1) * b, math.sin(th1) * a, 0])
        for k in range(tseg):
            f0 = 2 * math.pi * k / tseg
            f1 = 2 * math.pi * (k + 1) / tseg
            def P(e, r, f):
                n = math.cos(f) * r + math.sin(f) * np.array([0, 0, 1.0])
                return e + n * tr, n
            p00, n00 = P(e0, r0, f0)
            p01, n01 = P(e0, r0, f1)
            p10, n10 = P(e1, r1, f0)
            p11, n11 = P(e1, r1, f1)
            mid = (u0 + u1) / 2, (v0 + v1) / 2
            mesh._add(mat, [p00, p10, p11], [mid] * 3, norm(n00 + n10 + n11))
            mesh._add(mat, [p00, p11, p01], [mid] * 3, norm(n00 + n11 + n01))


# ----------------------------------------------------------------------------
# Texturas
# ----------------------------------------------------------------------------
def save(img, name):
    img.save(os.path.join(OUT, name))


def noise(w, h, scale=1.0, seed=0):
    rng = np.random.RandomState(seed)
    return rng.rand(h, w) * scale


def blur(arr, r):
    im = Image.fromarray((np.clip(arr, 0, 1) * 255).astype('uint8'))
    im = im.filter(ImageFilter.GaussianBlur(r))
    return np.asarray(im).astype(float) / 255.0


def tex_grass():
    w = h = 512
    n1 = blur(noise(w, h, 1, 1), 2)
    n2 = blur(noise(w, h, 1, 2), 6)
    n3 = noise(w, h, 1, 3)
    base = np.zeros((h, w, 3))
    g = 0.30 + 0.35 * n1 + 0.2 * n2 + 0.12 * n3
    base[..., 0] = 0.10 + 0.14 * g
    base[..., 1] = 0.25 + 0.34 * g
    base[..., 2] = 0.07 + 0.07 * g
    save(Image.fromarray((np.clip(base, 0, 1) * 255).astype('uint8')), 'grass_tenis.png')


def tex_court():
    W, H = 1024, 2048
    sc = W / 18.29
    img = Image.new('RGB', (W, H), (58, 138, 98))
    d = ImageDraw.Draw(img)
    X = lambda x: (x + 9.145) * sc
    Z = lambda z: (18.285 - z) * sc
    d.rectangle([X(-5.485), Z(11.885), X(5.485), Z(-11.885)], fill=(47, 102, 170))
    arr = np.asarray(img).astype(float)
    arr += (np.random.randn(H, W, 1) * 5.0)
    img = Image.fromarray(np.clip(arr, 0, 255).astype('uint8'))
    d = ImageDraw.Draw(img)
    wl = 6
    L = (240, 240, 240)

    def line(x0, z0, x1, z1):
        a, b = X(x0), Z(z0)
        c, e = X(x1), Z(z1)
        d.rectangle([min(a, c) - wl / 2, min(b, e) - wl / 2, max(a, c) + wl / 2, max(b, e) + wl / 2], fill=L)

    line(-5.485, 11.885, 5.485, 11.885)
    line(-5.485, -11.885, 5.485, -11.885)
    line(-5.485, -11.885, -5.485, 11.885)
    line(5.485, -11.885, 5.485, 11.885)
    line(-4.115, -11.885, -4.115, 11.885)
    line(4.115, -11.885, 4.115, 11.885)
    line(-4.115, 6.40, 4.115, 6.40)
    line(-4.115, -6.40, 4.115, -6.40)
    line(0, -6.40, 0, 6.40)
    line(0, 11.885, 0, 11.6)
    line(0, -11.885, 0, -11.6)
    save(img, 'cancha_tenis.png')


def tex_ad(name, text, bg, fg, sub=None):
    img = Image.new('RGB', (1024, 256), bg)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 1023, 255], outline=fg, width=6)
    f = font(120)
    tw = d.textlength(text, font=f)
    d.text(((1024 - tw) / 2, 40 if sub else 62), text, font=f, fill=fg)
    if sub:
        f2 = font(46)
        tw = d.textlength(sub, font=f2)
        d.text(((1024 - tw) / 2, 185), sub, font=f2, fill=fg)
    save(img, name)


def tex_concrete():
    n = blur(noise(256, 256, 1, 5), 1.2)
    a = 0.55 + 0.25 * n + 0.08 * noise(256, 256, 1, 6)
    arr = np.stack([a, a, a * 1.02], axis=-1)
    save(Image.fromarray((np.clip(arr, 0, 1) * 255).astype('uint8')), 'concreto.png')


def tex_seats():
    img = Image.new('RGB', (256, 64), (30, 70, 140))
    d = ImageDraw.Draw(img)
    cols = [(30, 70, 140), (240, 200, 40), (235, 235, 235), (30, 70, 140)]
    for i in range(8):
        d.rectangle([i * 32 + 3, 8, i * 32 + 29, 56], fill=cols[(i // 2) % 4] if i % 4 else (200, 40, 40))
    arr = np.asarray(img).astype(float) + np.random.randn(64, 256, 1) * 5
    save(Image.fromarray(np.clip(arr, 0, 255).astype('uint8')), 'asientos.png')


def tex_solid(name, rgb, noise_amp=6, size=64):
    arr = np.ones((size, size, 3)) * np.array(rgb, dtype=float) + np.random.randn(size, size, 1) * noise_amp
    save(Image.fromarray(np.clip(arr, 0, 255).astype('uint8')), name)


def tex_wood(name='madera.png'):
    w = h = 128
    arr = np.zeros((h, w, 3))
    for y in range(h):
        t = 0.5 + 0.5 * math.sin(y * 0.35 + np.random.rand() * 0.6)
        arr[y, :, :] = np.array([150, 100, 55]) * (0.75 + 0.25 * t)
    arr += np.random.randn(h, w, 1) * 4
    save(Image.fromarray(np.clip(arr, 0, 255).astype('uint8')), name)


def tex_net():
    img = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(8):
        d.line([i * 8, 0, i * 8, 63], fill=(25, 25, 25, 235), width=2)
        d.line([0, i * 8, 63, i * 8], fill=(25, 25, 25, 235), width=2)
    save(img, 'red_malla.png')


def tex_strings():
    img = Image.new('RGBA', (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(0, 128, 12):
        d.line([i, 0, i, 127], fill=(235, 235, 235, 255), width=2)
        d.line([0, i, 127, i], fill=(235, 235, 235, 255), width=2)
    save(img, 'cuerdas_raqueta.png')


def tex_ball():
    w, h = 256, 128
    img = Image.new('RGB', (w, h), (200, 220, 40))
    arr = np.asarray(img).astype(float) + np.random.randn(h, w, 1) * 7
    img = Image.fromarray(np.clip(arr, 0, 255).astype('uint8'))
    d = ImageDraw.Draw(img)
    for x0 in (64, 192):
        pts = [(x0 + 34 * math.sin(math.pi * t / 40), t * h / 40) for t in range(41)]
        d.line(pts, fill=(245, 245, 235), width=5)
    save(img, 'pelota_tenis.png')


def atlas(name, cells):
    """Atlas 256x128: 4x2 celdas de 64 px."""
    img = Image.new('RGB', (256, 128))
    d = ImageDraw.Draw(img)
    for i, c in enumerate(cells):
        x, y = (i % 4) * 64, (i // 4) * 64
        d.rectangle([x, y, x + 63, y + 63], fill=c)
    arr = np.asarray(img).astype(float) + np.random.randn(128, 256, 1) * 3
    save(Image.fromarray(np.clip(arr, 0, 255).astype('uint8')), name)


def cell(i, margin=0.04):
    """Rectangulo UV (u0,v0,u1,v1) de la celda i del atlas (v=1 arriba)."""
    cx, cy = i % 4, i // 4
    u0, u1 = cx / 4 + margin / 4, (cx + 1) / 4 - margin / 4
    v1 = 1 - cy / 2 - margin / 2
    v0 = 1 - (cy + 1) / 2 + margin / 2
    return (u0, v0, u1, v1)


SKIN, SHIRT, SHORTS, SHOES, HAIR, WHITE, DARK, ACC = [cell(i) for i in range(8)]


def tex_score_base():
    img = Image.new('RGB', (1024, 384), (14, 24, 48))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 1023, 383], outline=(240, 200, 40), width=8)
    d.text((30, 18), 'TENIS  FI-UNAM', font=font(54), fill=(240, 200, 40))
    d.text((610, 24), 'JUEGOS', font=font(40), fill=(220, 220, 220))
    d.text((820, 24), 'PUNTOS', font=font(40), fill=(220, 220, 220))
    d.line([20, 100, 1000, 100], fill=(240, 200, 40), width=4)
    d.text((30, 150), 'JUGADOR 1', font=font(70), fill=(90, 150, 255))
    d.text((30, 270), 'JUGADOR 2', font=font(70), fill=(255, 100, 100))
    save(img, 'marcador_base.png')


def tex_digit(name, text):
    img = Image.new('RGB', (128, 128), (14, 24, 48))
    d = ImageDraw.Draw(img)
    f = font(84 if len(text) == 1 else 70)
    tw = d.textlength(text, font=f)
    d.text(((128 - tw) / 2, 18), text, font=f, fill=(255, 235, 90))
    save(img, name)


# ----------------------------------------------------------------------------
# Modelos
# ----------------------------------------------------------------------------
def m_court():
    m = Mesh('Cancha')
    m.material('c', 'cancha_tenis.png')
    m.quad('c', (-9.145, 0.01, 18.285), (9.145, 0.01, 18.285), (9.145, 0.01, -18.285), (-9.145, 0.01, -18.285), n=(0, 1, 0))
    m.write()


def m_ground():
    m = Mesh('Suelo')
    m.material('g', 'grass_tenis.png')
    m.quad('g', (-90, 0, 90), (90, 0, 90), (90, 0, -90), (-90, 0, -90), uv=(0, 0, 70, 70), n=(0, 1, 0))
    m.write()


def m_adboard(name, tex):
    m = Mesh(name)
    m.material('ad', tex)
    m.material('dk', 'tablero_oscuro.png')
    L, H, T = 4.0, 0.95, 0.14
    box(m, {'+z': 'ad', '-z': 'dk', '+x': 'dk', '-x': 'dk', '+y': 'dk', '-y': 'dk'}, (0, H / 2, 0), (L, H, T))
    m.write()


def m_net():
    m = Mesh('Red')
    m.material('n', 'red_malla.png')
    m.material('t', 'tape_blanco.png')
    W, H = 12.8, 1.0
    m.quad('n', (-W / 2, 0.02, 0), (W / 2, 0.02, 0), (W / 2, H, 0), (-W / 2, H, 0), uv=(0, 0, 32, 2.5), n=(0, 0, 1))
    m.quad('n', (W / 2, 0.02, 0), (-W / 2, 0.02, 0), (-W / 2, H, 0), (W / 2, H, 0), uv=(0, 0, 32, 2.5), n=(0, 0, -1))
    box(m, 't', (0, H + 0.03, 0), (W, 0.08, 0.04), uv=(0, 0, 1, 1))
    box(m, 't', (0, 0.03, 0), (W, 0.04, 0.03))
    m.write()


def m_post():
    m = Mesh('PosteRed')
    m.material('m', 'metal_oscuro.png')
    cyl(m, 'm', (0, 0, 0), (0, 1.07, 0), 0.04, segs=12)
    box(m, 'm', (0, 0.0, 0), (0.18, 0.04, 0.18))
    m.write()


def m_stand():
    m = Mesh('Gradas')
    m.material('co', 'concreto.png')
    m.material('se', 'asientos.png')
    rows, depth, rise = 10, 0.85, 0.45
    x0, z0, z1 = 11.0, -18.5, 18.5
    zc, zl = (z0 + z1) / 2, z1 - z0
    for i in range(rows):
        top = (i + 1) * rise
        xc = x0 + i * depth + depth / 2
        box(m, {'+y': 'se', '-x': 'co', '+x': 'co', '+z': 'co', '-z': 'co', '-y': 'co'},
            (xc, top / 2, zc), (depth, top, zl),
            uvf={'+y': (0, 0, zl / 6, 1), '-x': (0, 0, zl / 4, top / 2), '+x': (0, 0, 2, 2), '+z': (0, 0, 1, 1), '-z': (0, 0, 1, 1), '-y': (0, 0, 1, 1)})
    xe = x0 + rows * depth
    box(m, 'co', (xe + 0.1, (rows * rise + 1.6) / 2, zc), (0.2, rows * rise + 1.6, zl), uv=(0, 0, zl / 4, 3))
    m.write()


def m_umpire_chair():
    m = Mesh('SillaJuez')
    m.material('w', 'madera.png')
    m.material('m', 'metal_oscuro.png')
    m.material('b', 'tape_blanco.png')
    for sx in (-0.3, 0.3):
        for sz in (-0.3, 0.3):
            cyl(m, 'm', (sx, 0, sz), (sx * 0.8, 2.4, sz * 0.8), 0.04, segs=8)
    box(m, 'b', (0, 2.4, 0), (0.8, 0.08, 0.8))
    box(m, 'w', (0, 2.9, -0.36), (0.8, 0.9, 0.08))
    box(m, 'm', (0, 0.0, 0), (1.0, 0.05, 1.0))
    box(m, 'm', (0, 1.1, 0.0), (0.7, 0.04, 0.7))
    box(m, 'w', (0.0, 3.15, 0.55), (0.6, 0.05, 0.35))
    m.write()


def m_bench():
    m = Mesh('Banca')
    m.material('w', 'madera.png')
    m.material('m', 'metal_oscuro.png')
    box(m, 'w', (0, 0.45, 0), (2.4, 0.06, 0.45))
    box(m, 'w', (0, 0.85, -0.2), (2.4, 0.45, 0.05))
    for sx in (-1.1, 1.1):
        box(m, 'm', (sx, 0.22, 0), (0.06, 0.44, 0.4))
        box(m, 'm', (sx, 0.65, -0.2), (0.06, 0.8, 0.05))
    m.write()


def m_cooler():
    m = Mesh('Hielera')
    m.material('a', 'azul_hielera.png')
    m.material('b', 'tape_blanco.png')
    box(m, 'a', (0, 0.25, 0), (0.5, 0.5, 0.35))
    box(m, 'b', (0, 0.52, 0), (0.54, 0.06, 0.39))
    m.write()


def m_umbrella():
    m = Mesh('Sombrilla')
    m.material('r', 'sombrilla.png')
    m.material('m', 'metal_oscuro.png')
    cyl(m, 'm', (0, 0, 0), (0, 2.3, 0), 0.03, segs=8)
    cyl(m, 'r', (0, 2.0, 0), (0, 2.45, 0), 1.3, 0.05, segs=16, caps=False)
    m.write()


def m_light_tower():
    m = Mesh('TorreLuz')
    m.material('m', 'metal_oscuro.png')
    m.material('g', 'metal_claro.png')
    cyl(m, 'm', (0, 0, 0), (0, 19.0, 0), 0.35, 0.22, segs=10)
    box(m, 'g', (0, 19.4, 0), (4.2, 1.9, 0.35))
    box(m, 'm', (0, 19.4, -0.25), (4.4, 2.1, 0.15))
    m.write()


def m_scoreboard():
    m = Mesh('Marcador')
    m.material('b', 'marcador_base.png')
    m.material('m', 'metal_oscuro.png')
    W, H = 8.0, 3.0
    box(m, {'+z': 'b', '-z': 'm', '+x': 'm', '-x': 'm', '+y': 'm', '-y': 'm'}, (0, 4.6, 0), (W, H, 0.3))
    for sx in (-3.0, 3.0):
        box(m, 'm', (sx, 1.55, -0.05), (0.3, 3.1, 0.3))
    m.write()


def m_digit(name, tex):
    m = Mesh(name)
    m.material('d', tex)
    m.quad('d', (-0.5, -0.5, 0), (0.5, -0.5, 0), (0.5, 0.5, 0), (-0.5, 0.5, 0), n=(0, 0, 1))
    m.write()


def m_ball():
    m = Mesh('Pelota')
    m.material('p', 'pelota_tenis.png')
    sphere(m, 'p', (0, 0, 0), 0.1, lat=12, lon=20)
    m.write()


# ----- Jugador (partes con pivote en la articulacion) -----
def player_parts(prefix, atlas_tex):
    def mk(name):
        m = Mesh('%s_%s' % (prefix, name))
        m.material('a', atlas_tex)
        return m

    # Torso: pivote en la cintura (centro de la pelvis)
    m = mk('Torso')
    box(m, 'a', (0, 0.30, 0), (0.42, 0.56, 0.24), uv=SHIRT)
    box(m, 'a', (0, -0.08, 0), (0.40, 0.22, 0.25), uv=SHORTS)
    sphere(m, 'a', (-0.23, 0.52, 0), (0.07, 0.07, 0.07), uv=SHIRT)
    sphere(m, 'a', (0.23, 0.52, 0), (0.07, 0.07, 0.07), uv=SHIRT)
    m.write()
    # Cabeza: pivote en el cuello
    m = mk('Cabeza')
    cyl(m, 'a', (0, 0, 0), (0, 0.08, 0), 0.045, segs=10, uv=SKIN)
    sphere(m, 'a', (0, 0.18, 0), (0.095, 0.115, 0.105), uv=SKIN)
    sphere(m, 'a', (0, 0.205, -0.015), (0.10, 0.10, 0.105), uv=HAIR, full=False)
    m.write()
    # Brazo superior: pivote en el hombro, cuelga hacia -Y
    m = mk('Brazo')
    cyl(m, 'a', (0, 0, 0), (0, -0.30, 0), 0.05, 0.042, segs=10, uv=SHIRT)
    m.write()
    # Antebrazo: pivote en el codo
    m = mk('Antebrazo')
    cyl(m, 'a', (0, 0, 0), (0, -0.26, 0), 0.04, 0.032, segs=10, uv=SKIN)
    sphere(m, 'a', (0, -0.29, 0), 0.04, uv=SKIN)
    m.write()
    # Muslo: pivote en la cadera
    m = mk('Muslo')
    cyl(m, 'a', (0, 0, 0), (0, -0.16, 0), 0.085, 0.085, segs=10, uv=SHORTS, caps=False)
    cyl(m, 'a', (0, -0.16, 0), (0, -0.45, 0), 0.075, 0.06, segs=10, uv=SKIN)
    m.write()
    # Pierna: pivote en la rodilla
    m = mk('Pierna')
    cyl(m, 'a', (0, 0, 0), (0, -0.34, 0), 0.058, 0.045, segs=10, uv=SKIN)
    cyl(m, 'a', (0, -0.30, 0), (0, -0.40, 0), 0.052, 0.052, segs=10, uv=WHITE)
    box(m, 'a', (0, -0.44, 0.04), (0.10, 0.08, 0.26), uv=SHOES)
    m.write()
    # Raqueta: pivote en el agarre, el mango se extiende hacia -Y
    m = Mesh('%s_Raqueta' % prefix)
    m.material('f', atlas_tex)
    m.material('s', 'cuerdas_raqueta.png')
    cyl(m, 'f', (0, 0.05, 0), (0, -0.20, 0), 0.016, segs=8, uv=DARK)
    cyl(m, 'f', (0, -0.20, 0), (0, -0.30, 0), 0.010, 0.010, segs=8, uv=ACC)
    ring(m, 'f', (0, -0.46, 0), 0.115, 0.165, 0.011, uv=ACC)
    m.quad('s', (-0.115, -0.625, 0.0), (0.115, -0.625, 0.0), (0.115, -0.295, 0.0), (-0.115, -0.295, 0.0), n=(0, 0, 1))
    m.quad('s', (0.115, -0.625, 0.0), (-0.115, -0.625, 0.0), (-0.115, -0.295, 0.0), (0.115, -0.295, 0.0), n=(0, 0, -1))
    m.write()


def spectator(name, tex):
    m = Mesh(name)
    m.material('a', tex)
    # sentado, mirando hacia +z; origen en el asiento
    box(m, 'a', (0, 0.55, 0), (0.36, 0.5, 0.22), uv=SHIRT)
    box(m, 'a', (0, 0.12, 0.12), (0.34, 0.14, 0.4), uv=SHORTS)
    box(m, 'a', (-0.1, -0.15, 0.30), (0.12, 0.45, 0.12), uv=SHORTS)
    box(m, 'a', (0.1, -0.15, 0.30), (0.12, 0.45, 0.12), uv=SHORTS)
    box(m, 'a', (-0.2, 0.45, 0.06), (0.09, 0.09, 0.30), uv=SKIN)
    box(m, 'a', (0.2, 0.45, 0.06), (0.09, 0.09, 0.30), uv=SKIN)
    sphere(m, 'a', (0, 0.93, 0), (0.1, 0.11, 0.1), uv=SKIN)
    sphere(m, 'a', (0, 0.96, -0.01), (0.105, 0.10, 0.105), uv=HAIR, full=False)
    m.write()


def umpire_parts():
    atlas('atlas_juez.png', [(224, 180, 150), (245, 245, 245), (30, 30, 40), (30, 30, 30), (60, 40, 25), (255, 255, 255), (20, 20, 20), (200, 50, 50)])
    m = Mesh('Juez_Torso')
    m.material('a', 'atlas_juez.png')
    box(m, 'a', (0, 0.30, 0), (0.44, 0.58, 0.26), uv=SHIRT)
    box(m, 'a', (0, -0.05, 0.12), (0.40, 0.14, 0.45), uv=SHORTS)
    box(m, 'a', (-0.1, -0.2, 0.34), (0.14, 0.14, 0.40), uv=SHORTS)
    box(m, 'a', (0.1, -0.2, 0.34), (0.14, 0.14, 0.40), uv=SHORTS)
    box(m, 'a', (-0.10, -0.40, 0.52), (0.13, 0.55, 0.13), uv=SHORTS)
    box(m, 'a', (0.10, -0.40, 0.52), (0.13, 0.55, 0.13), uv=SHORTS)
    box(m, 'a', (-0.26, 0.38, 0.12), (0.09, 0.09, 0.40), uv=SHIRT)
    box(m, 'a', (0.26, 0.38, 0.12), (0.09, 0.09, 0.40), uv=SHIRT)
    m.write()
    m = Mesh('Juez_Cabeza')
    m.material('a', 'atlas_juez.png')
    cyl(m, 'a', (0, 0, 0), (0, 0.08, 0), 0.045, segs=10, uv=SKIN)
    sphere(m, 'a', (0, 0.18, 0), (0.095, 0.115, 0.105), uv=SKIN)
    sphere(m, 'a', (0, 0.205, -0.015), (0.10, 0.10, 0.105), uv=HAIR, full=False)
    m.write()


def main():
    tex_grass(); tex_court(); tex_concrete(); tex_seats(); tex_wood(); tex_net(); tex_strings(); tex_ball()
    tex_ad('ad_ace.png', 'ACE SPORT', (20, 60, 140), (255, 255, 255), 'tenis para todos')
    tex_ad('ad_fi.png', 'FI  UNAM', (200, 150, 20), (20, 20, 60), 'computacion grafica')
    tex_ad('ad_smash.png', 'SMASH', (190, 30, 40), (255, 255, 255), 'energy drink')
    tex_solid('tablero_oscuro.png', (25, 30, 45))
    tex_solid('metal_oscuro.png', (55, 58, 65))
    tex_solid('metal_claro.png', (150, 155, 165))
    tex_solid('tape_blanco.png', (240, 240, 240), 2)
    tex_solid('azul_hielera.png', (30, 90, 190))
    tex_solid('sombrilla.png', (210, 50, 50))
    tex_score_base()
    for t in ('0', '15', '30', '40', 'AD'):
        tex_digit('pt_%s.png' % t, t)
        m_digit('Pt_%s' % t, 'pt_%s.png' % t)
    for g in range(10):
        tex_digit('g_%d.png' % g, str(g))
        m_digit('G_%d' % g, 'g_%d.png' % g)
    m_court(); m_ground()
    m_adboard('AnuncioA', 'ad_ace.png'); m_adboard('AnuncioB', 'ad_fi.png'); m_adboard('AnuncioC', 'ad_smash.png')
    m_net(); m_post(); m_stand(); m_umpire_chair(); m_bench(); m_cooler(); m_umbrella(); m_light_tower(); m_scoreboard(); m_ball()
    # jugadores
    atlas('atlas_jugA.png', [(232, 190, 150), (40, 90, 200), (245, 245, 245), (240, 240, 240), (60, 40, 25), (255, 255, 255), (25, 25, 25), (240, 200, 40)])
    atlas('atlas_jugB.png', [(200, 150, 110), (210, 50, 50), (30, 30, 40), (240, 240, 240), (20, 20, 20), (255, 255, 255), (25, 25, 25), (240, 240, 240)])
    player_parts('JugA', 'atlas_jugA.png')
    player_parts('JugB', 'atlas_jugB.png')
    umpire_parts()
    colores = [(210, 60, 60), (60, 120, 210), (240, 200, 60), (80, 170, 90), (230, 230, 230), (150, 80, 180)]
    for i, c in enumerate(colores):
        atlas('atlas_espec%d.png' % i, [(225, 180, 150), c, (40, 50, 80), (30, 30, 30), (random.randint(20, 120),) * 3, (255, 255, 255), (20, 20, 20), c])
        spectator('Espectador%d' % i, 'atlas_espec%d.png' % i)
    print('Modelos generados en', os.path.abspath(OUT))


if __name__ == '__main__':
    main()
