#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include "TextFormatter.h"

class OutputEngine;

// Messages a l'ecran pendant le direct.
//
// Deux canaux distincts, chacun avec son fichier :
//
//   message.txt  -- le message ponctuel tape par l'operateur ("un enfant vous
//                   attend a l'accueil"). Reste affiche jusqu'a effacement.
//
//   annonce.txt  -- une liste d'annonces qui tournent automatiquement a
//                   intervalle reglable. Equivalent du Textline Changer de
//                   Snaz, sans sa limite de trois lignes.
//
// Chaque canal a sa propre mise en forme : le bandeau d'assemblee et l'ecran
// de retour de l'orateur n'ont ni la meme largeur ni le meme nombre de lignes.
class MessageCenter : public QObject
{
    Q_OBJECT
public:
    explicit MessageCenter(OutputEngine* output, QObject *parent = nullptr);

    // --- Message ponctuel ---
    void setMessage(const QString& text);
    QString message() const { return m_message; }
    void clearMessage() { setMessage(QString()); }

    void setMessageFormat(const TextFormat& format);
    TextFormat messageFormat() const { return m_messageFormat; }

    // --- Annonces rotatives ---
    void setAnnouncements(const QStringList& lines);
    QStringList announcements() const { return m_announcements; }

    void setRotationEnabled(bool enabled);
    bool rotationEnabled() const { return m_rotationEnabled; }

    void setRotationSeconds(int seconds);
    int rotationSeconds() const { return m_rotationSeconds; }

    void setAnnouncementFormat(const TextFormat& format);
    TextFormat announcementFormat() const { return m_announcementFormat; }

    // Passe immediatement a l'annonce suivante.
    void nextAnnouncement();

signals:
    // Emis a chaque changement de rendu, pour que l'overlay et l'ecran de
    // controle affichent exactement ce qui part dans les fichiers.
    void messageRendered(const QString& rendered);
    void announcementRendered(const QString& rendered);

private:
    OutputEngine* m_output = nullptr;

    QString m_message;
    TextFormat m_messageFormat;

    QStringList m_announcements;
    TextFormat m_announcementFormat;
    int m_currentAnnouncement = 0;
    bool m_rotationEnabled = false;
    int m_rotationSeconds = 10;

    QTimer m_rotationTimer;
    QTimer m_scrollTimer;
    int m_messageScrollOffset = 0;
    int m_announcementScrollOffset = 0;

    void renderMessage();
    void renderAnnouncement();
    void updateScrollTimer();
    QString currentAnnouncementText() const;
};
