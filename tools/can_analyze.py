#!/usr/bin/env python3
"""Analyse des releves CAN produits par le boitier en mode sniffer.

Format attendu (carte SD ou port serie) :  ms,id,ext,dlc,b0,...,b7
Les lignes commencant par '#' sont ignorees.

Exemples :
  # 1) Vue d'ensemble : quels identifiants, a quel rythme, quels octets bougent
  python3 can_analyze.py resume can_000.csv

  # 2) Comparer deux releves (ex. moteur au ralenti / moteur a 4000 tr/min)
  #    pour voir quels octets ont change de plage
  python3 can_analyze.py compare ralenti.csv 4000rpm.csv

  # 3) Suivre un identifiant dans le temps (candidats 8 et 16 bits)
  python3 can_analyze.py suivre can_000.csv 0x102 --pas 0.5
"""
import argparse
import csv
import sys
from collections import defaultdict


def lire(path):
    """Renvoie {id: [(t_secondes, bytes), ...]}."""
    frames = defaultdict(list)
    with open(path, newline="", errors="replace") as f:
        for row in csv.reader(f):
            if len(row) < 4 or row[0].startswith("#") or not row[0].strip().isdigit():
                continue
            try:
                t = int(row[0]) / 1000.0
                can_id = int(row[1], 16)
                dlc = int(row[3])
                data = bytes(int(x, 16) for x in row[4:4 + dlc])
            except ValueError:
                continue
            if len(data) == dlc:
                frames[can_id].append((t, data))
    return frames


def plages(msgs):
    """Par position d'octet : (min, max, nombre de valeurs distinctes)."""
    n = max(len(d) for _, d in msgs)
    out = []
    for i in range(n):
        vals = [d[i] for _, d in msgs if len(d) > i]
        out.append((min(vals), max(vals), len(set(vals))))
    return out


def cmd_resume(args):
    frames = lire(args.fichier)
    if not frames:
        sys.exit("Aucune trame lisible dans ce fichier.")
    print(f"{len(frames)} identifiants, {sum(len(m) for m in frames.values())} trames\n")
    print(f"{'ID':>6} {'trames':>7} {'periode':>9}  octets (min-max, '.' = constant)")
    for can_id in sorted(frames):
        msgs = frames[can_id]
        duree = msgs[-1][0] - msgs[0][0]
        periode = f"{1000 * duree / (len(msgs) - 1):.0f} ms" if len(msgs) > 1 and duree > 0 else "-"
        cols = []
        for lo, hi, distinct in plages(msgs):
            cols.append(f"   {lo:02X}." if distinct == 1 else f"{lo:02X}-{hi:02X}")
        print(f"{can_id:>6X} {len(msgs):>7} {periode:>9}  {' '.join(cols)}")


def cmd_compare(args):
    a, b = lire(args.a), lire(args.b)
    print(f"Octets dont la plage differe entre {args.a} et {args.b} :\n")
    trouve = False
    for can_id in sorted(set(a) & set(b)):
        pa, pb = plages(a[can_id]), plages(b[can_id])
        for i in range(min(len(pa), len(pb))):
            (alo, ahi, _), (blo, bhi, _) = pa[i], pb[i]
            # plages disjointes = l'octet suit vraiment la grandeur qu'on a fait varier
            if ahi < blo or bhi < alo:
                trouve = True
                print(f"  ID {can_id:X} octet {i} : {alo:02X}-{ahi:02X}  ->  {blo:02X}-{bhi:02X}")
    if not trouve:
        print("  (aucun octet avec des plages disjointes)")
    seuls = sorted(set(a) ^ set(b))
    if seuls:
        print("\nIdentifiants presents dans un seul releve :", " ".join(f"{i:X}" for i in seuls))


def cmd_suivre(args):
    frames = lire(args.fichier)
    can_id = int(args.id, 16)
    if can_id not in frames:
        sys.exit(f"ID {can_id:X} absent. Identifiants presents : " + " ".join(f"{i:X}" for i in sorted(frames)))
    msgs = frames[can_id]
    n = max(len(d) for _, d in msgs)
    print("t(s)     octets" + " " * (3 * n - 5) + "| 16 bits gros-boutiste (octets 0-1, 1-2, ...)")
    prochain = msgs[0][0]
    for t, d in msgs:
        if t < prochain:
            continue
        prochain = t + args.pas
        mots = " ".join(f"{(d[i] << 8) | d[i + 1]:5d}" for i in range(len(d) - 1))
        print(f"{t:8.2f} {d.hex(' ').upper():<{3 * n}}| {mots}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("resume", help="vue d'ensemble d'un releve")
    s.add_argument("fichier")
    s.set_defaults(func=cmd_resume)
    s = sub.add_parser("compare", help="octets qui changent de plage entre deux releves")
    s.add_argument("a")
    s.add_argument("b")
    s.set_defaults(func=cmd_compare)
    s = sub.add_parser("suivre", help="evolution d'un identifiant dans le temps")
    s.add_argument("fichier")
    s.add_argument("id", help="identifiant en hexadecimal, ex. 0x102")
    s.add_argument("--pas", type=float, default=0.5, help="intervalle d'affichage en secondes")
    s.set_defaults(func=cmd_suivre)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
