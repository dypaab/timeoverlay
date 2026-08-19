#pragma once
#include <QObject>
#include <QDateTime>
#include <QVector>
#include "Profile.h"
#include "Timer.h"
#include "../utils/CSVExporter.h"

// Deroule le programme d'un culte, phase par phase.
//
// Deux choix importants par rapport a la version precedente :
//
// 1. Un seul Timer est cree puis reutilise. L'ancienne version en detruisait
//    un et en creait un autre a chaque phase ; le minuteur detruit continuait
//    d'emettre jusqu'au retour a la boucle d'evenements et ecrasait
//    l'affichage de la phase suivante.
//
// 2. L'enchainement automatique est desactive par defaut. Pendant un culte,
//    c'est l'operateur qui decide quand on passe a la suite : le chant n'est
//    pas fini parce que le minuteur est arrive a zero. A zero on affiche le
//    depassement et on attend.
class PhaseManager : public QObject
{
    Q_OBJECT
public:
    explicit PhaseManager(QObject *parent = nullptr);

    void loadProfile(const Profile& profile);
    const Profile& profile() const { return m_profile; }

    void start();          // demarre ou reprend la phase courante
    void pause();
    void reset();          // revient au debut du programme
    void nextPhase();
    void previousPhase();
    void goToPhase(int index);

    // Ajoute du temps a la phase en cours sans casser le decompte.
    void addSecondsToCurrentPhase(int delta);

    void setAutoAdvance(bool enabled) { m_autoAdvance = enabled; }
    bool autoAdvance() const { return m_autoAdvance; }

    int currentPhaseIndex() const { return m_currentIndex; }
    int totalPhases() const { return int(m_profile.phases.size()); }
    QString currentPhaseName() const;
    QString nextPhaseName() const;

    Timer* timer() const { return m_timer; }
    Timer::State currentState() const;

    // Compte rendu de la seance : prevu face au reel, phase par phase.
    const QVector<SessionEntry>& sessionLog() const { return m_sessionLog; }
    void clearSessionLog() { m_sessionLog.clear(); }

    // Archive la phase en cours sans la quitter.
    //
    // Une phase n'entre dans le compte rendu qu'au moment ou on la quitte. La
    // derniere phase d'un culte, elle, n'est jamais quittee : sans cet appel
    // elle manquerait a l'historique, et c'est justement la predication.
    // Appeler a la fin d'une seance : fermeture de l'application ou passage a
    // un autre programme.
    void endSession();

    static QString stateLabel(Timer::State s);

signals:
    void phaseChanged(int index, QString name, int durationSeconds);
    void tick(QString countdown, QString overtime, Timer::State state);
    void phaseFinished(int index, QString name);
    void allPhasesFinished();
    void stateChanged(Timer::State state);

private:
    Profile m_profile;
    int m_currentIndex = -1;
    Timer* m_timer = nullptr;
    bool m_autoAdvance = false;

    QVector<SessionEntry> m_sessionLog;
    QDateTime m_currentPhaseStartedAt;

    void startPhase(int index);
    void recordCurrentPhase();
    bool hasPhase(int index) const;
};
