#include "MessageCenter.h"
#include "OutputEngine.h"
#include <QtGlobal>

namespace {
// Cadence du defilement. 150 ms donne un mouvement lisible sans donner
// l'impression que le texte court.
constexpr int kScrollIntervalMs = 150;
}

MessageCenter::MessageCenter(OutputEngine* output, QObject *parent)
    : QObject(parent), m_output(output)
{
    m_rotationTimer.setInterval(m_rotationSeconds * 1000);
    connect(&m_rotationTimer, &QTimer::timeout, this, &MessageCenter::nextAnnouncement);

    m_scrollTimer.setInterval(kScrollIntervalMs);
    connect(&m_scrollTimer, &QTimer::timeout, this, [this]() {
        ++m_messageScrollOffset;
        ++m_announcementScrollOffset;
        if (m_messageFormat.overflow == TextFormat::Overflow::Scroll) renderMessage();
        if (m_announcementFormat.overflow == TextFormat::Overflow::Scroll) renderAnnouncement();
    });
}

// --------------------------------------------------------------- message

void MessageCenter::setMessage(const QString& text)
{
    if (m_message == text) return;
    m_message = text;
    // Un nouveau message repart du debut, sinon il apparaitrait tronque au
    // milieu si le precedent defilait.
    m_messageScrollOffset = 0;
    renderMessage();
}

void MessageCenter::setMessageFormat(const TextFormat& format)
{
    if (m_messageFormat == format) return;
    m_messageFormat = format;
    m_messageScrollOffset = 0;
    updateScrollTimer();
    renderMessage();
}

void MessageCenter::renderMessage()
{
    QString rendered;

    if (m_message.isEmpty()) {
        rendered.clear();
    } else if (m_messageFormat.overflow == TextFormat::Overflow::Scroll) {
        const QString flat = TextFormatter::apply(m_message, m_messageFormat);
        rendered = TextFormatter::scrollWindow(flat, m_messageFormat.maxCharsPerLine,
                                               m_messageScrollOffset);
    } else {
        rendered = TextFormatter::apply(m_message, m_messageFormat);
    }

    if (m_output) m_output->set(OutputEngine::Message, rendered);
    emit messageRendered(rendered);
}

// -------------------------------------------------------------- annonces

void MessageCenter::setAnnouncements(const QStringList& lines)
{
    QStringList cleaned;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) cleaned.append(trimmed);
    }

    if (m_announcements == cleaned) return;
    m_announcements = cleaned;

    if (m_currentAnnouncement >= m_announcements.size()) {
        m_currentAnnouncement = 0;
    }
    m_announcementScrollOffset = 0;
    renderAnnouncement();
}

void MessageCenter::setRotationEnabled(bool enabled)
{
    if (m_rotationEnabled == enabled) return;
    m_rotationEnabled = enabled;

    if (enabled && m_announcements.size() > 1) {
        m_rotationTimer.start();
    } else {
        m_rotationTimer.stop();
    }
    renderAnnouncement();
}

void MessageCenter::setRotationSeconds(int seconds)
{
    m_rotationSeconds = qBound(2, seconds, 3600);
    m_rotationTimer.setInterval(m_rotationSeconds * 1000);
}

void MessageCenter::setAnnouncementFormat(const TextFormat& format)
{
    if (m_announcementFormat == format) return;
    m_announcementFormat = format;
    m_announcementScrollOffset = 0;
    updateScrollTimer();
    renderAnnouncement();
}

void MessageCenter::nextAnnouncement()
{
    if (m_announcements.isEmpty()) return;
    m_currentAnnouncement = (m_currentAnnouncement + 1) % m_announcements.size();
    m_announcementScrollOffset = 0;
    renderAnnouncement();
}

QString MessageCenter::currentAnnouncementText() const
{
    if (m_announcements.isEmpty()) return QString();
    if (m_currentAnnouncement < 0 || m_currentAnnouncement >= m_announcements.size()) {
        return m_announcements.first();
    }
    return m_announcements.at(m_currentAnnouncement);
}

void MessageCenter::renderAnnouncement()
{
    // Rotation coupee : on n'affiche rien plutot que de figer une annonce au
    // hasard a l'antenne.
    if (!m_rotationEnabled || m_announcements.isEmpty()) {
        if (m_output) m_output->set(OutputEngine::Annonce, QString());
        emit announcementRendered(QString());
        return;
    }

    const QString source = currentAnnouncementText();
    QString rendered;

    if (m_announcementFormat.overflow == TextFormat::Overflow::Scroll) {
        const QString flat = TextFormatter::apply(source, m_announcementFormat);
        rendered = TextFormatter::scrollWindow(flat, m_announcementFormat.maxCharsPerLine,
                                               m_announcementScrollOffset);
    } else {
        rendered = TextFormatter::apply(source, m_announcementFormat);
    }

    if (m_output) m_output->set(OutputEngine::Annonce, rendered);
    emit announcementRendered(rendered);
}

void MessageCenter::updateScrollTimer()
{
    // Le minuteur de defilement ne tourne que si au moins un canal en a
    // besoin : inutile de reveiller le processus dix fois par seconde pour
    // rien.
    const bool needed =
        m_messageFormat.overflow == TextFormat::Overflow::Scroll
        || m_announcementFormat.overflow == TextFormat::Overflow::Scroll;

    if (needed && !m_scrollTimer.isActive()) {
        m_scrollTimer.start();
    } else if (!needed && m_scrollTimer.isActive()) {
        m_scrollTimer.stop();
    }
}
