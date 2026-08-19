#include "ProgrammeLibraryDialog.h"
#include "../core/Timer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QColor>
#include <QLocale>

namespace {

// Le role qui porte le chemin du fichier : la colonne affiche le nom du
// programme, pas son emplacement sur le disque.
constexpr int kPathRole = Qt::UserRole;

QString formatSigned(int seconds)
{
    if (seconds == 0) return QStringLiteral("—");
    const QString text = Timer::format(seconds < 0 ? -seconds : seconds);
    return (seconds < 0 ? QStringLiteral("- ") : QStringLiteral("+ ")) + text;
}

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

// QDateTime::toString() avec un format ecrit les noms de jours et de mois en
// anglais, quelle que soit la langue du systeme : « Wednesday 19 August ».
// Il faut passer par la locale pour obtenir « mercredi 19 août ».
QString localDate(const QDateTime& moment, const QString& format)
{
    return QLocale::system().toString(moment, format);
}

} // namespace

ProgrammeLibraryDialog::ProgrammeLibraryDialog(ProgrammeLibrary* library, QWidget *parent)
    : QDialog(parent), m_library(library)
{
    setWindowTitle(tr("Mes programmes"));
    resize(760, 560);

    auto* root = new QVBoxLayout(this);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("Programme"), tr("Phases"), tr("Durée prévue"), tr("Dernière utilisation")
    });
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    // Les trois autres colonnes se dimensionnent sur leur contenu : a largeur
    // egale, « Dernière utilisation » etait tronque en « ernière utilisatio ».
    for (int column = ColPhases; column < ColumnCount; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ProgrammeLibraryDialog::onSelectionChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { onOpen(); });
    root->addWidget(m_table, 3);

    m_emptyHint = new QLabel(
        tr("Aucun programme enregistré pour l'instant. Les programmes créés avec "
           "« Programme ▸ Nouveau » viennent se ranger ici automatiquement."), this);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setStyleSheet("color: #777777;");
    root->addWidget(m_emptyHint);

    m_historyTitle = new QLabel(this);
    m_historyTitle->setStyleSheet("font-weight: bold; margin-top: 6px;");
    root->addWidget(m_historyTitle);

    m_history = new QTableWidget(0, 5, this);
    m_history->setHorizontalHeaderLabels({
        tr("Date"), tr("Phases"), tr("Prévu"), tr("Réel"), tr("Écart")
    });
    m_history->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        m_history->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionMode(QAbstractItemView::NoSelection);
    m_history->setAlternatingRowColors(true);
    m_history->verticalHeader()->setVisible(false);
    root->addWidget(m_history, 2);

    auto* buttons = new QHBoxLayout();
    m_btnOpen = new QPushButton(tr("Ouvrir"), this);
    m_btnOpen->setDefault(true);
    m_btnDelete = new QPushButton(tr("Supprimer"), this);
    auto* btnImport = new QPushButton(tr("Ouvrir un fichier..."), this);
    auto* btnClose = new QPushButton(tr("Fermer"), this);

    connect(m_btnOpen, &QPushButton::clicked, this, &ProgrammeLibraryDialog::onOpen);
    connect(m_btnDelete, &QPushButton::clicked, this, &ProgrammeLibraryDialog::onDelete);
    connect(btnImport, &QPushButton::clicked, this, &ProgrammeLibraryDialog::onImport);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);

    buttons->addWidget(m_btnOpen);
    buttons->addWidget(m_btnDelete);
    buttons->addStretch();
    buttons->addWidget(btnImport);
    buttons->addWidget(btnClose);
    root->addLayout(buttons);

    reload();
}

