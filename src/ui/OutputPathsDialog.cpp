#include "OutputPathsDialog.h"
#include "../core/OutputEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

namespace {

struct Sortie {
    QString cle;
    QString role;
};

// Ordre pense pour la mise en place dans OBS : d'abord ce qu'on affiche
// presque toujours, ensuite les complements.
QVector<Sortie> sorties()
{
    return {
        { OutputEngine::Heure,
          QObject::tr("Heure courante — 10:42:07") },
        { OutputEngine::Countdown,
          QObject::tr("Temps restant. Se fige à 00:00:00, ne devient jamais négatif.") },
        { OutputEngine::Depassement,
          QObject::tr("Temps en trop — +00:01:22. VIDE tant qu'on ne dépasse pas, "
                      "donc invisible dans OBS jusqu'au dépassement.") },
        { OutputEngine::Countup,
          QObject::tr("Temps écoulé depuis le début. Continue de monter après zéro, "
                      "sans s'arrêter.") },
        { OutputEngine::AvantDebut,
          QObject::tr("Décompte avant le début du culte. Se vide au démarrage.") },
        { OutputEngine::Phase,
          QObject::tr("Nom de la phase en cours — Prédication") },
        { OutputEngine::PhaseSuivante,
          QObject::tr("Nom de la phase suivante — Annonces") },
        { OutputEngine::Statut,
          QObject::tr("EN_COURS, PAUSE, DEPASSEMENT, TERMINE, ARRETE") },
        { OutputEngine::Message,
          QObject::tr("Message tapé par l'opérateur pendant le direct.") },
        { OutputEngine::Annonce,
          QObject::tr("Annonce en cours de rotation.") },
        { OutputEngine::Date,
          QObject::tr("Date courante — lundi 17 août 2026") },
    };
}

} // namespace

