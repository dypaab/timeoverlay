#include "Alarm.h"
#include <QUrl>
#include <QFileInfo>
#include <QApplication>
#include <QtGlobal>

Alarm::Alarm(QObject *parent) : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(1.0);

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& errorString) {
        // Le son configure ne peut pas etre lu : on previent, et on fait
        // quand meme du bruit. Une alarme muette pendant un culte serait pire
        // qu'une alarme approximative.
        emit soundError(errorString);
        QApplication::beep();
    });
}

void Alarm::setSoundFile(const QString& path)
{
    m_soundFile = path;
    if (!m_soundFile.isEmpty() && QFileInfo::exists(m_soundFile)) {
        m_player->setSource(QUrl::fromLocalFile(m_soundFile));
    } else {
        m_player->setSource(QUrl());
    }
}

void Alarm::setVolume(qreal volume)
{
    m_audioOutput->setVolume(qBound(qreal(0.0), volume, qreal(1.0)));
}

qreal Alarm::volume() const
{
    return m_audioOutput->volume();
}

void Alarm::trigger()
{
    if (!m_enabled) return;

    if (!m_soundFile.isEmpty() && QFileInfo::exists(m_soundFile)) {
        // Rembobine : sans cela, un second declenchement ne rejoue rien
        // puisque la lecture est deja arrivee a la fin.
        m_player->stop();
        m_player->setPosition(0);
        m_player->play();
        return;
    }

    QApplication::beep();
}

void Alarm::stop()
{
    m_player->stop();
}
