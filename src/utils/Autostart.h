#pragma once
#include <QString>

// Lancement de TimeOverlay a l'ouverture de la session.
//
// L'application affiche une heure et un decompte avant le debut du culte :
// elle ne sert a rien si personne ne pense a la lancer. Elle doit donc etre
// deja ouverte quand on arrive a la regie.
//
// Deux mecanismes, un par systeme :
//   Linux   : un fichier .desktop dans ~/.config/autostart, lu par toutes les
//             sessions de bureau conformes a la specification XDG.
//   Windows : la cle de registre Run de l'utilisateur courant.
//
// Dans les deux cas c'est l'utilisateur seul qui est concerne : aucun droit
// administrateur n'est demande, et aucune autre session n'est touchee.
namespace Autostart {

// Faux sur les systemes ou aucun des deux mecanismes n'existe. L'interface
// masque alors l'option plutot que d'offrir un reglage sans effet.
bool isSupported();

bool isEnabled();

// Renvoie false et renseigne errorMessage si l'ecriture echoue.
bool setEnabled(bool enabled, QString* errorMessage = nullptr);

} // namespace Autostart
