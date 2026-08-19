#pragma once
#include <QDialog>
#include "../core/ProgrammeLibrary.h"

class QTableWidget;
class QLabel;
class QPushButton;

// Liste des programmes enregistres, avec l'historique de celui qu'on
// selectionne.
//
// Les deux tableaux sont dans la meme fenetre a dessein : la question qu'on se
// pose en preparant un culte est « lequel je reprends, et combien de temps il
// a vraiment pris la derniere fois ». Deux fenetres separeraient la reponse en
// deux.
class ProgrammeLibraryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProgrammeLibraryDialog(ProgrammeLibrary* library, QWidget *parent = nullptr);

    // Programme a ouvrir, renseigne seulement si exec() a renvoye Accepted.
    QString selectedPath() const { return m_selectedPath; }

private slots:
    void onOpen();
    void onDelete();
    void onImport();
    void onSelectionChanged();

private:
    void reload();
    QString currentPath() const;
    QString currentName() const;

    ProgrammeLibrary* m_library = nullptr;
    QString m_selectedPath;

    QTableWidget* m_table = nullptr;
    QTableWidget* m_history = nullptr;
    QLabel* m_historyTitle = nullptr;
    QLabel* m_emptyHint = nullptr;
    QPushButton* m_btnOpen = nullptr;
    QPushButton* m_btnDelete = nullptr;

    enum Column { ColName = 0, ColPhases, ColDuration, ColLastUsed, ColumnCount };
};
