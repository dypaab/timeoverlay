#include "SettingsDialog.h"
#include "../core/Clock.h"
#include "../utils/Autostart.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Paramètres"));
    resize(560, 520);

    auto* root = new QVBoxLayout(this);

    // --- Dossier de sortie ---
    auto* outputBox = new QGroupBox(tr("Dossier de sortie"), this);
    auto* outputLayout = new QHBoxLayout(outputBox);
    m_outputDir = new QLineEdit(outputBox);
    auto* btnBrowse = new QPushButton(tr("Parcourir..."), outputBox);
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Dossier de sortie"), m_outputDir->text());
        if (!dir.isEmpty()) m_outputDir->setText(dir);
    });
    outputLayout->addWidget(m_outputDir);
    outputLayout->addWidget(btnBrowse);
    root->addWidget(outputBox);

    auto* hint = new QLabel(
        tr("C'est ici que sont écrits heure.txt, countdown.txt, countup.txt et les autres "
           "fichiers à pointer depuis les sources Texte d'OBS."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #777777;");
    root->addWidget(hint);

    // --- Formats ---
    auto* formatBox = new QGroupBox(tr("Formats d'affichage"), this);
    auto* formatLayout = new QFormLayout(formatBox);
    m_timeFormat = new QLineEdit(formatBox);
    m_dateFormat = new QLineEdit(formatBox);
    formatLayout->addRow(tr("Heure :"), m_timeFormat);
    formatLayout->addRow(tr("Date :"), m_dateFormat);
    auto* formatHint = new QLabel(
        tr("Exemples : HH:mm:ss, HH'h'mm, dddd d MMMM yyyy"), formatBox);
    formatHint->setStyleSheet("color: #777777;");
    formatLayout->addRow(QString(), formatHint);
    root->addWidget(formatBox);

    // --- Seuils et couleurs ---
    auto* colorBox = new QGroupBox(tr("Alerte visuelle"), this);
    auto* colorLayout = new QFormLayout(colorBox);

    m_warningThreshold = new QSpinBox(colorBox);
    m_warningThreshold->setRange(0, 3600);
    m_warningThreshold->setSuffix(tr(" s"));
    m_criticalThreshold = new QSpinBox(colorBox);
    m_criticalThreshold->setRange(0, 3600);
    m_criticalThreshold->setSuffix(tr(" s"));

    colorLayout->addRow(tr("Passer à l'orange à :"), m_warningThreshold);
    colorLayout->addRow(tr("Passer au rouge à :"), m_criticalThreshold);

    m_btnNormal   = makeColorButton(tr("Couleur normale"), &m_normal);
    m_btnWarning  = makeColorButton(tr("Couleur d'avertissement"), &m_warning);
    m_btnCritical = makeColorButton(tr("Couleur critique"), &m_critical);
    m_btnOvertime = makeColorButton(tr("Couleur de dépassement"), &m_overtime);

    colorLayout->addRow(m_btnNormal);
    colorLayout->addRow(m_btnWarning);
    colorLayout->addRow(m_btnCritical);
    colorLayout->addRow(m_btnOvertime);
    root->addWidget(colorBox);

    // --- Alarme et deroulement ---
    auto* alarmBox = new QGroupBox(tr("Fin de phase"), this);
    auto* alarmLayout = new QFormLayout(alarmBox);

    m_alarmEnabled = new QCheckBox(tr("Émettre un son en fin de phase"), alarmBox);
    alarmLayout->addRow(m_alarmEnabled);

    auto* soundRow = new QHBoxLayout();
    m_alarmSound = new QLineEdit(alarmBox);
    m_alarmSound->setPlaceholderText(tr("Vide = bip système"));
    auto* btnSound = new QPushButton(tr("Choisir..."), alarmBox);
    connect(btnSound, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Son d'alarme"), QDir::homePath(),
            tr("Fichiers audio (*.wav *.mp3 *.ogg *.flac);;Tous les fichiers (*)"));
        if (!file.isEmpty()) m_alarmSound->setText(file);
    });
    soundRow->addWidget(m_alarmSound);
    soundRow->addWidget(btnSound);
    alarmLayout->addRow(tr("Son :"), soundRow);

    m_autoAdvance = new QCheckBox(
        tr("Enchaîner automatiquement sur la phase suivante"), alarmBox);
    m_autoAdvance->setToolTip(
        tr("Désactivé par défaut : pendant un culte, c'est l'opérateur qui décide "
           "du passage à la suite."));
    alarmLayout->addRow(m_autoAdvance);
    root->addWidget(alarmBox);

    // --- Demarrage ---
    m_autostart = new QCheckBox(
        tr("Lancer TimeOverlay au démarrage de l'ordinateur"), this);
    m_autostart->setToolTip(
        tr("L'application affiche l'heure et le décompte avant le culte : elle doit "
           "déjà être ouverte quand vous arrivez à la régie."));
    // Masquee plutot que grisee la ou le mecanisme n'existe pas : une case a
    // cocher sans effet est pire qu'une case absente.
    m_autostart->setVisible(Autostart::isSupported());
    root->addWidget(m_autostart);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QPushButton* SettingsDialog::makeColorButton(const QString& label, QColor* target)
{
    auto* button = new QPushButton(label, this);
    connect(button, &QPushButton::clicked, this, [this, button, target]() {
        const QColor chosen = QColorDialog::getColor(*target, this, tr("Choisir une couleur"));
        if (chosen.isValid()) {
            *target = chosen;
            updateColorButton(button, chosen);
        }
    });
    updateColorButton(button, *target);
    return button;
}

