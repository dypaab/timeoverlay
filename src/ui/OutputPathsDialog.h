#pragma once
#include <QDialog>
#include <QStringList>

class QTableWidget;
class QLabel;

// Liste les fichiers ecrits par l'application, avec leur chemin complet.
//
// Configurer OBS demande de coller un chemin exact dans chaque source Texte.
// Sans cet ecran, il fallait aller le chercher dans l'explorateur de fichiers
// et le retaper -- source d'erreurs, et pas seulement de perte de temps.
//
// Les chemins sont presentes comme des liens : un clic copie, un clic sur
// l'icone de dossier ouvre l'emplacement. Passer par une selection de ligne
// puis un bouton etait exact mais peu evident.
class OutputPathsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OutputPathsDialog(const QString& baseDir, QWidget *parent = nullptr);

private slots:
    void copySelectedPath();
    void copyAllPaths();
    void openFolder();

private:
    void fillTable();
    void copyPathAt(int row);
    void revealPathAt(int row);
    void announce(const QString& message);

    QString m_baseDir;
    QTableWidget* m_table = nullptr;
    QLabel* m_feedback = nullptr;
    QStringList m_paths;   // indexe par ligne du tableau

    enum Column { ColFile = 0, ColRole, ColPath, ColReveal, ColumnCount };
};
