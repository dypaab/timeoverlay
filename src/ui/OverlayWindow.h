#pragma once
#include <QWidget>
#include <QLabel>
#include <QColor>

class QMouseEvent;
class QFrame;

// Fenetre flottante posee par-dessus les autres, deplacable a la souris.
// Sert de retour visuel a l'intervenant quand on ne passe pas par OBS.
class OverlayWindow : public QWidget
{
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget *parent = nullptr);

    void setClockText(const QString& text);
    void setTimerText(const QString& text);
    void setColor(const QColor& color);

    // Redefinies sous Windows par l'implementation Win32.
    virtual void setTopMost(bool enable);
    virtual void setClickThrough(bool enable);

    // Replace l'overlay en haut a droite de l'ecran.
    void moveToDefaultCorner();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    QFrame* m_panel = nullptr;
    QLabel* m_clockLabel = nullptr;
    QLabel* m_timerLabel = nullptr;

private:
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_positionRestored = false;
    QColor m_timerColor = QColor("#22c55e");

    void applyTimerStyle();
};

// Renvoie l'implementation adaptee a la plateforme : la version Win32 sous
// Windows, la version Qt ailleurs. L'ancienne version n'instanciait jamais
// OverlayWindowWin32, si bien que tout le code Win32 etait mort alors que le
// README annoncait un overlay natif.
OverlayWindow* createOverlayWindow(QWidget *parent = nullptr);
