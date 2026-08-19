#include "MessagePanel.h"
#include "../core/MessageCenter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QShortcut>
#include <QSignalBlocker>

MessagePanel::MessagePanel(MessageCenter* center, QWidget *parent)
    : QWidget(parent), m_center(center)
{
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildMessageTab(), tr("Message"));
    tabs->addTab(buildAnnouncementTab(), tr("Annonces"));
    root->addWidget(tabs);

    if (m_center) {
        // L'apercu montre exactement ce qui part dans le fichier, defilement
        // compris : l'operateur voit ce que voit l'assemblee.
        connect(m_center, &MessageCenter::messageRendered, this, [this](const QString& text) {
            m_messagePreview->setText(text.isEmpty() ? tr("(aucun message affiché)") : text);
        });
        connect(m_center, &MessageCenter::announcementRendered, this, [this](const QString& text) {
            m_announcementPreview->setText(text.isEmpty() ? tr("(aucune annonce affichée)") : text);
        });
    }
}

// ---------------------------------------------------------- onglet message

QWidget* MessagePanel::buildMessageTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(tr("Message à afficher :"), page));
    m_messageEdit = new QPlainTextEdit(page);
    m_messageEdit->setPlaceholderText(
        tr("Un enfant vous attend à l'accueil.\n\n"
           "Ctrl+Entrée pour envoyer."));
    m_messageEdit->setMaximumHeight(110);
    layout->addWidget(m_messageEdit);

    auto* actions = new QHBoxLayout();
    auto* btnSend = new QPushButton(tr("Afficher"), page);
    btnSend->setStyleSheet("font-weight: bold; padding: 8px;");
    auto* btnClear = new QPushButton(tr("Effacer l'écran"), page);
    connect(btnSend, &QPushButton::clicked, this, &MessagePanel::onSendMessage);
    connect(btnClear, &QPushButton::clicked, this, &MessagePanel::onClearMessage);
    actions->addWidget(btnSend, 2);
    actions->addWidget(btnClear, 1);
    layout->addLayout(actions);

    // Ctrl+Entree : envoi sans lacher le clavier, utile en direct.
    auto* sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_messageEdit);
    connect(sendShortcut, &QShortcut::activated, this, &MessagePanel::onSendMessage);

    auto* previewBox = new QGroupBox(tr("Ce qui s'affiche à l'écran"), page);
    auto* previewLayout = new QVBoxLayout(previewBox);
    m_messagePreview = new QLabel(tr("(aucun message affiché)"), previewBox);
    m_messagePreview->setWordWrap(true);
    m_messagePreview->setStyleSheet(
        "font-family: monospace; font-size: 14px; background: #111111; "
        "color: #eeeeee; padding: 8px;");
    m_messagePreview->setMinimumHeight(60);
    m_messagePreview->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    previewLayout->addWidget(m_messagePreview);
    layout->addWidget(previewBox);

    // --- Modeles ---
    auto* templateBox = new QGroupBox(tr("Messages fréquents"), page);
    auto* templateLayout = new QVBoxLayout(templateBox);
    m_templateList = new QListWidget(templateBox);
    m_templateList->setMaximumHeight(110);
    connect(m_templateList, &QListWidget::itemDoubleClicked,
            this, &MessagePanel::onTemplateActivated);
    templateLayout->addWidget(m_templateList);

    auto* templateButtons = new QHBoxLayout();
    auto* btnSaveTemplate = new QPushButton(tr("Ajouter le message actuel"), templateBox);
    auto* btnRemoveTemplate = new QPushButton(tr("Retirer"), templateBox);
    connect(btnSaveTemplate, &QPushButton::clicked, this, &MessagePanel::onSaveTemplate);
    connect(btnRemoveTemplate, &QPushButton::clicked, this, &MessagePanel::onRemoveTemplate);
    templateButtons->addWidget(btnSaveTemplate);
    templateButtons->addWidget(btnRemoveTemplate);
    templateLayout->addLayout(templateButtons);

    auto* templateHint = new QLabel(tr("Double-cliquez sur un modèle pour le charger."), templateBox);
    templateHint->setStyleSheet("color: #777777;");
    templateLayout->addWidget(templateHint);
    layout->addWidget(templateBox);

    layout->addWidget(buildFormatBox(tr("Mise en forme du message"), &m_messageFormat,
                                     [this]() { onMessageFormatChanged(); }));
    layout->addStretch();
    return page;
}

