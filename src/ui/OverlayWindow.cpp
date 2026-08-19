#include "OverlayWindow.h"
#include <QVBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QWindow>

#ifdef Q_OS_WIN
#include "OverlayWindowWin32.h"
#endif

OverlayWindow::OverlayWindow(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("TimeOverlay"));

    // Une fenetre sans cadre ne dit pas d'elle-meme qu'elle se deplace. Le
    // curseur de main est le seul indice dont dispose l'operateur.
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Glissez pour déplacer cet affichage."));

    // Un cadre peint sert de fond : sans lui, le texte flottait a nu au-dessus
    // des autres fenetres et donnait l'impression d'un defaut d'affichage
    // plutot que d'un element voulu.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_panel = new QFrame(this);
    m_panel->setObjectName(QStringLiteral("overlayPanel"));
    m_panel->setStyleSheet(
        QStringLiteral("QFrame#overlayPanel {"
                       "  background-color: rgba(17, 24, 39, 220);"
                       "  border: 1px solid rgba(255, 255, 255, 40);"
                       "  border-radius: 14px;"
                       "}"));
    outer->addWidget(m_panel);

    auto* layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(26, 18, 26, 18);
    layout->setSpacing(6);

    m_clockLabel = new QLabel(QStringLiteral("00:00:00"), m_panel);
    m_clockLabel->setStyleSheet("color: #e5e7eb; font-size: 30px; font-weight: 600;"
                                " background: transparent;");
    m_clockLabel->setAlignment(Qt::AlignCenter);

    m_timerLabel = new QLabel(QString(), m_panel);
    m_timerLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_clockLabel);
    layout->addWidget(m_timerLabel);

    applyTimerStyle();

    // Etat initial coherent : le minuteur est vide au demarrage, son
    // etiquette doit donc etre masquee des la construction. Sans cela la
    // fenetre garde une bande vide tant qu'aucun minuteur n'a tourne.
    m_timerLabel->setVisible(false);
    adjustSize();
    moveToDefaultCorner();
}

void OverlayWindow::moveToDefaultCorner()
{
    // Sur un poste a deux ecrans, l'overlay va sur le second : c'est son usage
    // reel, un retour face a l'intervenant. Sur un seul ecran il recouvrira
    // forcement la regie -- on le pose alors en bas a droite, la ou il gene le
    // moins.
    const QList<QScreen*> ecrans = QGuiApplication::screens();
    const QScreen* principal = QGuiApplication::primaryScreen();
    const QScreen* cible = principal;

    for (QScreen* ecran : ecrans) {
        if (ecran != principal) { cible = ecran; break; }
    }
    if (!cible) return;

    const QRect zone = cible->availableGeometry();
    constexpr int marge = 24;

    if (cible != principal) {
        // Second ecran : centre, c'est un affichage a part entiere.
        move(zone.center().x() - width() / 2, zone.center().y() - height() / 2);
    } else {
        move(zone.right() - width() - marge, zone.bottom() - height() - marge);
    }
}

void OverlayWindow::applyTimerStyle()
{
    m_timerLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 54px; font-weight: bold; background: transparent;")
            .arg(m_timerColor.name()));
}

void OverlayWindow::setClockText(const QString& text)
{
    m_clockLabel->setText(text);
    // Une etiquette vide occupe quand meme sa hauteur : on la masque, sinon
    // l'overlay reserve une bande inutile.
    m_clockLabel->setVisible(!text.isEmpty());
    adjustSize();
}

void OverlayWindow::setTimerText(const QString& text)
{
    m_timerLabel->setText(text);
    m_timerLabel->setVisible(!text.isEmpty());
    adjustSize();
}

void OverlayWindow::setColor(const QColor& color)
{
    if (!color.isValid() || color == m_timerColor) return;
    m_timerColor = color;
    applyTimerStyle();
}

void OverlayWindow::setTopMost(bool enable)
{
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, enable);
    // Changer un drapeau de fenetre la cache : il faut la reafficher.
    if (wasVisible) show();
}

void OverlayWindow::setClickThrough(bool enable)
{
    // Sur les plateformes non Windows, Qt propose l'attribut transparent aux
    // evenements souris. Le support depend du gestionnaire de fenetres, mais
    // c'est le meilleur equivalent portable.
    setAttribute(Qt::WA_TransparentForMouseEvents, enable);
}

void OverlayWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // La position choisie par l'operateur est conservee d'une ouverture a
    // l'autre : il l'a placee la ou elle ne gene pas.
    if (!m_positionRestored) {
        m_positionRestored = true;
        QSettings settings(QStringLiteral("TimeOverlay"), QStringLiteral("TimeOverlay"));
        const QPoint saved = settings.value(QStringLiteral("overlay/position")).toPoint();

        // Une position enregistree hors des ecrans actuels rendrait l'overlay
        // invisible : cela arrive apres avoir debranche un second ecran.
        bool visible = false;
        for (const QScreen* ecran : QGuiApplication::screens()) {
            if (ecran->availableGeometry().contains(saved)) { visible = true; break; }
        }

        if (!saved.isNull() && visible) {
            move(saved);
        } else {
            moveToDefaultCorner();
        }
    }
}

void OverlayWindow::hideEvent(QHideEvent *event)
{
    QSettings settings(QStringLiteral("TimeOverlay"), QStringLiteral("TimeOverlay"));
    settings.setValue(QStringLiteral("overlay/position"), pos());
    QWidget::hideEvent(event);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    // Sous Wayland, une application n'a pas le droit de se positionner
    // elle-meme : move() est purement et simplement ignore par le
    // compositeur, et la fenetre restait collee la ou elle etait apparue.
    // startSystemMove() demande au compositeur d'effectuer le deplacement,
    // ce qui fonctionne sous Wayland, sous X11 et sous Windows. On ne garde
    // le deplacement a la main que pour les cas ou il n'aboutit pas.
    if (QWindow* handle = windowHandle()) {
        if (handle->startSystemMove()) {
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }

    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}

OverlayWindow* createOverlayWindow(QWidget *parent)
{
#ifdef Q_OS_WIN
    return new OverlayWindowWin32(parent);
#else
    return new OverlayWindow(parent);
#endif
}