void SettingsDialog::updateColorButton(QPushButton* button, const QColor& color)
{
    // Texte noir ou blanc selon la luminosite du fond, pour rester lisible.
    const QString textColor = color.lightness() > 140 ? "#000000" : "#ffffff";
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2; padding: 6px;")
                              .arg(color.name(), textColor));
}

void SettingsDialog::setOutputDir(const QString& dir)      { m_outputDir->setText(dir); }
void SettingsDialog::setTimeFormat(const QString& format)  { m_timeFormat->setText(format); }
void SettingsDialog::setDateFormat(const QString& format)  { m_dateFormat->setText(format); }
void SettingsDialog::setAlarmEnabled(bool enabled)         { m_alarmEnabled->setChecked(enabled); }
void SettingsDialog::setAlarmSound(const QString& path)    { m_alarmSound->setText(path); }
void SettingsDialog::setAutoAdvance(bool enabled)          { m_autoAdvance->setChecked(enabled); }
void SettingsDialog::setAutostart(bool enabled)            { m_autostart->setChecked(enabled); }

void SettingsDialog::setWarningThreshold(int seconds)  { m_warningThreshold->setValue(seconds); }
void SettingsDialog::setCriticalThreshold(int seconds) { m_criticalThreshold->setValue(seconds); }

void SettingsDialog::setColors(QColor normal, QColor warning, QColor critical, QColor overtime)
{
    if (normal.isValid())   { m_normal = normal;     updateColorButton(m_btnNormal, m_normal); }
    if (warning.isValid())  { m_warning = warning;   updateColorButton(m_btnWarning, m_warning); }
    if (critical.isValid()) { m_critical = critical; updateColorButton(m_btnCritical, m_critical); }
    if (overtime.isValid()) { m_overtime = overtime; updateColorButton(m_btnOvertime, m_overtime); }
}

QString SettingsDialog::outputDir() const   { return m_outputDir->text().trimmed(); }
QString SettingsDialog::alarmSound() const  { return m_alarmSound->text().trimmed(); }
bool SettingsDialog::alarmEnabled() const   { return m_alarmEnabled->isChecked(); }
bool SettingsDialog::autoAdvance() const    { return m_autoAdvance->isChecked(); }
bool SettingsDialog::autostart() const      { return m_autostart->isChecked(); }
int SettingsDialog::warningThreshold() const  { return m_warningThreshold->value(); }
int SettingsDialog::criticalThreshold() const { return m_criticalThreshold->value(); }

QString SettingsDialog::timeFormat() const
{
    const QString text = m_timeFormat->text().trimmed();
    return text.isEmpty() ? Clock::defaultTimeFormat() : text;
}

QString SettingsDialog::dateFormat() const
{
    const QString text = m_dateFormat->text().trimmed();
    return text.isEmpty() ? Clock::defaultDateFormat() : text;
}
