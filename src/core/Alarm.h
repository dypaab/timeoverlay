#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QString>

// Signal sonore de fin de phase.
//
// L'ancienne version testait m_useSystem avant m_soundFile et rien ne mettait
// jamais m_useSystem a false : le son personnalise etait inatteignable. Ici
// la regle est simple -- un fichier son valide est utilise, sinon on retombe
// sur le bip systeme.
class Alarm : public QObject
{
    Q_OBJECT
public:
    explicit Alarm(QObject *parent = nullptr);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Chemin vide ou fichier introuvable : retour automatique au bip systeme.
    void setSoundFile(const QString& path);
    QString soundFile() const { return m_soundFile; }

    void setVolume(qreal volume);
    qreal volume() const;

    void trigger();
    void stop();

signals:
    void soundError(const QString& message);

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    bool m_enabled = true;
    QString m_soundFile;
};
