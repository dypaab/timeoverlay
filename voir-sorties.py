#!/usr/bin/env python3
"""Affiche en direct, dans un terminal, les fichiers texte lus par OBS.

Sert a verifier d'un coup d'oeil ce que TimeOverlay ecrit reellement, sans
passer par OBS : quel fichier est vide, lequel change, et a quel moment le
compte a rebours passe le relais au depassement.

    python voir-sorties.py                 tous les fichiers
    python voir-sorties.py countdown       un seul, en gros
    python voir-sorties.py --dossier CHEMIN

Ctrl+C pour quitter.
"""

import os
import sys
import time

# L'ordre d'affichage suit le deroule d'un culte, pas l'ordre alphabetique :
# on lit d'abord ou on en est, ensuite les compteurs, enfin les textes.
FICHIERS = [
    "heure", "date", "avant_debut",
    "countdown", "depassement", "countup",
    "phase", "phase_suivante", "statut",
    "message", "annonce",
]

# Le compte a rebours et le depassement sont mis en evidence : ce sont les
# deux qu'on superpose dans OBS, et ils ne doivent jamais s'afficher ensemble.
RELAIS = ("countdown", "depassement")


def dossier_par_defaut():
    """Le dossier de sortie de TimeOverlay, selon le systeme."""
    if os.name == "nt":
        base = os.environ.get("APPDATA", "")
        return os.path.join(base, "TimeOverlay", "obs")
    return os.path.join(os.path.expanduser("~"), ".local", "share", "TimeOverlay", "obs")


def lire(chemin):
    """Contenu du fichier, ou None s'il est illisible.

    Les erreurs sont avalees a dessein : le fichier est reecrit en permanence
    par l'application, et tomber pile pendant un remplacement ne doit pas
    arreter l'affichage.
    """
    try:
        with open(chemin, "r", encoding="utf-8") as fichier:
            return fichier.read().strip()
    except OSError:
        return None


def efface_ecran():
    # On repositionne le curseur au lieu d'effacer vraiment : l'ecran ne
    # clignote pas, et le terminal reste utilisable pendant un direct.
    sys.stdout.write("\033[H\033[J")


def affiche_tout(dossier):
    lignes = []
    lignes.append("TimeOverlay — sorties OBS en direct")
    lignes.append(dossier)
    lignes.append("")

    # La colonne affiche "nom.txt" : la largeur doit tenir compte de
    # l'extension, sinon la ligne la plus longue deborde et casse l'alignement.
    largeur = max(len(nom) for nom in FICHIERS) + len(".txt") + 2

    for nom in FICHIERS:
        contenu = lire(os.path.join(dossier, nom + ".txt"))

        if contenu is None:
            valeur = "· fichier absent"
        elif contenu == "":
            valeur = "· vide"
        else:
            valeur = contenu.replace("\n", " ⏎ ")

        marque = "▸" if nom in RELAIS else " "
        lignes.append("{} {:<{}} {}".format(marque, nom + ".txt", largeur, valeur))

    lignes.append("")
    lignes.append("▸ les deux sources superposees dans OBS : une seule doit porter une valeur")
    lignes.append("Ctrl+C pour quitter")
    return lignes


def affiche_un(dossier, nom):
    contenu = lire(os.path.join(dossier, nom + ".txt"))
    if contenu is None:
        valeur = "fichier absent"
    elif contenu == "":
        valeur = "(vide)"
    else:
        valeur = contenu

    return ["", "  " + nom + ".txt", "", "  " + valeur, "", "  Ctrl+C pour quitter"]


def main():
    arguments = sys.argv[1:]
    dossier = dossier_par_defaut()
    cible = None

    if "--dossier" in arguments:
        index = arguments.index("--dossier")
        if index + 1 >= len(arguments):
            print("--dossier attend un chemin.")
            return 1
        dossier = arguments[index + 1]
        del arguments[index:index + 2]

    if arguments:
        cible = arguments[0].removesuffix(".txt")
        if cible not in FICHIERS:
            print("Fichier inconnu : " + cible)
            print("Choix possibles : " + ", ".join(FICHIERS))
            return 1

    if not os.path.isdir(dossier):
        print("Dossier introuvable : " + dossier)
        print("Lancez TimeOverlay au moins une fois, ou indiquez le chemin avec --dossier.")
        return 1

    # Sur une console Windows, la sortie n'est pas forcement en UTF-8 : sans
    # cela les accents et les symboles ressortent en charabia.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    try:
        while True:
            lignes = affiche_un(dossier, cible) if cible else affiche_tout(dossier)
            efface_ecran()
            print("\n".join(lignes), flush=True)
            # Quatre rafraichissements par seconde : l'oeil suit le relais
            # entre countdown et depassement sans que le terminal chauffe.
            time.sleep(0.25)
    except KeyboardInterrupt:
        print()
        return 0


if __name__ == "__main__":
    sys.exit(main())
