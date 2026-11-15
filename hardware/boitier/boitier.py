#!/usr/bin/env python3
"""
Boîtier étanche imprimable 3D pour le module Sea-Doo.

Génère 2 STL (fond + couvercle) :  python3 boitier.py
Dépendances :                      pip install manifold3d trimesh numpy

Toutes les cotes sont en mm et réglables ci-dessous (section PARAMÈTRES).
Repère : X = longueur intérieure (0 = paroi des presse-étoupe),
         Y = largeur intérieure (0 = paroi côté carte), Z = hauteur (0 = dessous du fond).
"""
import numpy as np
import trimesh
from manifold3d import Manifold, CrossSection, JoinType

# ---------------------------------------------------------------- PARAMÈTRES
L, W, H = 150.0, 82.0, 53.0   # volume intérieur utile (long., larg., haut. sous couvercle)
WALL = 3.0                    # épaisseur parois
FLOOR = 3.0                   # épaisseur fond
R_IN = 8.0                    # rayon des angles intérieurs (angles bien arrondis)

FLANGE_OUT = 11.0             # débord de la collerette au-dessus de la paroi
FLANGE_T = 6.0                # épaisseur collerette (reçoit les inserts)

# Joint : cordon silicone Ø2 mm dans une gorge du fond
GROOVE_W, GROOVE_D = 2.4, 1.5
GROOVE_POS = 4.5              # axe de la gorge, depuis le bord intérieur

# Vis couvercle : inserts laiton M3 (trou Ø4.0) dans la collerette, vis M3 dans le couvercle
INSERT_D = 4.0
SCREW_D = 3.4
SCREW_POS = 10.0              # axe des vis, depuis le bord intérieur (hors joint)

LID_T = 3.0                   # épaisseur couvercle
LIP_T, LIP_H, LIP_GAP = 1.6, 4.0, 0.4   # lèvre de centrage sous le couvercle

# Embase Deutsch DT04-6P-L012 sur la paroi X=0 (plan TE 0425-017-0000, feuille 2)
#   découpe recommandée 25.40 x 25.40 mm, rayons R6.35 ; 4 trous Ø4.32 à 45.01 x 24.00 mm entraxe
#   collerette 54.99 x 35.51 x 3.71 mm ; corps derrière la collerette 20.65 mm ; se branche sur une fiche DT06-6S
DT_CUT, DT_CUT_R = 25.4, 6.35
DT_HOLE_D, DT_HOLES_DX, DT_HOLES_DZ = 4.32, 45.01, 24.00
DT_FLANGE_W, DT_FLANGE_H = 54.99, 35.51
DT_Y, DT_Z = 41.0, 20.0       # centre de l'embase (Y le long de la paroi, Z hauteur)

# Design
BOTTOM_CHAMFER = 2.0          # chanfrein sur l'arête du dessous
JOINT_CHAMFER = 1.0           # chanfreins de part et d'autre du plan de joint (ligne en V)
LID_EDGE_CHAMFER = 1.5        # chanfrein sur l'arête du dessus du couvercle
PANEL_INSET, PANEL_H = 6.0, 1.2   # plateau en relief sur le couvercle
DECO_GROOVES_Z = [12.0, 17.0]     # lignes décoratives gravées sur 3 faces (pas sur la face Deutsch)
DECO_W, DECO_D = 1.2, 0.6

# Carte LilyGO T-SIM7600G-H : 111 x 33 x 20 mm (avec support 18650)
BOARD_L, BOARD_W = 111.0, 33.0
BOARD_X0, BOARD_Y0 = 36.0, 9.0   # coin de la carte (zone libre X<36 = arrière de l'embase DT)

# Buzzer actif Ø12 mm sur la paroi X=L, membrane amincie devant
BUZ_D, BUZ_Y, BUZ_Z = 12.0, 68.0, FLOOR + 12.0
BUZ_MEMBRANE = 1.0

SEG = 64
# ---------------------------------------------------------------------------


def rrect(lx, ly, r):
    """Rectangle arrondi centré, en 2D."""
    r = max(r, 0.01)
    return CrossSection.square((lx - 2 * r, ly - 2 * r), center=True).offset(
        r, JoinType.Round, circular_segments=SEG)


def at(x, y, z=0.0):
    """Coordonnées intérieures (coin X=0,Y=0) -> repère centré."""
    return (x - L / 2, y - W / 2, z)


def cyl(h, d, center=False):
    return Manifold.cylinder(h, d / 2, d / 2, SEG, center)


def box(sx, sy, sz):
    return Manifold.cube((sx, sy, sz))


def ring_path(offset_from_inner):
    """Contour (rectangle arrondi) à une distance donnée du bord intérieur."""
    return L + 2 * offset_from_inner, W + 2 * offset_from_inner, R_IN + offset_from_inner


