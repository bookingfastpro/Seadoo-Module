#!/usr/bin/env python3
"""
Boîtier étanche imprimable 3D pour le module Sea-Doo — version compacte.

Génère 2 STL (fond + couvercle) :  python3 boitier.py
Dépendances :                      pip install manifold3d trimesh numpy

Toutes les cotes sont en mm et réglables ci-dessous (section PARAMÈTRES).
Repère : X = longueur intérieure (0 = paroi du buzzer), embase Deutsch sur la grande paroi Y=W,
         Y = largeur intérieure, Z = hauteur (0 = dessous du fond).

Implantation intérieure (vue de dessus) :
  X 0..20     zone B : buck Pololu D24V22F5 (17,8 x 17,8) + module CAN, à plat,
              buzzer Ø12 au-dessus, contre la paroi X=0
  X 20..48    zone A : arrière de l'embase Deutsch DT04-6P-L012 (montée sur la grande paroi Y=W)
  X 48..160   carte LilyGO T-SIM7600G-H (111 x 33 x 20 mm, accu 18650 dessous)
"""
import numpy as np
import trimesh
from manifold3d import Manifold, CrossSection, JoinType

# ---------------------------------------------------------------- PARAMÈTRES
ZONE_B, ZONE_A = 20.0, 28.0
BOARD_L, BOARD_W, BOARD_H = 111.0, 33.0, 20.0
BOARD_X0 = ZONE_B + ZONE_A               # 48
L = BOARD_X0 + BOARD_L + 1.0             # 160 : longueur intérieure
W = BOARD_W + 3.0                         # 36  : largeur intérieure (1,5 mm de jeu de chaque côté)
H = 35.5                                  # hauteur intérieure : imposée par la collerette Deutsch (35,5 mm)
WALL = 3.6
FLOOR = 2.4
R_IN = 4.0

# Joint : cordon silicone Ø1,5 mm dans une gorge au milieu de la paroi
GROOVE_W, GROOVE_D = 2.0, 1.1

# Colonnes de vis aux 4 angles, à l'extérieur du joint : inserts M3 (Ø4,0) + vis M3
COL_D = 9.0
INSERT_D = 4.0
SCREW_D = 3.4
COL_R = R_IN + WALL / 2 + GROOVE_W / 2 + 0.8 + COL_D / 2   # distance au centre de l'arrondi d'angle

LID_T = 2.8
LIP_T, LIP_H, LIP_GAP = 1.4, 3.0, 0.35

# Embase Deutsch DT04-6P-L012 (plan TE 0425-017-0000) sur la grande paroi Y=W, collerette à plat
#   découpe 25,40 x 25,40 R6,35 ; 4 trous à 45,01 x 24,00 ; collerette 54,99 x 35,51 x 3,71
#   corps derrière la collerette 20,65 mm ; se branche sur la fiche DT06-6S
#   Fixation : vis M4 dans des inserts M4 noyés dans un plot en relief (rien ne dépasse à l'intérieur)
DT_CUT, DT_CUT_R = 25.4, 6.35
DT_HOLES_DX, DT_HOLES_DZ = 45.01, 24.00
DT_FLANGE_W, DT_FLANGE_H = 54.99, 35.51
DT_X = ZONE_B + ZONE_A / 2               # 34 : centre de l'embase le long de la paroi
DT_Z = 19.8
PAD_T = 4.5                              # épaisseur du plot sous la collerette
M4_INSERT_D, M4_INSERT_L = 5.6, 7.5

# Buzzer actif Ø12 sur la paroi X=0, membrane amincie devant, au-dessus du buck
BUZ_D, BUZ_Y, BUZ_Z = 12.0, W / 2, FLOOR + 17.0
BUZ_MEMBRANE = 1.0

# Design
BOTTOM_CHAMFER = 1.6
LID_EDGE_CHAMFER = 1.2
PANEL_INSET, PANEL_H = 5.0, 1.0
DECO_GROOVES_Z = [12.0, 17.0]
DECO_W, DECO_D = 1.2, 0.6

# Marquage gravé sur le couvercle (police Poppins si installée, sinon DejaVu Sans)
BRAND, BRAND_H = "NAUTIX", 12.0         # hauteur des capitales
SIGN, SIGN_H = "by WinO", 4.8
BRAND_X = -2.0                          # centre du bloc texte le long du couvercle
ENGRAVE_D = 0.6

SEG = 64
# ---------------------------------------------------------------------------


def text_section(txt, cap_h, font="Poppins-Bold.ttf", fallback_weight="bold", style="normal"):
    """Contour 2D d'un texte, centré en (0, 0), hauteur des capitales = cap_h."""
    import os
    from matplotlib.textpath import TextPath
    from matplotlib.font_manager import FontProperties
    path = os.path.join("/usr/share/fonts/truetype/google-fonts", font)
    fp = FontProperties(fname=path) if os.path.exists(path) else \
        FontProperties(family="DejaVu Sans", weight=fallback_weight, style=style)
    cap = TextPath((0, 0), "H", size=100, prop=fp).get_extents().height
    tp = TextPath((0, 0), txt, size=100 * cap_h / cap, prop=fp)
    polys = [p[:-1] for p in tp.to_polygons(closed_only=True) if len(p) > 3]
    cs = CrossSection(polys, fillrule=__import__("manifold3d").FillRule.EvenOdd)
    b = cs.bounds()
    return cs.translate((-(b[0] + b[2]) / 2, -(b[1] + b[3]) / 2))


