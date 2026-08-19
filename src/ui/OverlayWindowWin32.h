#pragma once
#include "OverlayWindow.h"

// Overlay natif Windows : SetWindowPos(HWND_TOPMOST) tient au-dessus des
// applications plein ecran la ou le simple drapeau Qt lache, et WS_EX_TRANSPARENT
// permet de laisser passer les clics vers la fenetre du dessous.
//
// Ce fichier n'est compile que sous Windows (voir src/CMakeLists.txt).
class OverlayWindowWin32 : public OverlayWindow
{
    Q_OBJECT
public:
    explicit OverlayWindowWin32(QWidget *parent = nullptr);

    void setTopMost(bool enable) override;
    void setClickThrough(bool enable) override;

protected:
    void showEvent(QShowEvent *event) override;
};
