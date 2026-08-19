#include "ProgrammeDialog.h"
#include "../core/Timer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QTimeEdit>


ProgrammeDialog::ProgrammeDialog(const Profile& profile, QWidget *parent)
    : QDialog(parent), m_profile(profile)
{
    setWindowTitle(tr("Programme du culte"));
    resize(820, 560);

    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    m_nameEdit = new QLineEdit(profile.name, this);
    m_nameEdit->setPlaceholderText(tr("Culte du dimanche matin"));
    form->addRow(tr("Nom du programme :"), m_nameEdit);

    // --- Heure de debut ---
    auto* startRow = new QHBoxLayout();
    m_hasStartTime = new QCheckBox(tr("Le culte commence à"), this);
    m_startTime = new QTimeEdit(this);
    m_startTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_startTime->setTime(QTime(10, 0));
    m_autoStart = new QCheckBox(tr("démarrer la première phase automatiquement"), this);

    const QTime existing = profile.startTimeAsTime();
    m_hasStartTime->setChecked(existing.isValid());
    if (existing.isValid()) m_startTime->setTime(existing);
    m_autoStart->setChecked(profile.autoStart);

    const auto syncEnabled = [this]() {
        const bool on = m_hasStartTime->isChecked();
        m_startTime->setEnabled(on);
        m_autoStart->setEnabled(on);
    };
    connect(m_hasStartTime, &QCheckBox::toggled, this, syncEnabled);
    syncEnabled();

    startRow->addWidget(m_hasStartTime);
    startRow->addWidget(m_startTime);
    startRow->addWidget(m_autoStart);
    startRow->addStretch();
    form->addRow(startRow);

    auto* startHint = new QLabel(
        tr("Le décompte avant le début est écrit dans avant_debut.txt, à afficher sur "
           "l'écran d'accueil. Il se vide dès que le culte démarre. Seule la première "
           "phase démarre à l'heure : les suivantes se lancent à la main, pour qu'une "
           "phase qui déborde ne soit jamais coupée."), this);
    startHint->setWordWrap(true);
    startHint->setStyleSheet("color: #777777;");
    form->addRow(startHint);

    root->addLayout(form);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("Phase"), tr("Durée"), tr("Dépassement")
    });
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table);

    auto* hint = new QLabel(
        tr("Durée : \"25\" pour 25 minutes, \"05:30\" pour 5 min 30 s, \"01:15:00\" pour "
           "1 h 15. Dépassement décoché : la phase s'arrête net au lieu de compter le "
           "temps en trop."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #777777;");
    root->addWidget(hint);

    auto* buttonRow = new QHBoxLayout();
    auto* btnAdd = new QPushButton(tr("Ajouter"), this);
    auto* btnRemove = new QPushButton(tr("Supprimer"), this);
    auto* btnUp = new QPushButton(tr("Monter"), this);
    auto* btnDown = new QPushButton(tr("Descendre"), this);
    connect(btnAdd, &QPushButton::clicked, this, &ProgrammeDialog::onAddPhase);
    connect(btnRemove, &QPushButton::clicked, this, &ProgrammeDialog::onRemovePhase);
    connect(btnUp, &QPushButton::clicked, this, &ProgrammeDialog::onMoveUp);
    connect(btnDown, &QPushButton::clicked, this, &ProgrammeDialog::onMoveDown);
    buttonRow->addWidget(btnAdd);
    buttonRow->addWidget(btnRemove);
    buttonRow->addStretch();
    buttonRow->addWidget(btnUp);
    buttonRow->addWidget(btnDown);
    root->addLayout(buttonRow);

    m_totalLabel = new QLabel(this);
    m_totalLabel->setStyleSheet("font-weight: bold;");
    root->addWidget(m_totalLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProgrammeDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    fillTable(profile);
    connect(m_table, &QTableWidget::itemChanged, this, [this]() { updateTotal(); });
    updateTotal();
}

void ProgrammeDialog::fillTable(const Profile& profile)
{
    m_table->setRowCount(0);
    for (const Phase& phase : profile.phases) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, ColName, new QTableWidgetItem(phase.name));
        m_table->setItem(row, ColDuration,
                         new QTableWidgetItem(Timer::format(phase.durationSeconds)));

        auto* overtimeItem = new QTableWidgetItem();
        overtimeItem->setFlags((overtimeItem->flags() | Qt::ItemIsUserCheckable)
                               & ~Qt::ItemIsEditable);
        overtimeItem->setCheckState(phase.overtimeEnabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, ColOvertime, overtimeItem);
    }
}

