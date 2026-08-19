#pragma once
#include <QDialog>
#include "../core/Profile.h"

class QTableWidget;
class QLineEdit;
class QLabel;
class QTimeEdit;
class QCheckBox;

// Editeur du programme d'un culte.
//
// Sans cet ecran, la seule facon de definir un deroule etait d'ecrire un
// fichier JSON a la main -- inutilisable pour la personne qui prepare le
// culte le samedi soir.
class ProgrammeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProgrammeDialog(const Profile& profile, QWidget *parent = nullptr);

    // Programme resultant, valide seulement si exec() a renvoye Accepted.
    Profile profile() const { return m_profile; }

private slots:
    void onAddPhase();
    void onRemovePhase();
    void onMoveUp();
    void onMoveDown();
    void onAccept();

private:
    void fillTable(const Profile& profile);
    void updateTotal();
    void moveRow(int from, int to);

    // Lit la table et renvoie false si une ligne est invalide, en decrivant
    // le probleme dans errorMessage.
    bool collect(Profile* out, QString* errorMessage) const;

    Profile m_profile;
    QLineEdit* m_nameEdit = nullptr;
    QCheckBox* m_hasStartTime = nullptr;
    QTimeEdit* m_startTime = nullptr;
    QCheckBox* m_autoStart = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_totalLabel = nullptr;

    enum Column { ColName = 0, ColDuration, ColOvertime, ColumnCount };
};
