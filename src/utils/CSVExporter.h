#pragma once
#include <QString>
#include <QDateTime>
#include <QVector>

// Une ligne du compte rendu de seance : ce qui etait prevu face a ce qui
// s'est reellement passe. C'est la donnee utile apres un culte pour ajuster
// le programme de la fois suivante.
struct SessionEntry {
    QDateTime startedAt;
    QString phaseName;
    int plannedSeconds = 0;
    int actualSeconds = 0;

    int deltaSeconds() const { return actualSeconds - plannedSeconds; }
};

class CSVExporter
{
public:
    // Ecrit le compte rendu complet d'une seance.
    // Renvoie false et renseigne errorMessage en cas d'echec.
    static bool exportSession(const QString& path,
                              const QVector<SessionEntry>& entries,
                              QString* errorMessage = nullptr);

    // Ajoute une ligne au journal du jour. Cree l'en-tete si le fichier
    // vient d'etre cree.
    static bool appendLog(const QString& logDir,
                          const QString& name,
                          const QString& event,
                          const QString& value,
                          QString* errorMessage = nullptr);
};
