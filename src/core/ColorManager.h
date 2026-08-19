#pragma once
#include <QColor>
#include <QObject>

// Couleur de l'affichage selon le temps restant.
// C'est le signal visuel donne a l'intervenant : vert tant qu'il a le temps,
// orange quand il doit conclure, rouge quand il n'a plus rien, et une
// couleur distincte une fois en depassement.
class ColorManager : public QObject
{
    Q_OBJECT
public:
    explicit ColorManager(QObject *parent = nullptr);

    // remainingSeconds : temps restant ; inOvertime : on a depasse la duree.
    QColor colorFor(int remainingSeconds, bool inOvertime) const;

    void setWarningThreshold(int seconds);
    void setCriticalThreshold(int seconds);
    int warningThreshold() const { return m_warningThreshold; }
    int criticalThreshold() const { return m_criticalThreshold; }

    void setColors(QColor normal, QColor warning, QColor critical, QColor overtime);
    QColor normalColor() const { return m_normal; }
    QColor warningColor() const { return m_warning; }
    QColor criticalColor() const { return m_critical; }
    QColor overtimeColor() const { return m_overtime; }

private:
    int m_warningThreshold = 300;  // 5 min : "prepare-toi a conclure"
    int m_criticalThreshold = 60;  // 1 min : "conclus maintenant"

    QColor m_normal   = QColor("#22c55e");
    QColor m_warning  = QColor("#f59e0b");
    QColor m_critical = QColor("#ef4444");
    QColor m_overtime = QColor("#dc2626");
};
