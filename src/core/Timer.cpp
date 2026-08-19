#include "Timer.h"
#include <QtGlobal>

namespace {
// Bornes de securite : une duree lue depuis un fichier de programme ne doit
// pas pouvoir provoquer de debordement dans les calculs en millisecondes.
constexpr int kMaxDurationSeconds = 24 * 3600;  // 24 h
}

Timer::Timer(QString name, int seconds, QObject *parent)
    : QObject(parent), m_name(std::move(name))
{
    setDuration(seconds);

    // 100 ms : l'affichage reste reactif au dixieme de seconde pres, et
    // comme le moteur de sortie n'ecrit que ce qui change, une valeur au
    // format HH:mm:ss ne produit qu'une ecriture par seconde.
    m_ticker.setInterval(100);
    connect(&m_ticker, &QTimer::timeout, this, &Timer::onTick);
}

void Timer::setDuration(int seconds)
{
    const int clamped = qBound(0, seconds, kMaxDurationSeconds);
    m_durationMs = qint64(clamped) * 1000;
}

void Timer::addSeconds(int delta)
{
    const qint64 newDurationMs = m_durationMs + qint64(delta) * 1000;
    m_durationMs = qBound(qint64(0), newDurationMs, qint64(kMaxDurationSeconds) * 1000);

    // Ajouter du temps alors qu'on depassait deja doit ramener en decompte.
    if (m_state == State::OVERTIME || m_state == State::FINISHED) {
        if (elapsedMs() < m_durationMs) {
            m_finishedEmitted = false;
            if (!m_elapsed.isValid()) m_elapsed.start();
            m_ticker.start();
            setState(State::RUNNING);
        }
    }
    onTick();
}

qint64 Timer::elapsedMs() const
{
    qint64 total = m_accumulatedMs;
    if (m_state == State::RUNNING || m_state == State::OVERTIME) {
        if (m_elapsed.isValid()) total += m_elapsed.elapsed();
    }
    return total;
}

void Timer::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(m_state);
}

void Timer::start()
{
    if (m_state == State::RUNNING || m_state == State::OVERTIME) {
        return;
    }

    if (m_state == State::STOPPED) {
        m_accumulatedMs = 0;
        m_finishedEmitted = false;
    }

    m_elapsed.start();
    m_ticker.start();
    setState(elapsedMs() < m_durationMs ? State::RUNNING : State::OVERTIME);
    onTick();
}

void Timer::restart()
{
    m_ticker.stop();
    m_accumulatedMs = 0;
    m_finishedEmitted = false;
    m_elapsed.invalidate();
    setState(State::STOPPED);
    start();
}

void Timer::pause()
{
    if (m_state != State::RUNNING && m_state != State::OVERTIME) {
        return;
    }
    // Fige le temps ecoule avant de couper la mesure, sinon la periode en
    // cours serait perdue.
    if (m_elapsed.isValid()) {
        m_accumulatedMs += m_elapsed.elapsed();
        m_elapsed.invalidate();
    }
    m_ticker.stop();
    setState(State::PAUSED);
    onTick();
}

void Timer::reset()
{
    m_ticker.stop();
    m_accumulatedMs = 0;
    m_finishedEmitted = false;
    m_elapsed.invalidate();
    setState(State::STOPPED);
    onTick();
}

int Timer::elapsedSeconds() const
{
    return int(elapsedMs() / 1000);
}

int Timer::remainingSeconds() const
{
    const qint64 remainingMs = m_durationMs - elapsedMs();
    if (remainingMs <= 0) return 0;
    // Arrondi au superieur : tant qu'il reste une fraction de seconde,
    // l'affichage montre encore cette seconde. Un compte a rebours qui
    // affiche 00:00:00 alors qu'il reste 900 ms serait trompeur.
    return int((remainingMs + 999) / 1000);
}

int Timer::overtimeSeconds() const
{
    const qint64 overMs = elapsedMs() - m_durationMs;
    if (overMs <= 0) return 0;
    return int(overMs / 1000);
}

QString Timer::format(qint64 totalSeconds)
{
    if (totalSeconds < 0) totalSeconds = 0;
    const qint64 hours = totalSeconds / 3600;
    const qint64 mins = (totalSeconds % 3600) / 60;
    const qint64 secs = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

bool Timer::parseDuration(const QString& text, int* outSeconds)
{
    if (!outSeconds) return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return false;

    const QStringList parts = trimmed.split(':');
    if (parts.size() > 3) return false;

    qint64 seconds = 0;
    if (parts.size() == 1) {
        // Un nombre seul se lit en minutes : c'est ce qu'on a en tete quand on
        // dit "mets un minuteur de 5".
        bool ok = false;
        const int minutes = parts.first().toInt(&ok);
        if (!ok || minutes < 0) return false;
        seconds = qint64(minutes) * 60;
    } else {
        for (const QString& part : parts) {
            bool ok = false;
            const int component = part.trimmed().toInt(&ok);
            if (!ok || component < 0) return false;
            seconds = seconds * 60 + component;
        }
    }

    if (seconds < 0 || seconds > kMaxDurationSeconds) return false;
    *outSeconds = int(seconds);
    return true;
}

QString Timer::countdown() const
{
    return format(remainingSeconds());
}

QString Timer::overtime() const
{
    const int over = overtimeSeconds();
    if (over <= 0) return QString();
    return QStringLiteral("+") + format(over);
}

QString Timer::countup() const
{
    return format(elapsedMs() / 1000);
}

void Timer::onTick()
{
    const qint64 elapsed = elapsedMs();

    if (elapsed >= m_durationMs && !m_finishedEmitted
        && m_state != State::STOPPED && m_state != State::PAUSED) {
        m_finishedEmitted = true;

        if (m_overtimeEnabled) {
            setState(State::OVERTIME);
            emit finished();
            emit overtimeStarted();
        } else {
            // Fige le temps ecoule sur la duree exacte pour que le countup
            // n'aille pas au-dela de la duree prevue.
            m_accumulatedMs = m_durationMs;
            m_elapsed.invalidate();
            m_ticker.stop();
            setState(State::FINISHED);
            emit finished();
        }
    }

    emit tick(countdown(), overtime(), m_state);
}
