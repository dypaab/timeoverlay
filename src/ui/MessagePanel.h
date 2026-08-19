#pragma once
#include <QWidget>
#include <functional>
#include "../core/TextFormatter.h"

class MessageCenter;
class QPlainTextEdit;
class QListWidget;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;

// Panneau de regie : saisie du message ponctuel et gestion des annonces
// rotatives, avec les reglages de mise en forme de chaque canal.
class MessagePanel : public QWidget
{
    Q_OBJECT
public:
    explicit MessagePanel(MessageCenter* center, QWidget *parent = nullptr);

    // Modeles de messages frequents, conserves entre deux cultes.
    QStringList templates() const;
    void setTemplates(const QStringList& templates);

    QStringList announcements() const;
    void setAnnouncements(const QStringList& lines);

    // Aligne les controles sur une configuration rechargee, sans declencher
    // d'enregistrement ni de reecriture des fichiers.
    void applyStoredState(const TextFormat& messageFormat,
                          const TextFormat& announcementFormat,
                          int rotationSeconds);

signals:
    // Emis quand un reglage change, pour que la fenetre principale
    // enregistre la configuration.
    void settingsChanged();

private slots:
    void onSendMessage();
    void onClearMessage();
    void onSaveTemplate();
    void onRemoveTemplate();
    void onTemplateActivated();

    void onAddAnnouncement();
    void onRemoveAnnouncement();
    void onMoveAnnouncementUp();
    void onMoveAnnouncementDown();
    void onAnnouncementsChanged();

    void onMessageFormatChanged();
    void onAnnouncementFormatChanged();

private:
    struct FormatWidgets {
        QSpinBox* width = nullptr;
        QSpinBox* lines = nullptr;
        QComboBox* overflow = nullptr;
        QCheckBox* uppercase = nullptr;
    };

    QWidget* buildMessageTab();
    QWidget* buildAnnouncementTab();
    QWidget* buildFormatBox(const QString& title, FormatWidgets* widgets,
                            std::function<void()> onChanged);
    TextFormat readFormat(const FormatWidgets& widgets) const;
    void writeFormat(const FormatWidgets& widgets, const TextFormat& format);

    MessageCenter* m_center = nullptr;

    QPlainTextEdit* m_messageEdit = nullptr;
    QListWidget* m_templateList = nullptr;
    QLabel* m_messagePreview = nullptr;
    FormatWidgets m_messageFormat;

    QListWidget* m_announcementList = nullptr;
    QSpinBox* m_rotationSeconds = nullptr;
    QCheckBox* m_rotationEnabled = nullptr;
    QLabel* m_announcementPreview = nullptr;
    FormatWidgets m_announcementFormat;
};