void ProgrammeDialog::onAddPhase()
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, ColName, new QTableWidgetItem(tr("Nouvelle phase")));
    m_table->setItem(row, ColDuration, new QTableWidgetItem(QStringLiteral("00:10:00")));

    auto* overtimeItem = new QTableWidgetItem();
    overtimeItem->setFlags((overtimeItem->flags() | Qt::ItemIsUserCheckable)
                           & ~Qt::ItemIsEditable);
    overtimeItem->setCheckState(Qt::Checked);
    m_table->setItem(row, ColOvertime, overtimeItem);

    m_table->setCurrentCell(row, ColName);
    updateTotal();
}

void ProgrammeDialog::onRemovePhase()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    m_table->removeRow(row);
    updateTotal();
}

void ProgrammeDialog::moveRow(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_table->rowCount() || to >= m_table->rowCount()) {
        return;
    }

    // On deplace les items un par un : QTableWidget n'offre pas de
    // deplacement de ligne, et recreer la table perdrait les etats coches.
    for (int col = 0; col < ColumnCount; ++col) {
        QTableWidgetItem* a = m_table->takeItem(from, col);
        QTableWidgetItem* b = m_table->takeItem(to, col);
        m_table->setItem(from, col, b);
        m_table->setItem(to, col, a);
    }
    m_table->setCurrentCell(to, ColName);
}

void ProgrammeDialog::onMoveUp()
{
    const int row = m_table->currentRow();
    if (row > 0) moveRow(row, row - 1);
}

void ProgrammeDialog::onMoveDown()
{
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_table->rowCount() - 1) moveRow(row, row + 1);
}

bool ProgrammeDialog::collect(Profile* out, QString* errorMessage) const
{
    Profile result;
    result.name = m_nameEdit->text().trimmed();
    if (result.name.isEmpty()) result.name = tr("Programme sans nom");

    if (m_hasStartTime->isChecked()) {
        result.startTime = m_startTime->time().toString(QStringLiteral("HH:mm"));
        result.autoStart = m_autoStart->isChecked();
    } else {
        result.startTime.clear();
        result.autoStart = false;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        Phase phase;

        const QTableWidgetItem* nameItem = m_table->item(row, ColName);
        phase.name = nameItem ? nameItem->text().trimmed() : QString();
        if (phase.name.isEmpty()) {
            *errorMessage = tr("La phase %1 n'a pas de nom.").arg(row + 1);
            return false;
        }

        const QTableWidgetItem* durationItem = m_table->item(row, ColDuration);
        const QString durationText = durationItem ? durationItem->text() : QString();
        if (!Timer::parseDuration(durationText, &phase.durationSeconds)) {
            *errorMessage = tr("Durée invalide pour la phase \"%1\" : \"%2\".")
                                .arg(phase.name, durationText);
            return false;
        }

        const QTableWidgetItem* overtimeItem = m_table->item(row, ColOvertime);
        phase.overtimeEnabled = !overtimeItem || overtimeItem->checkState() == Qt::Checked;

        result.phases.append(phase);
    }

    *out = result;
    return true;
}

void ProgrammeDialog::updateTotal()
{
    Profile draft;
    QString ignored;
    if (!collect(&draft, &ignored)) {
        // Saisie en cours : on n'affiche pas d'erreur, seulement un total
        // indisponible.
        m_totalLabel->setText(tr("Durée totale : --"));
        return;
    }
    m_totalLabel->setText(tr("Durée totale : %1  (%2 phases)")
                              .arg(Timer::format(draft.totalDuration()))
                              .arg(draft.phases.size()));
}

void ProgrammeDialog::onAccept()
{
    Profile result;
    QString error;
    if (!collect(&result, &error)) {
        QMessageBox::warning(this, tr("Programme incomplet"), error);
        return;
    }
    if (result.phases.isEmpty()) {
        QMessageBox::warning(this, tr("Programme vide"),
                             tr("Ajoutez au moins une phase."));
        return;
    }

    m_profile = result;
    accept();
}