void ProgrammeLibraryDialog::reload()
{
    m_table->setRowCount(0);

    const QVector<ProgrammeLibrary::Entry> entries = m_library->entries();
    for (const ProgrammeLibrary::Entry& entry : entries) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* nameItem = readOnlyItem(entry.name);
        nameItem->setData(kPathRole, entry.path);
        m_table->setItem(row, ColName, nameItem);

        m_table->setItem(row, ColPhases, readOnlyItem(QString::number(entry.phaseCount)));
        m_table->setItem(row, ColDuration, readOnlyItem(Timer::format(entry.totalSeconds)));
        m_table->setItem(row, ColLastUsed,
                         readOnlyItem(entry.lastUsedAt.isValid()
                                          ? localDate(entry.lastUsedAt, QStringLiteral("dd/MM/yyyy HH:mm"))
                                          : tr("jamais")));
    }

    m_emptyHint->setVisible(entries.isEmpty());
    if (!entries.isEmpty()) m_table->selectRow(0);
    onSelectionChanged();
}

QString ProgrammeLibraryDialog::currentPath() const
{
    const QTableWidgetItem* item = m_table->item(m_table->currentRow(), ColName);
    return item ? item->data(kPathRole).toString() : QString();
}

QString ProgrammeLibraryDialog::currentName() const
{
    const QTableWidgetItem* item = m_table->item(m_table->currentRow(), ColName);
    return item ? item->text() : QString();
}

void ProgrammeLibraryDialog::onSelectionChanged()
{
    const QString name = currentName();
    const bool hasSelection = !name.isEmpty();

    m_btnOpen->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);

    m_history->setRowCount(0);
    if (!hasSelection) {
        m_historyTitle->clear();
        m_history->hide();
        return;
    }

    const QVector<ProgrammeLibrary::Session> sessions = m_library->sessions(name);
    if (sessions.isEmpty()) {
        m_historyTitle->setText(tr("« %1 » n'a encore jamais été déroulé.").arg(name));
        m_history->hide();
        return;
    }

    m_historyTitle->setText(tr("Historique de « %1 »").arg(name));
    m_history->show();

    // Le culte le plus recent en premier : c'est celui auquel on se compare.
    for (int i = sessions.size() - 1; i >= 0; --i) {
        const ProgrammeLibrary::Session& session = sessions.at(i);
        const int row = m_history->rowCount();
        m_history->insertRow(row);

        m_history->setItem(row, 0, readOnlyItem(
            localDate(session.startedAt, QStringLiteral("dddd d MMMM yyyy, HH:mm"))));
        m_history->setItem(row, 1, readOnlyItem(QString::number(session.phases.size())));
        m_history->setItem(row, 2, readOnlyItem(Timer::format(session.plannedSeconds)));
        m_history->setItem(row, 3, readOnlyItem(Timer::format(session.actualSeconds)));

        auto* delta = readOnlyItem(formatSigned(session.deltaSeconds()));
        if (session.deltaSeconds() > 0) delta->setForeground(QColor("#dc2626"));
        m_history->setItem(row, 4, delta);
    }
}

void ProgrammeLibraryDialog::onOpen()
{
    const QString path = currentPath();
    if (path.isEmpty()) return;
    m_selectedPath = path;
    accept();
}

void ProgrammeLibraryDialog::onDelete()
{
    const QString path = currentPath();
    const QString name = currentName();
    if (path.isEmpty()) return;

    const auto answer = QMessageBox::question(
        this, tr("Supprimer le programme"),
        tr("Supprimer « %1 » de la bibliothèque ?\n\n"
           "Son historique sera supprimé également. Cette action est définitive.")
            .arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    QString error;
    if (!m_library->remove(path, &error)) {
        QMessageBox::warning(this, tr("Suppression impossible"), error);
        return;
    }
    reload();
}

void ProgrammeLibraryDialog::onImport()
{
    // Un programme peut arriver par cle USB ou par courriel : on doit pouvoir
    // l'ouvrir sans qu'il soit d'abord range dans la bibliotheque.
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Ouvrir un programme"), QDir::homePath(),
        tr("Programmes (*.timerproject *.json);;Tous les fichiers (*)"));
    if (path.isEmpty()) return;

    m_selectedPath = path;
    accept();
}