OutputPathsDialog::OutputPathsDialog(const QString& baseDir, QWidget *parent)
    : QDialog(parent), m_baseDir(baseDir)
{
    setWindowTitle(tr("Fichiers à pointer depuis OBS"));
    // Large : le chemin complet est la raison d'etre de cette fenetre, le
    // tronquer la viderait de son interet.
    resize(1260, 640);

    auto* root = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("Dans OBS : ajoutez une source <b>Texte (GDI+)</b>, cochez "
           "<b>Lire à partir d'un fichier</b>, et collez le chemin.<br>"
           "<b>Cliquez sur un chemin pour le copier</b>, ou sur 📁 pour ouvrir "
           "son emplacement dans l'explorateur."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* dirLabel = new QLabel(tr("Dossier : <code>%1</code>")
                                    .arg(QDir::toNativeSeparators(m_baseDir).toHtmlEscaped()), this);
    dirLabel->setWordWrap(true);
    dirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dirLabel->setStyleSheet("color: #555555;");
    root->addWidget(dirLabel);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels(
        { tr("Fichier"), tr("Ce qu'il contient"), tr("Chemin complet"), QString() });
    m_table->horizontalHeader()->setSectionResizeMode(ColFile, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColRole, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColPath, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColReveal, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(true);
    // Selection en teinte claire plutot qu'en bleu plein : les chemins sont
    // des liens bleus, et sur une selection bleue soutenue ils devenaient
    // illisibles sur la ligne courante.
    m_table->setStyleSheet(
        QStringLiteral("QTableWidget::item:selected { background: #dbeafe; color: #111827; }"));
    root->addWidget(m_table);

    fillTable();
    m_table->resizeRowsToContents();
    if (m_table->rowCount() > 0) m_table->selectRow(0);

    m_feedback = new QLabel(tr("Cliquez sur un chemin pour le copier."), this);
    m_feedback->setStyleSheet("color: #555555; padding: 4px 0;");
    root->addWidget(m_feedback);

    auto* buttons = new QHBoxLayout();
    auto* btnCopy = new QPushButton(tr("Copier le chemin sélectionné"), this);
    auto* btnCopyAll = new QPushButton(tr("Copier tous les chemins"), this);
    auto* btnOpen = new QPushButton(tr("Ouvrir le dossier"), this);
    connect(btnCopy, &QPushButton::clicked, this, &OutputPathsDialog::copySelectedPath);
    connect(btnCopyAll, &QPushButton::clicked, this, &OutputPathsDialog::copyAllPaths);
    connect(btnOpen, &QPushButton::clicked, this, &OutputPathsDialog::openFolder);
    buttons->addWidget(btnCopy);
    buttons->addWidget(btnCopyAll);
    buttons->addWidget(btnOpen);
    buttons->addStretch();
    root->addLayout(buttons);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

void OutputPathsDialog::fillTable()
{
    const QDir dir(m_baseDir);
    m_paths.clear();

    for (const Sortie& sortie : sorties()) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        const QString nomFichier = sortie.cle + QStringLiteral(".txt");
        // Chemin natif : sous Windows OBS attend des antislashs.
        const QString chemin = QDir::toNativeSeparators(dir.filePath(nomFichier));
        m_paths.append(chemin);

        auto* fichier = new QTableWidgetItem(nomFichier);
        fichier->setFlags(fichier->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColFile, fichier);

        auto* role = new QTableWidgetItem(sortie.role);
        role->setFlags(role->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColRole, role);

        // Le chemin est un vrai lien : curseur en main, souligne, et un clic
        // suffit. L'href porte le numero de ligne plutot que le chemin, pour
        // qu'aucun caractere du chemin n'ait a etre echappe dans une URL.
        auto* lien = new QLabel(m_table);
        lien->setText(QStringLiteral("<a href=\"%1\" style=\"color:#1d4ed8;\">%2</a>")
                          .arg(row)
                          .arg(chemin.toHtmlEscaped()));
        lien->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
        lien->setCursor(Qt::PointingHandCursor);
        lien->setToolTip(tr("Cliquez pour copier ce chemin"));
        lien->setContentsMargins(6, 2, 6, 2);
        connect(lien, &QLabel::linkActivated, this, [this](const QString& href) {
            copyPathAt(href.toInt());
        });
        m_table->setCellWidget(row, ColPath, lien);

        auto* dossier = new QLabel(m_table);
        dossier->setText(QStringLiteral("<a href=\"%1\" style=\"text-decoration:none;\">📁</a>")
                             .arg(row));
        dossier->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
        dossier->setCursor(Qt::PointingHandCursor);
        dossier->setToolTip(tr("Ouvrir l'emplacement de ce fichier"));
        dossier->setAlignment(Qt::AlignCenter);
        dossier->setContentsMargins(8, 2, 8, 2);
        connect(dossier, &QLabel::linkActivated, this, [this](const QString& href) {
            revealPathAt(href.toInt());
        });
        m_table->setCellWidget(row, ColReveal, dossier);
    }
}

void OutputPathsDialog::announce(const QString& message)
{
    m_feedback->setText(message);
    m_feedback->setStyleSheet("color: #15803d; font-weight: bold; padding: 4px 0;");

    // Le message revient a son etat neutre : sans cela, un "copie" fige a
    // l'ecran laisse croire que l'action vient d'avoir lieu.
    QTimer::singleShot(4000, this, [this]() {
        m_feedback->setText(tr("Cliquez sur un chemin pour le copier."));
        m_feedback->setStyleSheet("color: #555555; padding: 4px 0;");
    });
}

void OutputPathsDialog::copyPathAt(int row)
{
    if (row < 0 || row >= m_paths.size()) return;

    QApplication::clipboard()->setText(m_paths.at(row));
    m_table->selectRow(row);

    const QTableWidgetItem* fichier = m_table->item(row, ColFile);
    announce(tr("Chemin de %1 copié — collez-le dans OBS")
                 .arg(fichier ? fichier->text() : QString()));
}

void OutputPathsDialog::revealPathAt(int row)
{
    if (row < 0 || row >= m_paths.size()) return;
    const QString chemin = m_paths.at(row);

#ifdef Q_OS_WIN
    // /select ouvre l'explorateur avec le fichier deja mis en evidence.
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            { QStringLiteral("/select,") + chemin });
#else
    // Ailleurs on ouvre le dossier : la selection d'un fichier precis n'est
    // pas portable d'un gestionnaire de fichiers a l'autre.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(chemin).absolutePath()));
#endif
    m_table->selectRow(row);
}

void OutputPathsDialog::copySelectedPath()
{
    copyPathAt(m_table->currentRow());
}

void OutputPathsDialog::copyAllPaths()
{
    QStringList lignes;
    for (int row = 0; row < m_paths.size(); ++row) {
        const QTableWidgetItem* fichier = m_table->item(row, ColFile);
        if (!fichier) continue;
        lignes << QStringLiteral("%1\t%2").arg(fichier->text(), m_paths.at(row));
    }
    QApplication::clipboard()->setText(lignes.join(QLatin1Char('\n')));
    announce(tr("%1 chemins copiés").arg(lignes.size()));
}

void OutputPathsDialog::openFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_baseDir));
}
