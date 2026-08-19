#include "ColorManager.h"
#include <QtGlobal>

ColorManager::ColorManager(QObject *parent) : QObject(parent)
{
}

QColor ColorManager::colorFor(int remainingSeconds, bool inOvertime) const
{
    if (inOvertime) return m_overtime;
    if (remainingSeconds <= m_criticalThreshold) return m_critical;
    if (remainingSeconds <= m_warningThreshold) return m_warning;
    return m_normal;
}

void ColorManager::setWarningThreshold(int seconds)
{
    m_warningThreshold = qMax(0, seconds);
    // Le seuil critique doit rester en deca du seuil d'avertissement, sinon
    // l'affichage passerait au rouge avant l'orange.
    if (m_criticalThreshold > m_warningThreshold) {
        m_criticalThreshold = m_warningThreshold;
    }
}

void ColorManager::setCriticalThreshold(int seconds)
{
    m_criticalThreshold = qBound(0, seconds, m_warningThreshold);
}

void ColorManager::setColors(QColor normal, QColor warning, QColor critical, QColor overtime)
{
    if (normal.isValid())   m_normal = normal;
    if (warning.isValid())  m_warning = warning;
    if (critical.isValid()) m_critical = critical;
    if (overtime.isValid()) m_overtime = overtime;
}
