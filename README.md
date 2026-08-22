# TimeOverlay

Chronométrage pour la diffusion en direct, sous **Linux et Windows**.

Écrit l'heure, un compte à rebours et le temps écoulé dans des fichiers texte
qu'OBS affiche en direct. Conçu pour remplacer Snaz, qui n'existe que sous
Windows, dans un usage de régie de culte : minuter chaque moment du programme
(louange, annonces, prédication) et signaler à l'intervenant qu'il doit
conclure.

## Le principe

Chaque valeur possède **son propre fichier**, écrit indépendamment des autres.
Vous pouvez donc afficher l'heure **et** un compte à rebours **et** le
dépassement en même temps, chacun dans sa propre source Texte d'OBS.

| Fichier | Contenu |
|---|---|
| `avant_debut.txt` | Décompte avant le début du culte — `00:12:34`, vide une fois démarré |
| `heure.txt` | Heure courante — `10:42:07` |
| `date.txt` | Date courante — `dimanche 17 août 2026` |
| `countdown.txt` | Temps restant sur la phase — `00:04:31`, **vide une fois zéro atteint** |
| `countup.txt` | Temps écoulé sur la phase, continue au-delà de la durée prévue |
| `depassement.txt` | `-00:01:22` uniquement en cas de dépassement, vide sinon |
| `phase.txt` | Nom de la phase en cours — `Prédication` |
| `phase_suivante.txt` | Nom de la phase suivante |
| `statut.txt` | `EN_COURS`, `PAUSE`, `DEPASSEMENT`, `TERMINE` |
| `message.txt` | Message libre tapé par l'opérateur pendant le culte |
| `annonce.txt` | Annonce en cours de rotation |

**Quand le compte à rebours atteint zéro, le relais est passé** : `countdown.txt`
**se vide**, `depassement.txt` affiche `-00:00:14` et `countup.txt` continue de
progresser.

Le compte à rebours se vide pour une raison précise : en régie, on superpose
souvent les deux sources au même endroit de l'écran. Tant que `countdown.txt`
gardait `00:00:00`, ce zéro figé restait affiché sous le temps de dépassement.
Maintenant l'un s'efface exactement quand l'autre apparaît, sans script ni
condition dans OBS.

> Une phase réglée pour **s'arrêter net** (dépassement décoché) garde son
> `00:00:00` : rien ne vient le remplacer, et un écran vide n'annoncerait pas
> la fin.
>
> Si l'une de vos scènes n'affiche **que** le compte à rebours, elle deviendra
> vide en cas de dépassement. Pointez-y plutôt `countup.txt`, qui ne se vide
> jamais.

### Où sont écrits ces fichiers

| Système | Chemin |
|---|---|
| Linux | `~/.local/share/TimeOverlay/obs/` |
| Windows | `%APPDATA%\TimeOverlay\obs\` |

Modifiable dans **Outils → Paramètres**. Le menu **Affichage → Ouvrir le
dossier de sortie** vous y emmène directement.

## Télécharger

Les binaires prêts à l'emploi sont sur la
**[page des publications](https://github.com/dypaab/timeoverlay/releases/latest)**.

| Votre système | Le fichier |
|---|---|
| Windows 10 / 11 | `TimeOverlay-Windows-x64.zip` |
| Ubuntu 22.04+, Debian 12+ | `timeoverlay_<version>_amd64.deb` |
| Autre distribution Linux | `TimeOverlay-x86_64.AppImage` |

La marche à suivre pas à pas, y compris pour quelqu'un qui n'ouvre jamais un
terminal, est dans **[INSTALLATION.txt](INSTALLATION.txt)**.

## Installation

### Ubuntu et Debian — le paquet `.deb` (recommandé)

```bash
sudo apt install ./timeoverlay_2.3.0_amd64.deb
```

Le `./` est indispensable : sans lui, `apt` cherche un paquet de ce nom sur
internet. Ne pas utiliser `dpkg -i`, qui n'installe pas les dépendances.

`apt` installe Qt tout seul. L'application apparaît ensuite dans le menu des
applications, avec son icône, et se lance aussi depuis un terminal :

```bash
TimeOverlay
```

Le programme d'exemple et le script OBS sont installés dans
`/usr/share/timeoverlay/`. Pour désinstaller :

```bash
sudo apt remove timeoverlay
```

### Autres distributions — l'AppImage

Un fichier unique, sans installation, qui embarque Qt.

```bash
chmod +x TimeOverlay-x86_64.AppImage
./TimeOverlay-x86_64.AppImage
```

Compilée sur Ubuntu 22.04, elle fonctionne sur les distributions dont la
bibliothèque C système est au moins aussi récente (glibc 2.35) : Ubuntu 22.04
et plus, Debian 12 et plus, Fedora récent. Sur une distribution plus ancienne,
elle refusera de démarrer — compilez alors depuis les sources.

Sous une session Wayland, l'application passe par XWayland. C'est transparent,
mais un avertissement `Could not find the Qt platform plugin "wayland"` peut
apparaître dans le terminal : il est sans conséquence.

### Windows — le paquet autonome

Décompressez `TimeOverlay-Windows-x64.zip` où vous voulez et lancez
`TimeOverlay.exe`. Aucune installation, aucun droit administrateur.

## Compilation

### Linux

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev
```