// --------------------------------------------------------- onglet annonces

QWidget* MessagePanel::buildAnnouncementTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* hint = new QLabel(
        tr("Les annonces tournent automatiquement dans annonce.txt. "
           "Double-cliquez sur une ligne pour la modifier."), page);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #777777;");
    layout->addWidget(hint);

    m_announcementList = new QListWidget(page);
    m_announcementList->setAlternatingRowColors(true);
    connect(m_announcementList, &QListWidget::itemChanged,
            this, &MessagePanel::onAnnouncementsChanged);
    layout->addWidget(m_announcementList);

    auto* buttons = new QHBoxLayout();
    auto* btnAdd = new QPushButton(tr("Ajouter"), page);
    auto* btnRemove = new QPushButton(tr("Supprimer"), page);
    auto* btnUp = new QPushButton(tr("Monter"), page);
    auto* btnDown = new QPushButton(tr("Descendre"), page);
    connect(btnAdd, &QPushButton::clicked, this, &MessagePanel::onAddAnnouncement);
    connect(btnRemove, &QPushButton::clicked, this, &MessagePanel::onRemoveAnnouncement);
    connect(btnUp, &QPushButton::clicked, this, &MessagePanel::onMoveAnnouncementUp);
    connect(btnDown, &QPushButton::clicked, this, &MessagePanel::onMoveAnnouncementDown);
    buttons->addWidget(btnAdd);
    buttons->addWidget(btnRemove);
    buttons->addStretch();
    buttons->addWidget(btnUp);
    buttons->addWidget(btnDown);
    layout->addLayout(buttons);

    auto* rotationBox = new QGroupBox(tr("Rotation"), page);
    auto* rotationLayout = new QFormLayout(rotationBox);
    m_rotationEnabled = new QCheckBox(tr("Diffuser les annonces"), rotationBox);
    m_rotationSeconds = new QSpinBox(rotationBox);
    m_rotationSeconds->setRange(2, 3600);
    m_rotationSeconds->setValue(10);
    m_rotationSeconds->setSuffix(tr(" s"));

    connect(m_rotationEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_center) m_center->setRotationEnabled(checked);
        emit settingsChanged();
    });
    connect(m_rotationSeconds, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_center) m_center->setRotationSeconds(value);
        emit settingsChanged();
    });

    rotationLayout->addRow(m_rotationEnabled);
    rotationLayout->addRow(tr("Changer toutes les :"), m_rotationSeconds);

    auto* btnNext = new QPushButton(tr("Annonce suivante maintenant"), rotationBox);
    connect(btnNext, &QPushButton::clicked, this, [this]() {
        if (m_center) m_center->nextAnnouncement();
    });
    rotationLayout->addRow(btnNext);
    layout->addWidget(rotationBox);

    auto* previewBox = new QGroupBox(tr("Annonce à l'antenne"), page);
    auto* previewLayout = new QVBoxLayout(previewBox);
    m_announcementPreview = new QLabel(tr("(aucune annonce affichée)"), previewBox);
    m_announcementPreview->setWordWrap(true);
    m_announcementPreview->setStyleSheet(
        "font-family: monospace; font-size: 14px; background: #111111; "
        "color: #eeeeee; padding: 8px;");
    m_announcementPreview->setMinimumHeight(48);
    previewLayout->addWidget(m_announcementPreview);
    layout->addWidget(previewBox);

    layout->addWidget(buildFormatBox(tr("Mise en forme des annonces"), &m_announcementFormat,
                                     [this]() { onAnnouncementFormatChanged(); }));
    return page;
}

// ------------------------------------------------------------ mise en forme

