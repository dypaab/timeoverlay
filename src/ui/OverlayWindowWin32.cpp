#include "OverlayWindowWin32.h"

#include <windows.h>
#include <dwmapi.h>

OverlayWindowWin32::OverlayWindowWin32(QWidget *parent) : OverlayWindow(parent)
{
    // WindowDoesNotAcceptFocus evite que l'overlay vole le focus au logiciel
    // de diffusion quand il apparait.
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool
                   | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
}

void OverlayWindowWin32::setTopMost(bool enable)
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;

    SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void OverlayWindowWin32::setClickThrough(bool enable)
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enable) {
        // WS_EX_TRANSPARENT n'a d'effet qu'accompagne de WS_EX_LAYERED.
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
    } else {
        // WS_EX_LAYERED est conserve : Qt s'en sert pour la transparence de
        // la fenetre, le retirer casserait le rendu.
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void OverlayWindowWin32::showEvent(QShowEvent *event)
{
    OverlayWindow::showEvent(event);
    setTopMost(true);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;

    // Etend le cadre sur toute la zone client pour que le canal alpha soit
    // respecte. Les quatre marges doivent valoir -1 : l'ancienne version
    // n'en initialisait qu'une seule, les trois autres restant a zero.
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}