```bash
./build.sh
```

Le binaire est produit dans `build/src/TimeOverlay`. Le script lance ensuite
automatiquement le test de fumée et **refuse de valider la compilation si un
test échoue**.

### Vérifier que tout fonctionne

Un test sans interface graphique vérifie en quatre secondes les comportements
dont dépend la régie : le passage automatique en montée à zéro, l'indépendance
des fichiers de sortie, la validation des programmes et l'échappement CSV.

```bash
./build/tests/TimeOverlaySmokeTest
```

Il doit afficher `TOUS LES TESTS PASSENT`. Si ce n'est pas le cas, ne pas
utiliser le binaire en direct.

### Windows

Nécessite Qt 6.2 ou plus récent (MinGW), CMake 3.20+ et Ninja. Aucun droit
administrateur : Qt et MinGW s'installent en espace utilisateur avec
[aqtinstall](https://github.com/miurahr/aqtinstall), `cmake` et `ninja` sont
disponibles comme paquets pip.

Ajoutez `<Qt>/bin` et `<Qt>/Tools/mingw<version>/bin` au `PATH`, puis :

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt>/mingw_64"
```

```powershell
cmake --build build
```

```powershell
.\build\tests\TimeOverlaySmokeTest.exe
```

Pour distribuer l'application sur une machine sans Qt, déployez les
bibliothèques à côté de l'exécutable :

```powershell
windeployqt --release --no-compiler-runtime --translations fr --dir dist build\src\TimeOverlay.exe
```

Il faut y ajouter à la main `libgcc_s_seh-1.dll`, `libstdc++-6.dll` et
`libwinpthread-1.dll`, pris dans le dossier `bin` de MinGW : `windeployqt` ne
copie pas les bibliothèques du compilateur.

## Utilisation en régie

1. **Préparez le programme** — menu `Programme → Nouveau`. Chaque phase a un
   nom, une durée et la possibilité de s'arrêter net plutôt que de compter le
   dépassement. **Le programme est enregistré automatiquement** dans votre
   bibliothèque dès que vous validez : vous n'avez ni fichier à choisir ni
   dossier à retenir. Le fichier `example_culte.timerproject` fourni sert de
   point de départ.

   **Raccourci pratique** : l'application accepte un fichier en argument.

   ```bash
   TimeOverlay ~/programmes/culte-dimanche.timerproject
   ```

   Vous pouvez donc créer un raccourci sur le bureau qui ouvre directement le
   programme du dimanche, ou associer les fichiers `.timerproject` à
   l'application pour les ouvrir d'un double-clic.

2. **Pendant le culte** — `Démarrer` lance la phase, `Pause` la suspend,
   `Démarrer` **reprend là où on s'était arrêté** sans jamais repartir de zéro.
   `Suivant` passe à la phase d'après. `+ 1 min` et `− 1 min` ajustent la durée
   en direct sans casser le décompte.

3. **L'enchaînement automatique est désactivé par défaut.** À zéro, le
   dépassement s'affiche et le logiciel attend : c'est vous qui décidez quand
   on passe à la suite. Activable dans les paramètres si vous préférez.

4. **Après le culte** — `Programme → Exporter le compte rendu` produit un CSV
   comparant le prévu au réel, phase par phase. De quoi ajuster le programme
   de la semaine suivante.

## Si le logiciel se ferme en plein culte

Un clic de trop sur la croix, une coupure de courant, un plantage : à la
réouverture, TimeOverlay **propose de reprendre là où vous en étiez**.

> TimeOverlay s'est fermé il y a 19 secondes, pendant « Louange ».
> Le culte, lui, a continué : cette phase en est maintenant à 00:00:19 sur
> 00:25:00. **Reprendre là où vous en étiez ?**

Le point important est dans la deuxième phrase : **le temps continue de courir
pendant l'absence du logiciel**. Le prédicateur n'a pas fait de pause parce
qu'une fenêtre s'est fermée. C'est l'heure de début de phase qui est
enregistrée, pas un compteur de secondes — le temps écoulé se recalcule donc
sur l'horloge, et reste juste quelle que soit la durée de la coupure.

Une phase qui était **en pause** garde son temps figé : là, le culte était
réellement suspendu.

Sont restitués : le programme, la phase en cours, le temps écoulé, et la durée
ajustée si vous aviez utilisé `+ 1 min`. Rien ne redémarre tout seul — la
question vous est posée, et « Repartir de zéro » reste à un clic.

Au-delà de **deux heures**, la proposition n'apparaît plus : il ne s'agit plus
d'un incident mais d'un autre jour. Le minuteur libre, lui, n'est pas restitué :
c'est un compte à rebours ponctuel, qu'un clic suffit à relancer.

## Vos programmes et leur historique

`Programme → Mes programmes` (ou le bouton **Mes programmes**) ouvre la liste
de tous les programmes enregistrés, avec pour chacun le nombre de phases, la
durée prévue et la date du dernier culte où il a servi.

Ils sont rangés dans :

| Système | Dossier |
|---|---|
| Linux | `~/.local/share/TimeOverlay/programmes/` |
| Windows | `%APPDATA%\TimeOverlay\programmes\` |

Un programme y entre **tout seul** quand vous le créez. Il est repéré par son
nom : deux programmes de même nom sont le même programme, et l'application
prévient avant de remplacer.

### L'historique

Sous la liste, le tableau du bas montre les cultes réellement déroulés avec le
programme sélectionné : date, nombre de phases parcourues, temps prévu, temps
réel et écart. C'est la réponse à « combien de temps ça prend vraiment » — la
donnée qui permet de corriger un programme trop optimiste.

Une séance est archivée quand vous fermez l'application ou quand vous chargez
un autre programme. Une remise à zéro en plein culte n'archive rien : c'est une
correction, pas une fin de séance. Les cent dernières séances sont conservées.

### Modifier un programme

`Modifier les phases` ouvre l'éditeur : changer une durée, renommer une phase,
en ajouter ou en retirer, changer l'ordre. Le titre de la fenêtre porte alors
une **astérisque** — le programme est modifié mais pas encore enregistré.

`Ctrl+S` enregistre tout de suite. Sinon, l'application **vous demandera** en
quittant si vous voulez conserver les changements, comme n'importe quel
logiciel de bureau. `Enregistrer une copie` sert à sortir un programme de la
bibliothèque, pour l'emporter sur une clé ou le donner à quelqu'un.

Si vous **changez le nom** du programme, l'application demande s'il faut le
renommer — son historique suit alors le nouveau nom — ou garder les deux, pour
décliner un programme existant en un second.

## Lancement au démarrage de l'ordinateur

L'application affiche l'heure et le décompte avant le culte : elle ne sert à
rien si personne ne pense à l'ouvrir. Elle s'inscrit donc au démarrage de la
session **dès son premier lancement**.

| Système | Mécanisme |
|---|---|
| Linux | `~/.config/autostart/TimeOverlay.desktop` (standard XDG) |
| Windows | clé de registre `Run` de l'utilisateur courant |

Aucun droit administrateur n'est demandé et aucune autre session n'est touchée.
La case se décoche dans `Outils → Paramètres` ; une fois décochée, elle n'est
jamais recochée toute seule.

## Aide intégrée

`Aide → Mode d'emploi` (ou `F1`) ouvre un mode d'emploi dont **les endroits
cités sont cliquables** : ouvrir la bibliothèque, créer un programme, afficher
les chemins OBS, ouvrir le dossier de sortie, les paramètres, l'overlay. La
fenêtre n'est pas modale — gardez-la ouverte à côté pendant que vous suivez les
étapes.

## Messages et annonces

Le panneau **Messages et annonces**, ancré à droite de la fenêtre, gère deux
canaux indépendants, chacun avec son propre fichier.

### Message libre → `message.txt`

Une zone de saisie où l'opérateur tape un message pendant le culte : « un
enfant vous attend à l'accueil », « merci de conclure ». **Afficher** l'envoie,
**Effacer l'écran** le retire. `Ctrl+Entrée` envoie sans lâcher le clavier.

Les messages fréquents se gardent comme **modèles** : un double-clic les
recharge. Ils sont conservés d'un dimanche à l'autre.

### Annonces qui défilent → `annonce.txt`

Une liste d'annonces qui tournent automatiquement à l'intervalle de votre choix.
Contrairement au Textline Changer de Snaz, le nombre de lignes n'est pas limité
à trois. La diffusion doit être activée explicitement — au démarrage elle est
toujours à l'arrêt, pour qu'aucune annonce de la semaine passée ne parte à
l'antenne toute seule.

### Adaptation à l'affichage

Chaque canal a ses propres réglages de mise en forme, parce qu'un bandeau d'une
ligne en bas de l'image et un panneau sur l'écran de retour de l'orateur n'ont
ni la même largeur ni la même hauteur :

- **Largeur** — le texte revient à la ligne tout seul, sans couper les mots.
- **Hauteur** — nombre de lignes maximum.
- **Si trop long** — soit couper avec des points de suspension, soit **faire
  défiler** le texte sur une seule ligne en boucle.
- **Tout en majuscules** — plus lisible de loin sur un bandeau.

Un aperçu montre en permanence **exactement** ce qui part dans le fichier,
défilement compris : ce que vous voyez est ce que voit l'assemblée.

## Minuteur libre

Pour les demandes de dernière minute — « mets-nous 5 minutes pour ça » — pas
besoin de créer un programme. En bas de la fenêtre, le bloc **Minuteur libre** :

- Six raccourcis (1, 2, 5, 10, 15, 30 min) qui **démarrent au premier clic**.
- Ou une durée libre dans le champ : `5` pour 5 minutes, `05:30` pour
  5 min 30 s, `01:15:00` pour 1 h 15. `Entrée` lance.

Il écrit dans **les mêmes fichiers** que le programme (`countdown.txt`,
`countup.txt`, `depassement.txt`), donc **votre configuration OBS fonctionne
sans rien changer**, et il bascule en dépassement à zéro exactement pareil.

**Le minuteur libre et le programme ne tournent jamais ensemble.** Lancer le
minuteur met le programme en pause ; démarrer une phase arrête le minuteur.
Un bandeau en bas de fenêtre le signale à chaque fois. C'est délibéré : deux
compteurs écrivant dans le même fichier produiraient un affichage incohérent
à l'antenne.

## Retrouver les chemins pour OBS

**Affichage → Chemins des fichiers pour OBS** (ou le bouton « Chemins OBS »)
ouvre la liste complète : chaque fichier, ce qu'il contient, et son chemin
exact. **Copier le chemin** le met dans le presse-papier, prêt à coller dans
une source Texte. **Copier tous les chemins** prend la liste entière.

La fenêtre n'est pas bloquante : gardez-la ouverte à côté d'OBS pendant que
vous créez vos sources.

## Une seule instance à la fois

Si TimeOverlay est déjà ouvert, un second lancement affiche un avertissement et
s'arrête. Deux copies écriraient dans les mêmes fichiers et OBS afficherait des
valeurs qui sautent — un double-clic de trop suffirait à gâcher un direct.

## Heure de début du culte

Dans l'éditeur de programme, cochez **« Le culte commence à »** et indiquez
l'heure. Deux effets :

**Un décompte avant le début** → `avant_debut.txt`. Pointez une source Texte
dessus sur votre écran d'accueil : pendant que l'assemblée s'installe, elle
affiche le temps restant, et **elle se vide toute seule** au démarrage du culte.
Comme une source Texte vide n'affiche rien, la mention disparaît sans que vous
ayez à toucher à OBS.

**Le démarrage automatique**, si vous cochez l'option. À l'heure dite, la
**première phase seulement** se lance. Les suivantes restent manuelles : une
phase déborde presque toujours, et rien ne doit jamais être coupé en direct.

Deux garde-fous :

- Si vous ouvrez l'application **après** l'heure prévue — à 10h15 pour un culte
  à 10h00 — **rien ne se lance**. L'application ne déclenche que si elle a vu
  l'heure arriver pendant qu'elle tournait. Une heure oubliée de la semaine
  passée ne peut pas lancer le culte à votre insu.
- Si vous appuyez sur `Démarrer` avant l'heure, le décompte s'efface et le
  démarrage automatique est annulé.

### Alerte visuelle

L'affichage passe du vert à l'orange puis au rouge à mesure que le temps
s'épuise, et prend une couleur distincte en dépassement. Les deux seuils et
les quatre couleurs se règlent dans les paramètres.

## Intégration OBS

### Méthode simple

Pour chaque valeur voulue : ajoutez une source **Texte (GDI+)**, cochez
**Lire à partir d'un fichier** et pointez le fichier correspondant.
OBS relit le fichier environ une fois par seconde.

### Méthode fluide (recommandée pour un compte à rebours)

Copiez `obs/TimeOverlay_Helper.lua` dans le dossier de scripts d'OBS, puis
`Outils → Scripts → +`. Renseignez le dossier TimeOverlay et, pour chaque
valeur, le **nom exact** de la source Texte à mettre à jour. Le rafraîchissement
passe à 10 Hz, ce qui supprime les à-coups sur les dernières secondes.

## Overlay

Une fenêtre flottante toujours au premier plan, déplaçable à la souris,
affichant l'heure et le compte à rebours. Utile pour un écran de retour
face à l'orateur, sans passer par OBS. Menu `Affichage → Afficher l'overlay`.

**Sur un poste à deux écrans, elle s'ouvre sur le second** — c'est son usage
réel : un retour face à l'intervenant. Sur un seul écran, elle recouvrira
forcément la régie, il n'y a pas d'échappatoire ; elle se pose alors en bas à
droite, là où elle gêne le moins.

Déplacez-la où vous voulez : sa position est retenue d'une ouverture à l'autre,
et une position devenue hors écran (après avoir débranché un second moniteur)
est automatiquement corrigée. Les lignes vides ne sont pas affichées — tant
qu'aucun minuteur ne tourne, seule l'heure apparaît.

Si vous n'avez qu'un écran et qu'OBS affiche déjà les compteurs, l'overlay ne
vous sert probablement à rien : laissez-le fermé.

Sous Windows elle utilise l'API native (`HWND_TOPMOST`) pour tenir au-dessus
des applications plein écran ; ailleurs elle s'appuie sur le gestionnaire de
fenêtres.

**Le déplacement passe par le compositeur** (`startSystemMove`). C'est ce qui
le fait fonctionner sous Wayland, où une application n'a pas le droit de se
positionner elle-même : le glisser-déposer y était auparavant sans effet, la
fenêtre restait collée là où elle était apparue.

## Notes techniques

- Le temps est calculé sur une **horloge monotone**, pas en comptant les
  impulsions d'un minuteur. Une prédication de 45 minutes ne dérive pas.
- Les fichiers sont écrits **de façon atomique** (fichier temporaire puis
  renommage) : OBS ne peut jamais lire un fichier à moitié écrit, ce qui
  provoquerait un clignotement à l'antenne.
- Un fichier n'est réécrit **que si sa valeur a changé**.
- Si le dossier de sortie devient inaccessible (clé USB retirée, disque plein),
  l'application vous prévient au lieu d'échouer en silence.