QWidget* MessagePanel::buildFormatBox(const QString& title, FormatWidgets* widgets,
                                      std::function<void()> onChanged)
{
    auto* box = new QGroupBox(title, this);
    auto* layout = new QFormLayout(box);

    widgets->width = new QSpinBox(box);
    widgets->width->setRange(0, 500);
    widgets->width->setValue(0);
    widgets->width->setSpecialValueText(tr("aucune limite"));
    widgets->width->setSuffix(tr(" caractères"));
    widgets->width->setToolTip(
        tr("Largeur de l'affichage cible. Le texte revient à la ligne sans couper les mots. "
           "En mode défilement, c'est la largeur de la fenêtre qui défile."));

    widgets->lines = new QSpinBox(box);
    widgets->lines->setRange(0, 20);
    widgets->lines->setValue(0);
    widgets->lines->setSpecialValueText(tr("aucune limite"));
    widgets->lines->setSuffix(tr(" lignes"));

    widgets->overflow = new QComboBox(box);
    widgets->overflow->addItem(tr("Couper le texte trop long"),
                               int(TextFormat::Overflow::Truncate));
    widgets->overflow->addItem(tr("Faire défiler sur une ligne"),
                               int(TextFormat::Overflow::Scroll));

    widgets->uppercase = new QCheckBox(tr("Tout en majuscules"), box);

    layout->addRow(tr("Largeur :"), widgets->width);
    layout->addRow(tr("Hauteur :"), widgets->lines);
    layout->addRow(tr("Si trop long :"), widgets->overflow);
    layout->addRow(widgets->uppercase);

    connect(widgets->width, &QSpinBox::valueChanged, this, [onChanged](int) { onChanged(); });
    connect(widgets->lines, &QSpinBox::valueChanged, this, [onChanged](int) { onChanged(); });
    connect(widgets->overflow, &QComboBox::currentIndexChanged, this,
            [onChanged](int) { onChanged(); });
    connect(widgets->uppercase, &QCheckBox::toggled, this, [onChanged](bool) { onChanged(); });

    return box;
}

TextFormat MessagePanel::readFormat(const FormatWidgets& widgets) const
{
    TextFormat format;
    format.maxCharsPerLine = widgets.width->value();
    format.maxLines = widgets.lines->value();
    format.overflow = static_cast<TextFormat::Overflow>(
        widgets.overflow->currentData().toInt());
    format.uppercase = widgets.uppercase->isChecked();
    return format;
}

void MessagePanel::writeFormat(const FormatWidgets& widgets, const TextFormat& format)
{
    widgets.width->setValue(format.maxCharsPerLine);
    widgets.lines->setValue(format.maxLines);
    const int index = widgets.overflow->findData(int(format.overflow));
    widgets.overflow->setCurrentIndex(index >= 0 ? index : 0);
    widgets.uppercase->setChecked(format.uppercase);
}

void MessagePanel::applyStoredState(const TextFormat& messageFormat,
                                    const TextFormat& announcementFormat,
                                    int rotationSeconds)
{
    // Les signaux sont coupes : sinon chaque setValue relancerait un rendu et
    // un enregistrement des reglages pendant leur propre chargement.
    const QSignalBlocker b1(m_messageFormat.width);
    const QSignalBlocker b2(m_messageFormat.lines);
    const QSignalBlocker b3(m_messageFormat.overflow);
    const QSignalBlocker b4(m_messageFormat.uppercase);
    const QSignalBlocker b5(m_announcementFormat.width);
    const QSignalBlocker b6(m_announcementFormat.lines);
    const QSignalBlocker b7(m_announcementFormat.overflow);
    const QSignalBlocker b8(m_announcementFormat.uppercase);
    const QSignalBlocker b9(m_rotationSeconds);
    const QSignalBlocker b10(m_rotationEnabled);

    writeFormat(m_messageFormat, messageFormat);
    writeFormat(m_announcementFormat, announcementFormat);
    m_rotationSeconds->setValue(rotationSeconds);
    m_rotationEnabled->setChecked(false);
}