def rrect(lx, ly, r):
    r = max(r, 0.01)
    return CrossSection.square((lx - 2 * r, ly - 2 * r), center=True).offset(
        r, JoinType.Round, circular_segments=SEG)


def ring(off, z0, h):
    """Contour arrondi décalé de `off` par rapport au bord intérieur, extrudé."""
    return rrect(L + 2 * off, W + 2 * off, R_IN + off).extrude(h).translate((0, 0, z0))


def at(x, y, z=0.0):
    return (x - L / 2, y - W / 2, z)


def cyl(h, d, center=False):
    return Manifold.cylinder(h, d / 2, d / 2, SEG, center)


def box(sx, sy, sz):
    return Manifold.cube((sx, sy, sz))


def column_points():
    k = COL_R / np.sqrt(2)
    cx, cy = L / 2 - R_IN, W / 2 - R_IN
    return [(sx * (cx + k), sy * (cy + k)) for sx in (-1, 1) for sy in (-1, 1)]


def outline(z0, h, grow=0.0):
    """Silhouette extérieure : paroi + colonnes d'angle fondues dedans."""
    shell = ring(WALL + grow, z0, h)
    for (x, y) in column_points():
        k = (R_IN + WALL / 2) / np.sqrt(2)
        sx, sy = np.sign(x), np.sign(y)
        lobe = Manifold.batch_hull([
            cyl(h, COL_D + 2 * grow).translate((x, y, z0)),
            cyl(h, COL_D + 2 * grow).translate((sx * (L / 2 - R_IN + k), sy * (W / 2 - R_IN + k), z0)),
        ])
        shell += lobe
    return shell


def make_base():
    zt = FLOOR + H
    c = BOTTOM_CHAMFER
    body = Manifold.batch_hull([outline(0, 0.01, -c), outline(c, 0.01)]) + outline(c, zt - c)
    body -= ring(0, FLOOR, zt)                                   # creux intérieur

    # plot en relief sous la collerette Deutsch (grande paroi Y=W)
    pad_h = DT_Z + DT_FLANGE_H / 2 + 0.3
    pad = rrect(DT_FLANGE_W + 1.0, pad_h * 2, 2.0).extrude(WALL / 2 + PAD_T) \
        .rotate((-90, 0, 0)).translate((DT_X - L / 2, W / 2 + WALL / 2, 0))
    body += pad ^ box(L, 60, pad_h).translate((-L / 2, 0, 0))

    # lignes décoratives : grande face Y=0 et petite face X=L, loin de l'embase
    keep = box(L + 40, W + 40, 60).translate((-L / 2 + BOARD_X0 + 16, -(W + 40) / 2, 0))
    for zg in DECO_GROOVES_Z:
        band = (ring(WALL + 5, 0, DECO_W) - ring(WALL - DECO_D, 0, DECO_W)).translate((0, 0, zg - DECO_W / 2))
        body -= band ^ keep

    # gorge de joint au milieu de la paroi
    groove = (ring(WALL / 2 + GROOVE_W / 2, 0, GROOVE_D + 0.01) - ring(WALL / 2 - GROOVE_W / 2, 0, GROOVE_D + 0.01))
    body -= groove.translate((0, 0, zt - GROOVE_D))

    # inserts M3 dans les colonnes
    for (x, y) in column_points():
        body -= cyl(10, INSERT_D).translate((x, y, zt - 9.5))

    # embase Deutsch : découpe + inserts M4 (borgnes, depuis l'extérieur)
    x, y, z = at(DT_X, W, DT_Z)
    yo = W / 2 + WALL + PAD_T                             # face extérieure du plot
    cut = rrect(DT_CUT, DT_CUT, DT_CUT_R).extrude(30).rotate((-90, 0, 0)).translate((x, W / 2 - 5, z))
    body -= cut
    for dx in (-DT_HOLES_DX / 2, DT_HOLES_DX / 2):
        for dz in (-DT_HOLES_DZ / 2, DT_HOLES_DZ / 2):
            body -= cyl(M4_INSERT_L + 1, M4_INSERT_D).rotate((90, 0, 0)).translate((x + dx, yo + 1, z + dz))

    # buzzer : logement en porte-à-faux sur la paroi X=0 + membrane amincie
    x, y, z = at(0, BUZ_Y, BUZ_Z)
    holder = (cyl(9, BUZ_D + 3.2) - cyl(10, BUZ_D + 0.4).translate((0, 0, -0.5))).rotate((0, 90, 0))
    body += holder.translate((x, y, z))
    body -= cyl(WALL - BUZ_MEMBRANE + 0.01, BUZ_D - 2).rotate((0, 90, 0)).translate((x - WALL - 0.01, y, z))

    # butées de la carte (bouts) + nervures latérales basses
    ZONE_A_ = BOARD_X0
    for rx in (ZONE_A_ - 2.0, ZONE_A_ + BOARD_L + 0.4):
        body += box(2.0, 8.0, 5.0).translate(at(rx, W / 2 - 4, FLOOR))
    for ry in (0.0, W - 1.2):
        for x0 in (BOARD_X0 + 15, BOARD_X0 + BOARD_L - 25):
            body += box(10.0, 1.2, 8.0).translate(at(x0, ry, FLOOR))
    return body