def screw_points():
    """6 vis : 4 dans les angles (sur la bissectrice) + milieux des grands côtés."""
    k = (R_IN + SCREW_POS) / np.sqrt(2)
    cx, cy = L / 2 - R_IN, W / 2 - R_IN
    pts = [(sx * (cx + k), sy * (cy + k)) for sx in (-1, 1) for sy in (-1, 1)]
    pts += [(0.0, W / 2 + SCREW_POS), (0.0, -(W / 2 + SCREW_POS))]
    return pts


def make_base():
    zt = FLOOR + H                                     # haut du fond (plan de joint)
    Lo, Wo, Ro = L + 2 * WALL, W + 2 * WALL, R_IN + WALL
    c = BOTTOM_CHAMFER
    outer = Manifold.batch_hull([
        rrect(Lo - 2 * c, Wo - 2 * c, Ro - c).extrude(0.01),
        rrect(Lo, Wo, Ro).extrude(0.01).translate((0, 0, c)),
    ]) + rrect(Lo, Wo, Ro).extrude(zt - c).translate((0, 0, c))
    inner = rrect(L, W, R_IN).extrude(zt + 1).translate((0, 0, FLOOR))
    fl = FLANGE_OUT + WALL
    j = JOINT_CHAMFER
    flange = Manifold.batch_hull([
        rrect(L + 2 * fl, W + 2 * fl, R_IN + fl).extrude(FLANGE_T - j).translate((0, 0, zt - FLANGE_T)),
        rrect(L + 2 * (fl - j), W + 2 * (fl - j), R_IN + fl - j).extrude(0.01).translate((0, 0, zt - 0.01)),
    ])
    # chanfrein à 45° sous la collerette (imprimable sans support)
    cham = Manifold.batch_hull([
        rrect(L + 2 * WALL, W + 2 * WALL, R_IN + WALL).extrude(0.01).translate((0, 0, zt - FLANGE_T - FLANGE_OUT)),
        rrect(L + 2 * fl, W + 2 * fl, R_IN + fl).extrude(0.01).translate((0, 0, zt - FLANGE_T)),
    ])
    body = outer + flange + cham - inner

    # lignes décoratives : 2 gorges horizontales, sauf sur la face de l'embase (qui doit rester plane)
    keep = box(L + 2 * WALL + 10, W + 2 * WALL + 10, 50).translate((-L / 2 + R_IN, -(W + 2 * WALL + 10) / 2, 0))
    for zg in DECO_GROOVES_Z:
        band = (rrect(Lo + 2, Wo + 2, Ro + 1) - rrect(Lo - 2 * DECO_D, Wo - 2 * DECO_D, Ro - DECO_D)) \
            .extrude(DECO_W).translate((0, 0, zg - DECO_W / 2))
        body -= band ^ keep

    # gorge de joint
    a = ring_path(GROOVE_POS + GROOVE_W / 2)
    b = ring_path(GROOVE_POS - GROOVE_W / 2)
    groove = (rrect(*a) - rrect(*b)).extrude(GROOVE_D + 0.01).translate((0, 0, zt - GROOVE_D))
    body -= groove

    # trous d'inserts M3 (débouchants dans la collerette)
    for (x, y) in screw_points():
        body -= cyl(FLANGE_T + FLANGE_OUT + 1, INSERT_D).translate((x, y, zt - FLANGE_T - FLANGE_OUT - 0.5))

    # embase Deutsch DT (paroi X=0) : découpe carrée arrondie + 4 trous de fixation
    x, y, z = at(0, DT_Y, DT_Z)
    cut = rrect(DT_CUT, DT_CUT, DT_CUT_R).extrude(WALL * 4).translate((0, 0, -WALL * 2))
    body -= cut.rotate((0, 90, 0)).translate((x, y, z))
    for dy in (-DT_HOLES_DX / 2, DT_HOLES_DX / 2):
        for dz in (-DT_HOLES_DZ / 2, DT_HOLES_DZ / 2):
            body -= cyl(WALL * 4, DT_HOLE_D, center=True).rotate((0, 90, 0)).translate((x, y + dy, z + dz))

    # buzzer : logement + membrane amincie (paroi X=L)
    x, y, z = at(L, BUZ_Y, BUZ_Z)
    holder = (cyl(8, BUZ_D + 4) - cyl(9, BUZ_D + 0.4).translate((0, 0, -0.5))).rotate((0, 90, 0))
    body += holder.translate((x - 8, y, z))
    body -= cyl(WALL - BUZ_MEMBRANE + 0.01, BUZ_D - 2).rotate((0, 90, 0)).translate((x + BUZ_MEMBRANE, y, z))
    # soutien du logement buzzer jusqu'au fond
    body += box(8, 4, BUZ_Z - FLOOR - (BUZ_D + 0.4) / 2).translate((x - 8, y - 2, FLOOR))

    # berceau carte : rails courts de chaque côté + butées en bout
    rail_h = 6.0
    for ry in (BOARD_Y0 - 2.0, BOARD_Y0 + BOARD_W + 0.6):
        for (a0, a1) in ((5, 15), (45, 65), (BOARD_L - 15, BOARD_L - 5)):
            body += box(a1 - a0, 2.0, rail_h).translate(at(BOARD_X0 + a0, ry, FLOOR))
    for rx in (BOARD_X0 - 2.6, BOARD_X0 + BOARD_L + 0.6):
        body += box(2.0, 10.0, rail_h).translate(at(rx, BOARD_Y0 + BOARD_W / 2 - 5, FLOOR))

    # passants de colliers (tunnel orienté Y : le collier passe en travers)
    def tie_anchor(xc, yc):
        blk = box(5, 8, 4) - box(3.2, 10, 2.2).translate((0.9, -1, -0.01))
        return blk.translate(at(xc - 2.5, yc - 4, FLOOR))

    for xc in (BOARD_X0 + 28, BOARD_X0 + BOARD_L - 28):        # carte
        body += tie_anchor(xc, BOARD_Y0 - 4.5)
        body += tie_anchor(xc, BOARD_Y0 + BOARD_W + 4.5)
    zone_y = BOARD_Y0 + BOARD_W + 9                               # bande convertisseur + CAN
    for xc in (70, 110):
        body += tie_anchor(xc, zone_y + 1)
        body += tie_anchor(xc, W - 5)

    return body


