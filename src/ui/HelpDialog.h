#pragma once
#include <QDialog>

class QTextBrowser;

// Aide de l'application.
//
// Le mode d'emploi n'est pas un texte a lire puis a appliquer de memoire : les
// endroits qu'il cite sont cliquables et ouvrent directement l'ecran concerne.
// La fenetre reste ouverte a cote pendant qu'on suit les etapes -- c'est le
// meme besoin que pour les chemins OBS, qu'on recopie un par un.
class HelpDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialog(const QString& outputDir, QWidget *parent = nullptr);

signals:
    // Un lien d'action a ete suivi. L'identifiant vaut "programmes",
    // "nouveau", "modifier", "chemins", "dossier", "parametres" ou "overlay".
    // La fenetre principale sait quoi en faire ; l'aide, elle, n'a pas a
    // connaitre ses slots.
    void actionRequested(const QString& id);

private:
    QTextBrowser* m_browser = nullptr;
};
