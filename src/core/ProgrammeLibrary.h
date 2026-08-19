#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include "Profile.h"
#include "../utils/CSVExporter.h"

// Bibliotheque des programmes enregistres.
//
// Avant, un programme n'existait que si l'operateur pensait a l'enregistrer
// quelque part, puis a le retrouver. La bibliotheque est un dossier connu de
// l'application : un programme cree y est ecrit tout seul, et la liste des
// programmes est simplement le contenu de ce dossier. Aucun index a tenir a
// jour, donc rien qui puisse se desynchroniser du disque.
//
// Le dossier est passe au constructeur plutot que lu d'une constante globale :
// le test de fumee travaille ainsi dans un dossier temporaire sans toucher a
// la bibliotheque reelle de l'utilisateur.
class ProgrammeLibrary
{
public:
    // Un programme enregistre, tel qu'il apparait dans la liste.
    struct Entry {
        QString path;
        QString name;
        int phaseCount = 0;
        int totalSeconds = 0;
        QDateTime savedAt;
        QDateTime lastUsedAt;   // invalide si le programme n'a jamais ete deroule
        int sessionCount = 0;
    };

    // Un culte reellement deroule avec ce programme.
    struct Session {
        QDateTime startedAt;
        int plannedSeconds = 0;
        int actualSeconds = 0;
        QVector<SessionEntry> phases;

        int deltaSeconds() const { return actualSeconds - plannedSeconds; }
    };

    explicit ProgrammeLibrary(const QString& directory);

    // ~/.local/share/TimeOverlay/programmes sous Linux,
    // %APPDATA%\TimeOverlay\programmes sous Windows.
    static QString defaultDirectory();

    QString directory() const { return m_dir; }

    // Chemin qu'aurait un programme portant ce nom. Le nom est assaini : deux
    // noms differents peuvent donc donner le meme fichier, et le second
    // remplace le premier. C'est voulu -- la bibliotheque est indexee par nom,
    // comme le serait un classeur.
    QString pathFor(const QString& programmeName) const;

    // Ce fichier appartient-il a la bibliotheque ? Sert a distinguer un
    // programme range d'un fichier ouvert depuis le disque de l'utilisateur.
    bool contains(const QString& path) const;

    // Les programmes lisibles du dossier, le plus recemment utilise en tete.
    // Un fichier illisible est ignore plutot que de faire echouer la liste :
    // un programme corrompu ne doit pas rendre les autres inaccessibles.
    QVector<Entry> entries() const;

    bool save(const Profile& profile, QString* savedPath, QString* errorMessage);
    bool remove(const QString& path, QString* errorMessage);

    // Le programme enregistre sous previousPath a change de nom : son ancien
    // fichier disparait et son historique suit le nouveau nom.
    //
    // Sans cela, renommer un programme puis l'enregistrer en laissait deux
    // dans la bibliotheque -- l'ancien avec tout l'historique, le nouveau
    // vide. Un dimanche matin, on lance le mauvais.
    //
    // A appeler APRES save(), qui a deja ecrit le fichier sous le nouveau nom.
    bool dropPreviousName(const QString& previousPath, const QString& newName,
                          QString* errorMessage = nullptr);

    // Historique : ce qui s'est reellement passe, culte apres culte. C'est la
    // donnee qui permet d'ajuster le programme de la fois suivante.
    bool appendSession(const QString& programmeName,
                       const QVector<SessionEntry>& phases,
                       QString* errorMessage = nullptr);
    QVector<Session> sessions(const QString& programmeName) const;

    // L'historique est plafonne : un fichier qui grossit sans fin finit par
    // ralentir l'ouverture de la fenetre, et personne ne consulte le culte
    // d'il y a deux ans.
    static constexpr int kMaxSessionsKept = 100;

private:
    QString historyPath(const QString& programmeName) const;

    QString m_dir;
};