def make_lid():
    fl = FLANGE_OUT + WALL
    j, e = JOINT_CHAMFER, LID_EDGE_CHAMFER

    def ring(off, z0, h=0.01):
        return rrect(L + 2 * off, W + 2 * off, R_IN + off).extrude(h).translate((0, 0, z0))

    # plaque : chanfrein bas (ligne de joint en V) + chanfrein haut
    plate = Manifold.batch_hull([ring(fl - j, 0), ring(fl, j, LID_T - j - e), ring(fl - e, LID_T - 0.01)])
    # plateau central en relief, flancs à 45°
    px, py = L - 2 * PANEL_INSET, W - 2 * PANEL_INSET
    pr = max(R_IN - PANEL_INSET + 4, 2)
    panel = Manifold.batch_hull([
        rrect(px, py, pr).extrude(0.01).translate((0, 0, LID_T - 0.01)),
        rrect(px - 2 * PANEL_H, py - 2 * PANEL_H, pr - PANEL_H).extrude(0.01).translate((0, 0, LID_T + PANEL_H - 0.01)),
    ])
    plate += panel
    for (x, y) in screw_points():
        plate -= cyl(LID_T + 2, SCREW_D).translate((x, y, -1))
        # lamage pour tête de vis M3 (Ø6, prof. 1.5)
        plate -= cyl(2, 6.4).translate((x, y, LID_T - 1.5))
    # lèvre de centrage (rentre dans le fond, à l'intérieur du joint)
    lo = rrect(L - 2 * LIP_GAP, W - 2 * LIP_GAP, R_IN - LIP_GAP)
    li = rrect(L - 2 * (LIP_GAP + LIP_T), W - 2 * (LIP_GAP + LIP_T), max(R_IN - LIP_GAP - LIP_T, 0.5))
    lip = (lo - li).extrude(LIP_H).translate((0, 0, -LIP_H))
    lid = plate + lip
    # repère "GPS ↑" : simple flèche gravée côté extérieur
    arrow = CrossSection.hull_points([(0, 10), (-7, 0), (7, 0)]) + CrossSection.square((5, 10)).translate((-2.5, -10))
    lid -= arrow.extrude(1.0).translate((-L / 2 + 30, 0, LID_T + PANEL_H - 0.6))
    # 3 bandes obliques gravées sur le plateau (côté opposé à la flèche)
    for i in range(3):
        x0 = L / 2 - 34 + i * 8
        stripe = CrossSection.hull_points([(x0, -py / 2 + 6), (x0 + 4, -py / 2 + 6),
                                           (x0 + 4 + 14, py / 2 - 6), (x0 + 14, py / 2 - 6)])
        lid -= stripe.extrude(1.0).translate((0, 0, LID_T + PANEL_H - 0.6))
    # le couvercle s'imprime face extérieure en bas : on le retourne
    return lid.rotate((180, 0, 0)).translate((0, 0, LID_T + PANEL_H))


def export(man, path):
    mesh = man.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3],
                         faces=np.asarray(mesh.tri_verts), process=False)
    tm.export(path)
    ext = tm.bounds[1] - tm.bounds[0]
    print(f"{path}: {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm, "
          f"étanche={tm.is_watertight}, volume={tm.volume / 1000:.1f} cm3")
    return tm


def check():
    flat_top = FLOOR + H - FLANGE_T - FLANGE_OUT        # début du chanfrein sous la collerette
    assert DT_Z + DT_FLANGE_H / 2 < flat_top, "collerette DT trop haute"
    assert DT_Z - DT_FLANGE_H / 2 > 0, "collerette DT sous le fond"
    assert DT_Y - DT_FLANGE_W / 2 > R_IN and DT_Y + DT_FLANGE_W / 2 < W - R_IN, "collerette DT hors du plat"


if __name__ == "__main__":
    check()
    export(make_base(), "boitier_fond.stl")
    export(make_lid(), "boitier_couvercle.stl")
