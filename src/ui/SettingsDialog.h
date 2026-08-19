#pragma once
#include <QDialog>
#include <QColor>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

// Parametres de l'application. Toutes les valeurs sont fournies par
// l'appelant avant exec() et relues apres acceptation : le dialogue ne
// connait ni les reglages persistants ni le moteur de sortie.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void setOutputDir(const QString& dir);
    void setTimeFormat(const QString& format);
    void setDateFormat(const QString& format);
    void setWarningThreshold(int seconds);
    void setCriticalThreshold(int seconds);
    void setColors(QColor normal, QColor warning, QColor critical, QColor overtime);
    void setAlarmEnabled(bool enabled);
    void setAlarmSound(const QString& path);
    void setAutoAdvance(bool enabled);
    void setAutostart(bool enabled);

    QString outputDir() const;
    QString timeFormat() const;
    QString dateFormat() const;
    int warningThreshold() const;
    int criticalThreshold() const;
    QColor normalColor() const   { return m_normal; }
    QColor warningColor() const  { return m_warning; }
    QColor criticalColor() const { return m_critical; }
    QColor overtimeColor() const { return m_overtime; }
    bool alarmEnabled() const;
    QString alarmSound() const;
    bool autoAdvance() const;
    bool autostart() const;

private:
    QPushButton* makeColorButton(const QString& label, QColor* target);
    void updateColorButton(QPushButton* button, const QColor& color);

    QLineEdit* m_outputDir = nullptr;
    QLineEdit* m_timeFormat = nullptr;
    QLineEdit* m_dateFormat = nullptr;
    QSpinBox* m_warningThreshold = nullptr;
    QSpinBox* m_criticalThreshold = nullptr;
    QCheckBox* m_alarmEnabled = nullptr;
    QLineEdit* m_alarmSound = nullptr;
    QCheckBox* m_autoAdvance = nullptr;
    QCheckBox* m_autostart = nullptr;

    QPushButton* m_btnNormal = nullptr;
    QPushButton* m_btnWarning = nullptr;
    QPushButton* m_btnCritical = nullptr;
    QPushButton* m_btnOvertime = nullptr;

    QColor m_normal   = QColor("#22c55e");
    QColor m_warning  = QColor("#f59e0b");
    QColor m_critical = QColor("#ef4444");
    QColor m_overtime = QColor("#dc2626");
};
