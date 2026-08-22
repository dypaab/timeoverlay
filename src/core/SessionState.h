#pragma once
#include <QString>
#include <QDateTime>

// Ou en etait le culte, pour pouvoir y revenir.
//
// L'application peut disparaitre en plein direct : un clic de trop sur la
// croix, une coupure, un plantage. Le culte, lui, continue. Il faut donc
// pouvoir rouvrir et reprendre la phase en cours, au bon endroit.
//
// La cle du procede : on enregistre **l'heure absolue de debut de phase**, pas
// un compteur de secondes. Au retour, le temps ecoule se recalcule sur
// l'horloge du systeme et reste juste, quelle que soit la duree de l'absence
// et quelle que soit la frequence d'enregistrement. Une phase en pause, elle,
// conserve son temps fige : le culte etait reellement suspendu.
//
// Consequence pratique : rien n'a besoin d'etre ecrit chaque seconde. Trois
// evenements suffisent -- changement de phase, changement d'etat, ajustement
// de duree.
struct SessionState
{
    QString profilePath;          // fichier du programme, pour le recharger
    QString profileName;
    int phaseIndex = -1;
    QString phaseName;
    int phaseDurationSeconds = 0; // duree effective, ajustements +/- 1 min compris

    QDateTime phaseStartedAt;     // heure reelle de demarrage de la phase
    int frozenElapsedSeconds = -1;// >= 0 uniquement si la phase etait en pause
    QDateTime savedAt;

    bool isRunning() const { return frozenElapsedSeconds < 0; }
    bool isValid() const;

    // Temps ecoule a reconstituer maintenant : recalcule depuis l'horloge pour
    // une phase qui tournait, fige pour une phase en pause.
    int elapsedSecondsNow() const;

    // Depuis combien de temps l'application est-elle absente.
    int secondsSinceSaved() const;

    // Au-dela, on ne propose plus de reprendre : il ne s'agit plus d'un
    // incident, mais d'un autre jour. La duree couvre largement un culte et
    // le temps de s'apercevoir que le logiciel s'est ferme.
    static constexpr int kMaxAgeSeconds = 2 * 3600;

    bool save(const QString& path, QString* errorMessage = nullptr) const;
    static SessionState load(const QString& path);
    static void discard(const QString& path);

    // <donnees>/session.json, a cote du verrou d'instance unique.
    static QString defaultPath();
};