void MessagePanel::onMessageFormatChanged()
{
    if (m_center) m_center->setMessageFormat(readFormat(m_messageFormat));
    emit settingsChanged();
}

void MessagePanel::onAnnouncementFormatChanged()
{
    if (m_center) m_center->setAnnouncementFormat(readFormat(m_announcementFormat));
    emit settingsChanged();
}

// ---------------------------------------------------------------- message

void MessagePanel::onSendMessage()
{
    if (m_center) m_center->setMessage(m_messageEdit->toPlainText());
}

void MessagePanel::onClearMessage()
{
    // Le champ de saisie est conserve : effacer l'ecran ne doit pas faire
    // perdre un message qu'on vient de taper et qu'on veut reafficher.
    if (m_center) m_center->clearMessage();
}

void MessagePanel::onSaveTemplate()
{
    const QString text = m_messageEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    for (int i = 0; i < m_templateList->count(); ++i) {
        if (m_templateList->item(i)->text() == text) return;  // deja present
    }

    m_templateList->addItem(text);
    emit settingsChanged();
}

void MessagePanel::onRemoveTemplate()
{
    const int row = m_templateList->currentRow();
    if (row < 0) return;
    delete m_templateList->takeItem(row);
    emit settingsChanged();
}

void MessagePanel::onTemplateActivated()
{
    QListWidgetItem* item = m_templateList->currentItem();
    if (!item) return;
    m_messageEdit->setPlainText(item->text());
}

QStringList MessagePanel::templates() const
{
    QStringList result;
    for (int i = 0; i < m_templateList->count(); ++i) {
        result.append(m_templateList->item(i)->text());
    }
    return result;
}

void MessagePanel::setTemplates(const QStringList& templates)
{
    m_templateList->clear();
    m_templateList->addItems(templates);
}

// --------------------------------------------------------------- annonces

void MessagePanel::onAddAnnouncement()
{
    auto* item = new QListWidgetItem(tr("Nouvelle annonce"));
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_announcementList->addItem(item);
    m_announcementList->editItem(item);
    onAnnouncementsChanged();
}

void MessagePanel::onRemoveAnnouncement()
{
    const int row = m_announcementList->currentRow();
    if (row < 0) return;
    delete m_announcementList->takeItem(row);
    onAnnouncementsChanged();
}

void MessagePanel::onMoveAnnouncementUp()
{
    const int row = m_announcementList->currentRow();
    if (row <= 0) return;
    QListWidgetItem* item = m_announcementList->takeItem(row);
    m_announcementList->insertItem(row - 1, item);
    m_announcementList->setCurrentRow(row - 1);
    onAnnouncementsChanged();
}

void MessagePanel::onMoveAnnouncementDown()
{
    const int row = m_announcementList->currentRow();
    if (row < 0 || row >= m_announcementList->count() - 1) return;
    QListWidgetItem* item = m_announcementList->takeItem(row);
    m_announcementList->insertItem(row + 1, item);
    m_announcementList->setCurrentRow(row + 1);
    onAnnouncementsChanged();
}

void MessagePanel::onAnnouncementsChanged()
{
    if (m_center) m_center->setAnnouncements(announcements());
    emit settingsChanged();
}

QStringList MessagePanel::announcements() const
{
    QStringList result;
    for (int i = 0; i < m_announcementList->count(); ++i) {
        const QString text = m_announcementList->item(i)->text().trimmed();
        if (!text.isEmpty()) result.append(text);
    }
    return result;
}

void MessagePanel::setAnnouncements(const QStringList& lines)
{
    // Bloque les signaux pendant le remplissage : sinon chaque insertion
    // declencherait une reecriture des annonces et un enregistrement.
    const QSignalBlocker blocker(m_announcementList);
    m_announcementList->clear();
    for (const QString& line : lines) {
        auto* item = new QListWidgetItem(line);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_announcementList->addItem(item);
    }
    if (m_center) m_center->setAnnouncements(announcements());
}
