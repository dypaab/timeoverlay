#include "PhaseManager.h"

PhaseManager::PhaseManager(QObject *parent) : QObject(parent)
{
    // Un seul minuteur pour toute la duree de vie du gestionnaire : on
    // change sa duree a chaque phase plutot que de le detruire et le
    // recreer. Les connexions sont donc etablies une fois pour toutes.
    m_timer = new Timer(QStringLiteral("Programme"), 0, this);

    connect(m_timer, &Timer::tick, this,
            [this](const QString& cd, const QString& ot, Timer::State s) {
        emit tick(cd, ot, s);
    });

    connect(m_timer, &Timer::stateChanged, this, [this](Timer::State s) {
        emit stateChanged(s);
    });

    connect(m_timer, &Timer::finished, this, [this]() {
        if (!hasPhase(m_currentIndex)) return;
        emit phaseFinished(m_currentIndex, currentPhaseName());

        if (m_autoAdvance) {
            nextPhase();
        }
    });
}

bool PhaseManager::hasPhase(int index) const
{
    return index >= 0 && index < m_profile.phases.size();
}

void PhaseManager::loadProfile(const Profile& profile)
{
    m_profile = profile;
    m_currentIndex = -1;
    m_sessionLog.clear();
    m_currentPhaseStartedAt = QDateTime();
    m_timer->reset();
    m_timer->setDuration(0);
}

void PhaseManager::start()
{
    if (m_profile.phases.isEmpty()) return;

    // Aucune phase encore active : on demarre la premiere.
    if (!hasPhase(m_currentIndex)) {
        startPhase(0);
        return;
    }

    // Sinon on reprend simplement la phase en cours.
    m_timer->start();
}

void PhaseManager::pause()
{
    m_timer->pause();
}

void PhaseManager::reset()
{
    m_timer->reset();
    m_currentIndex = -1;
    m_currentPhaseStartedAt = QDateTime();
    m_timer->setDuration(0);
}

void PhaseManager::endSession()
{
    recordCurrentPhase();
    // La date de debut est effacee : recordCurrentPhase() l'exige valide, ce
    // qui garantit qu'un second appel n'archive pas la phase deux fois.
    m_currentPhaseStartedAt = QDateTime();
}

void PhaseManager::recordCurrentPhase()
{
    if (!hasPhase(m_currentIndex) || !m_currentPhaseStartedAt.isValid()) {
        return;
    }

    SessionEntry entry;
    entry.startedAt = m_currentPhaseStartedAt;
    entry.phaseName = m_profile.phases[m_currentIndex].name;
    entry.plannedSeconds = m_profile.phases[m_currentIndex].durationSeconds;
    entry.actualSeconds = m_timer->elapsedSeconds();
    m_sessionLog.append(entry);
}

void PhaseManager::startPhase(int index)
{
    if (!hasPhase(index)) return;

    // Archive la phase qu'on quitte avant d'ecraser l'etat du minuteur.
    recordCurrentPhase();

    m_currentIndex = index;
    const Phase& phase = m_profile.phases[index];

    m_timer->reset();
    m_timer->setDuration(phase.durationSeconds);
    m_timer->setOvertimeEnabled(phase.overtimeEnabled);
    m_currentPhaseStartedAt = QDateTime::currentDateTime();

    emit phaseChanged(index, phase.name, phase.durationSeconds);
    m_timer->start();
}

void PhaseManager::nextPhase()
{
    if (m_currentIndex + 1 < m_profile.phases.size()) {
        startPhase(m_currentIndex + 1);
    } else {
        recordCurrentPhase();
        m_timer->pause();
        emit allPhasesFinished();
    }
}

void PhaseManager::previousPhase()
{
    if (m_currentIndex > 0) {
        startPhase(m_currentIndex - 1);
    }
}

void PhaseManager::goToPhase(int index)
{
    if (hasPhase(index)) {
        startPhase(index);
    }
}

void PhaseManager::resumePhase(int index, int durationSeconds, int elapsedSeconds, bool running)
{
    if (!hasPhase(index)) return;

    m_currentIndex = index;
    const Phase& phase = m_profile.phases[index];

    m_timer->setDuration(durationSeconds > 0 ? durationSeconds : phase.durationSeconds);
    m_timer->setOvertimeEnabled(phase.overtimeEnabled);

    // La date de debut est reconstituee, pas relevee maintenant : le compte
    // rendu de seance doit porter l'heure a laquelle la phase a reellement
    // commence, avant la fermeture.
    m_currentPhaseStartedAt = QDateTime::currentDateTime().addSecs(-elapsedSeconds);

    emit phaseChanged(index, phase.name, m_timer->durationSeconds());
    m_timer->resumeAt(elapsedSeconds, running);
}

void PhaseManager::addSecondsToCurrentPhase(int delta)
{
    if (!hasPhase(m_currentIndex)) return;
    m_timer->addSeconds(delta);
}

QString PhaseManager::currentPhaseName() const
{
    if (!hasPhase(m_currentIndex)) return QString();
    return m_profile.phases[m_currentIndex].name;
}

QString PhaseManager::nextPhaseName() const
{
    const int next = m_currentIndex + 1;
    if (!hasPhase(next)) return QString();
    return m_profile.phases[next].name;
}

Timer::State PhaseManager::currentState() const
{
    return m_timer->state();
}

QString PhaseManager::stateLabel(Timer::State s)
{
    switch (s) {
        case Timer::State::STOPPED:  return QStringLiteral("ARRETE");
        case Timer::State::RUNNING:  return QStringLiteral("EN_COURS");
        case Timer::State::PAUSED:   return QStringLiteral("PAUSE");
        case Timer::State::FINISHED: return QStringLiteral("TERMINE");
        case Timer::State::OVERTIME: return QStringLiteral("DEPASSEMENT");
    }
    return QStringLiteral("ARRETE");
}