def make_lid():
    e = LID_EDGE_CHAMFER
    plate = Manifold.batch_hull([outline(0, LID_T - e), outline(LID_T - 0.01, 0.01, -e)])
    for (x, y) in column_points():
        plate -= cyl(LID_T + 2, SCREW_D).translate((x, y, -1))
        plate -= cyl(2, 6.4).translate((x, y, LID_T - 1.2))     # lamage tête M3
    # plateau en relief, flancs à 45°
    px, py = L - 2 * PANEL_INSET, W - 2 * PANEL_INSET
    panel = Manifold.batch_hull([
        rrect(px, py, 3).extrude(0.01).translate((0, 0, LID_T - 0.01)),
        rrect(px - 2 * PANEL_H, py - 2 * PANEL_H, 2).extrude(0.01).translate((0, 0, LID_T + PANEL_H - 0.01)),
    ])
    lid = plate + panel
    # lèvre de centrage
    lip = (ring(-LIP_GAP, -LIP_H, LIP_H) - ring(-LIP_GAP - LIP_T, -LIP_H - 0.1, LIP_H + 0.2))
    lid += lip
    top = LID_T + PANEL_H
    # flèche GPS (vers le ciel) côté connecteur + 3 bandes obliques côté buzzer
    arrow = CrossSection.hull_points([(0, 7), (-5, 0), (5, 0)]) + CrossSection.square((3.4, 7)).translate((-1.7, -7))
    lid -= arrow.extrude(1.0).translate((-L / 2 + 22, 0, top - 0.6))
    for i in range(3):
        x0 = L / 2 - 30 + i * 6
        s = CrossSection.hull_points([(x0, -py / 2 + 4), (x0 + 3, -py / 2 + 4),
                                      (x0 + 3 + 9, py / 2 - 4), (x0 + 9, py / 2 - 4)])
        lid -= s.extrude(1.0).translate((0, 0, top - 0.6))
    # marquage « NAUTIX » + signature « by WinO » dessous, gravés au centre du plateau
    gap = 2.2
    brand = text_section(BRAND, BRAND_H)
    sign = text_section(SIGN, SIGN_H, font="Poppins-BoldItalic.ttf", style="italic")
    bh = brand.bounds()[3] - brand.bounds()[1]
    sh = sign.bounds()[3] - sign.bounds()[1]
    y0 = (bh + gap + sh) / 2
    bw = brand.bounds()[2] - brand.bounds()[0]
    sw = sign.bounds()[2] - sign.bounds()[0]
    marks = brand.translate((BRAND_X, y0 - bh / 2)) + \
        sign.translate((BRAND_X + bw / 2 - sw / 2, y0 - bh - gap - sh / 2))   # calée à droite sous NAUTIX
    b = marks.bounds()
    assert b[0] > -L / 2 + 30 and b[2] < L / 2 - 32 and b[3] - b[1] < py - 2 * PANEL_H - 2, "texte trop grand"
    lid -= marks.extrude(ENGRAVE_D + 0.4).translate((0, 0, top - ENGRAVE_D))
    return lid.rotate((180, 0, 0)).translate((0, 0, top))


def check():
    zt = FLOOR + H
    assert DT_Z - DT_FLANGE_H / 2 > BOTTOM_CHAMFER and DT_Z + DT_FLANGE_H / 2 < zt, "collerette DT hors de la hauteur"
    assert DT_X - DT_FLANGE_W / 2 > R_IN, "collerette DT trop près de l'angle"
    assert DT_X + DT_CUT / 2 + 1 < BOARD_X0, "le corps de l'embase touche la carte"
    assert WALL + PAD_T <= 16.5, "paroi trop épaisse pour la DT (16,5 mm max)"
    assert M4_INSERT_L < WALL + PAD_T - 0.4, "insert M4 débouchant"
    assert BUZ_Z - (BUZ_D + 3.2) / 2 > FLOOR + 8.5, "buzzer trop bas : touche le buck"


def export(man, path):
    mesh = man.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3],
                         faces=np.asarray(mesh.tri_verts), process=False)
    tm.export(path)
    ext = tm.bounds[1] - tm.bounds[0]
    print(f"{path}: {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm, "
          f"étanche={tm.is_watertight}, volume={tm.volume / 1000:.1f} cm3")
    return tm


if __name__ == "__main__":
    check()
    export(make_base(), "boitier_fond.stl")
    export(make_lid(), "boitier_couvercle.stl")
