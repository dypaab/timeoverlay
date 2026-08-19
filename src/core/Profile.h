#pragma once
#include <QString>
#include <QVector>
#include <QTime>
#include <QJsonObject>

// Une phase du programme : "Louange", "Prédication", "Annonces"...
struct Phase {
    QString name;
    int durationSeconds = 0;      // toujours initialise : une valeur non
                                  // initialisee lue depuis un JSON malforme
                                  // etait un comportement indefini
    bool overtimeEnabled = true;  // certaines phases doivent s'arreter net

    QJsonObject toJson() const;
    static Phase fromJson(const QJsonObject& obj);

    static constexpr int kMaxDurationSeconds = 24 * 3600;
};

// Programme complet d'un culte.
class Profile
{
public:
    QString name;
    QVector<Phase> phases;

    // Heure a laquelle le culte commence, au format "HH:mm". Vide : aucun
    // demarrage programme. Elle ne concerne que la premiere phase -- les
    // suivantes s'enchainent a la main, parce qu'elles debordent presque
    // toujours et que rien ne doit jamais etre coupe en direct.
    QString startTime;

    // Demarrer la premiere phase tout seul a l'heure dite.
    bool autoStart = false;

    // Heure de debut exploitable, ou invalide si absente ou malformee.
    QTime startTimeAsTime() const;

    int totalDuration() const;
    bool isValid() const { return !phases.isEmpty(); }

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject& obj);

    // Renvoie un profil vide et renseigne errorMessage en cas d'echec.
    // L'ancienne version renvoyait un profil vide sans distinguer un fichier
    // absent d'un fichier corrompu.
    static Profile fromFile(const QString& path, QString* errorMessage = nullptr);
    bool saveToFile(const QString& path, QString* errorMessage = nullptr) const;

    // Un programme de culte reste petit. Cette limite empeche qu'un fichier
    // volumineux, accidentel ou malveillant, sature la memoire.
    static constexpr qint64 kMaxFileSizeBytes = 1024 * 1024;  // 1 Mo
    static constexpr int kMaxPhases = 500;
};
