#!/usr/bin/env python3
"""Analyse du journal anti-collision (/collision_log.csv de la carte SD).

Le mode journal n'agit JAMAIS sur le jet-ski : il note seulement ce qu'un
systeme actif aurait fait. Cet outil regroupe ces notes en "episodes" (menaces
continues) pour que tu juges la fiabilite : combien de fois le systeme aurait
agi, et si c'etait justifie.

Colonnes du journal : ms,peer,dist_m,range_rate_mps,tcpa_s,dcpa_m,level,would_act

Usage :
  python3 collision_log_analyze.py collision_log.csv
  python3 collision_log_analyze.py collision_log.csv --ecart 3   # separe les episodes a 3 s de silence
"""
import argparse
import csv
import sys


def lire(path):
    rows = []
    with open(path, newline="", errors="replace") as f:
        for r in csv.DictReader(f):
            try:
                rows.append({
                    "t": int(r["ms"]) / 1000.0,
                    "peer": r["peer"],
                    "dist": float(r["dist_m"]),
                    "rr": float(r["range_rate_mps"]),
                    "tcpa": float(r["tcpa_s"]),
                    "dcpa": float(r["dcpa_m"]),
                    "level": int(r["level"]),
                    "act": int(r["would_act"]),
                })
            except (KeyError, ValueError):
                continue
    return rows


def episodes(rows, gap):
    """Regroupe par module, en coupant des qu'il y a plus de `gap` secondes de silence."""
    by_peer = {}
    for r in rows:
        by_peer.setdefault(r["peer"], []).append(r)
    eps = []
    for peer, rs in by_peer.items():
        rs.sort(key=lambda x: x["t"])
        cur = [rs[0]]
        for a, b in zip(rs, rs[1:]):
            if b["t"] - a["t"] > gap:
                eps.append((peer, cur))
                cur = []
            cur.append(b)
        eps.append((peer, cur))
    eps.sort(key=lambda e: e[1][0]["t"])
    return eps


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("fichier")
    p.add_argument("--ecart", type=float, default=5.0, help="silence (s) qui separe deux episodes")
    args = p.parse_args()

    rows = lire(args.fichier)
    if not rows:
        sys.exit("Journal vide ou illisible.")
    eps = episodes(rows, args.ecart)
    n_act = sum(1 for _, rs in eps if any(r["act"] for r in rs))

    print(f"{len(rows)} notes, {len(eps)} episode(s), dont {n_act} ou le systeme AURAIT AGI.\n")
    print(f"{'#':>2} {'module':<10} {'debut(s)':>9} {'duree':>6} {'dist.min':>9} {'rappr.max':>10} {'niv.max':>7}  aurait agi")
    for i, (peer, rs) in enumerate(eps, 1):
        dmin = min(r["dist"] for r in rs)
        rrmax = min(r["rr"] for r in rs)          # plus negatif = rapprochement le plus fort
        lvmax = max(r["level"] for r in rs)
        duree = rs[-1]["t"] - rs[0]["t"]
        act = "OUI" if any(r["act"] for r in rs) else "-"
        print(f"{i:>2} {peer:<10} {rs[0]['t']:>9.1f} {duree:>5.1f}s {dmin:>8.0f}m {rrmax:>9.1f}m/s {lvmax:>7} {act:>11}")

    print("\nComment juger un faux positif :")
    print("  - 'aurait agi' = OUI mais la distance.min est restee grande / tu te souviens")
    print("    qu'il n'y avait pas de danger reel -> c'est un faux positif a corriger.")
    print("  - rapprochement proche de 0 = vous ne vous rapprochiez pas vraiment.")
    print("  Tant qu'il reste des faux positifs, le systeme n'a rien a faire sur les gaz.")


if __name__ == "__main__":
    main()
