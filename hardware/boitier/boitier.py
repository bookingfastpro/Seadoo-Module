#!/usr/bin/env python3
"""
Boîtier étanche imprimable 3D pour le module Sea-Doo — version compacte.

Génère 2 STL (fond + couvercle) :  python3 boitier.py
Dépendances :                      pip install manifold3d trimesh numpy

Toutes les cotes sont en mm et réglables ci-dessous (section PARAMÈTRES).
Repère : X = longueur intérieure (0 = paroi du connecteur M12),
         Y = largeur intérieure, Z = hauteur (0 = dessous du fond).

Implantation intérieure (vue de dessus) :
  X 0..24     zone A : arrière du connecteur M12 + fils
  X 24..136   carte LilyGO T-SIM7600G-H (111 x 33 x 20 mm, accu 18650 dessous)
  X 136..156  zone B : buck Pololu D24V22F5 (17,8 x 17,8) + module CAN, à plat,
              buzzer Ø12 au-dessus, contre la paroi X=L
"""
import numpy as np
import trimesh
from manifold3d import Manifold, CrossSection, JoinType

# ---------------------------------------------------------------- PARAMÈTRES
ZONE_A, ZONE_B = 24.0, 20.0
BOARD_L, BOARD_W, BOARD_H = 111.0, 33.0, 20.0
L = ZONE_A + BOARD_L + 1.0 + ZONE_B      # 156 : longueur intérieure
W = BOARD_W + 3.0                         # 36  : largeur intérieure (1,5 mm de jeu de chaque côté)
H = 28.0                                  # hauteur intérieure (carte 20 + antenne GPS collée sous le couvercle)
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

# Connecteur M12 5 broches codage A, embase femelle à visser, filetage PG9 (Ø15,2)
#   perçage PG9 : Ø15,3 +0,1 ; épaisseur de paroi max 4,0 mm (fiches fabricants)
M12_HOLE = 15.4
M12_FLAT = 22.0                # zone plane réservée autour (écrou SW17 + joint)
M12_Y, M12_Z = W / 2, 15.0

# Buzzer actif Ø12 sur la paroi X=L, membrane amincie devant, au-dessus du buck
BUZ_D, BUZ_Y, BUZ_Z = 12.0, W / 2, FLOOR + 17.0
BUZ_MEMBRANE = 1.0

# Design
BOTTOM_CHAMFER = 1.6
LID_EDGE_CHAMFER = 1.2
PANEL_INSET, PANEL_H = 5.0, 1.0
DECO_GROOVES_Z = [10.0, 14.5]
DECO_W, DECO_D = 1.2, 0.6

SEG = 64
# ---------------------------------------------------------------------------


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

    # lignes décoratives sur les grandes faces et la face X=L (pas sur la face du M12)
    keep = box(L + 40, W + 40, 60).translate((-L / 2 + R_IN + 2, -(W + 40) / 2, 0))
    for zg in DECO_GROOVES_Z:
        band = (ring(WALL + 5, 0, DECO_W) - ring(WALL - DECO_D, 0, DECO_W)).translate((0, 0, zg - DECO_W / 2))
        body -= band ^ keep

    # gorge de joint au milieu de la paroi
    groove = (ring(WALL / 2 + GROOVE_W / 2, 0, GROOVE_D + 0.01) - ring(WALL / 2 - GROOVE_W / 2, 0, GROOVE_D + 0.01))
    body -= groove.translate((0, 0, zt - GROOVE_D))

    # inserts M3 dans les colonnes
    for (x, y) in column_points():
        body -= cyl(10, INSERT_D).translate((x, y, zt - 9.5))

    # connecteur M12 (paroi X=0)
    x, y, z = at(0, M12_Y, M12_Z)
    body -= cyl(WALL * 4, M12_HOLE, center=True).rotate((0, 90, 0)).translate((x, y, z))

    # buzzer : logement en porte-à-faux sur la paroi X=L + membrane amincie
    x, y, z = at(L, BUZ_Y, BUZ_Z)
    holder = (cyl(9, BUZ_D + 3.2) - cyl(10, BUZ_D + 0.4).translate((0, 0, -0.5))).rotate((0, 90, 0))
    body += holder.translate((x - 9, y, z))
    body -= cyl(WALL - BUZ_MEMBRANE + 0.01, BUZ_D - 2).rotate((0, 90, 0)).translate((x + BUZ_MEMBRANE, y, z))

    # butées de la carte (bouts) + nervures latérales basses
    for rx in (ZONE_A - 2.0, ZONE_A + BOARD_L + 1.0):
        body += box(2.0, 8.0, 5.0).translate(at(rx, W / 2 - 4, FLOOR))
    for ry in (0.0, W - 1.2):
        for x0 in (ZONE_A + 15, ZONE_A + BOARD_L - 25):
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
    return lid.rotate((180, 0, 0)).translate((0, 0, top))


def check():
    zt = FLOOR + H
    assert M12_Y - M12_FLAT / 2 > R_IN and M12_Y + M12_FLAT / 2 < W - R_IN, "zone plane M12 hors du plat"
    assert M12_Z - M12_FLAT / 2 > BOTTOM_CHAMFER and M12_Z + M12_FLAT / 2 < zt, "zone plane M12 trop haute/basse"
    assert WALL <= 4.0, "paroi trop épaisse pour l'embase M12 (4 mm max)"
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
