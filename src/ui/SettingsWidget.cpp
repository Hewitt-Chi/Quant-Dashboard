#include "SettingsWidget.h"
#include "../infra/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QScrollArea>

// ─────────────────────────────────────────────────────────────────────────────
SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    loadFromSettings();
}

// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::buildUi()
{
    // 外層：ScrollArea 讓內容可以捲動
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget;
    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(16);

    auto* title = new QLabel("Settings", container);
    title->setStyleSheet("font-size:20px;font-weight:500;");
    root->addWidget(title);

    // ── 1. Quote provider ─────────────────────────────────────────────────────
    {
        auto* box  = new QGroupBox("Quote provider", this);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setHorizontalSpacing(16); form->setVerticalSpacing(10);
        form->setContentsMargins(16,12,16,12);

        m_provider = new QComboBox(box);
        m_provider->addItem("Yahoo Finance (free, no key)", "yahoo");
        m_provider->addItem("Polygon.io (requires API key)", "polygon");
        form->addRow("Provider:", m_provider);
        connect(m_provider, &QComboBox::currentIndexChanged,
                this, &SettingsWidget::onProviderChanged);

        auto* keyRow = new QHBoxLayout;
        m_apiKey = new QLineEdit(box);
        m_apiKey->setPlaceholderText("Polygon API key");
        m_apiKey->setEchoMode(QLineEdit::Password);
        m_apiKey->setMinimumWidth(280);
        m_toggleKeyBtn = new QPushButton("Show", box);
        m_toggleKeyBtn->setFixedWidth(54);
        m_toggleKeyBtn->setStyleSheet(
            "QPushButton{border:0.5px solid palette(mid);border-radius:4px;"
            "padding:2px 6px;font-size:11px;}"
            "QPushButton:hover{background:palette(midlight);}");
        connect(m_toggleKeyBtn, &QPushButton::clicked, [this]{
            m_keyVisible = !m_keyVisible;
            m_apiKey->setEchoMode(m_keyVisible ? QLineEdit::Normal : QLineEdit::Password);
            m_toggleKeyBtn->setText(m_keyVisible ? "Hide" : "Show");
        });
        keyRow->addWidget(m_apiKey, 1);
        keyRow->addWidget(m_toggleKeyBtn);
        form->addRow("API key:", keyRow);

        auto* note = new QLabel(
            "Polygon free tier: 5 requests/min, sufficient for 60s polling.", box);
        note->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
        form->addRow("", note);
        root->addWidget(box);
    }

    // ── 2. Refresh interval ───────────────────────────────────────────────────
    {
        auto* box  = new QGroupBox("Refresh interval", this);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setHorizontalSpacing(16); form->setVerticalSpacing(10);
        form->setContentsMargins(16,12,16,12);

        m_refreshSec = new QSpinBox(box);
        m_refreshSec->setRange(10, 3600);
        m_refreshSec->setSuffix(" seconds");
        m_refreshSec->setMinimumWidth(140);
        auto* refreshRow = new QHBoxLayout;
        refreshRow->addWidget(m_refreshSec);
        refreshRow->addWidget(new QLabel("(restart app to apply)", box));
        refreshRow->addStretch();
        form->addRow("Quote refresh:", refreshRow);
        root->addWidget(box);
    }

    // ── 3. Pricing defaults ───────────────────────────────────────────────────
    {
        auto* box  = new QGroupBox("Pricing defaults", this);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setHorizontalSpacing(16); form->setVerticalSpacing(10);
        form->setContentsMargins(16,12,16,12);

        m_riskFree = new QDoubleSpinBox(box);
        m_riskFree->setRange(0.0, 0.5);
        m_riskFree->setSingleStep(0.005);
        m_riskFree->setDecimals(3);
        m_riskFree->setSuffix("  (e.g. 0.050 = 5%)");
        m_riskFree->setMinimumWidth(200);
        form->addRow("Risk-free rate r:", m_riskFree);

        m_defaultVol = new QDoubleSpinBox(box);
        m_defaultVol->setRange(0.01, 5.0);
        m_defaultVol->setSingleStep(0.01);
        m_defaultVol->setDecimals(3);
        m_defaultVol->setSuffix("  (e.g. 0.200 = 20%)");
        m_defaultVol->setMinimumWidth(200);
        form->addRow("Default volatility:", m_defaultVol);
        root->addWidget(box);
    }

    // ── 4. Watchlist symbols ─────────────────────────────────────────
    {
        auto* box = new QGroupBox("Watchlist symbols", this);
        box->setMinimumHeight(220);
        auto* vlay = new QVBoxLayout(box);
        vlay->setContentsMargins(16, 12, 16, 12);
        vlay->setSpacing(8);

        auto* note = new QLabel(
            "Symbols loaded on startup. Changes apply after restart.", box);
        note->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
        vlay->addWidget(note);

        m_watchSymList = new QListWidget(box);
        m_watchSymList->setFixedHeight(100);
        m_watchSymList->setSelectionMode(QAbstractItemView::SingleSelection);
        m_watchSymList->setStyleSheet(
            "QListWidget{border:1px solid palette(mid);border-radius:4px;"
            "background:palette(base);}"
            "QListWidget::item{padding:6px 10px;font-size:13px;}"
            "QListWidget::item:selected{background:palette(highlight);"
            "color:palette(highlighted-text);}");
        vlay->addWidget(m_watchSymList);  // list 獨立一行

        // 輸入列：另外建立，加到 vlay 而非 list
        m_watchSymInput = new QLineEdit(box);
        m_watchSymInput->setPlaceholderText("Add symbol (e.g. NVDA)");
        m_watchSymInput->setMaximumWidth(160);

        auto* addBtn = new QPushButton("Add", box);
        addBtn->setStyleSheet(
            "QPushButton{background:#2563eb;color:white;border-radius:5px;"
            "padding:4px 12px;}QPushButton:hover{background:#1d4ed8;}");

        auto* removeBtn = new QPushButton("Remove selected", box);
        removeBtn->setStyleSheet(
            "QPushButton{border:0.5px solid palette(mid);border-radius:5px;"
            "padding:4px 12px;}QPushButton:hover{background:palette(midlight);}");

        connect(addBtn, &QPushButton::clicked,
            this, &SettingsWidget::onAddWatchSymbol);
        connect(m_watchSymInput, &QLineEdit::returnPressed,
            this, &SettingsWidget::onAddWatchSymbol);
        connect(removeBtn, &QPushButton::clicked,
            this, &SettingsWidget::onRemoveWatchSymbol);

        auto* inputRow = new QHBoxLayout;
        inputRow->addWidget(m_watchSymInput);
        inputRow->addWidget(addBtn);
        inputRow->addWidget(removeBtn);
        inputRow->addStretch();
        vlay->addLayout(inputRow);  // ← 輸入列加到 vlay，不是 list

        root->addWidget(box);
    }

    // ── 5. Appearance / Theme ─────────────────────────────────────────────────
    {
        auto* box = new QGroupBox("Appearance", this);
        auto* lay = new QHBoxLayout(box);
        lay->setContentsMargins(16,12,16,12);

        m_darkModeChk = new QCheckBox("Dark mode", box);
        m_darkModeChk->setStyleSheet("font-size:13px;");
        auto* hint = new QLabel("(applies immediately)", box);
        hint->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.6);");

        lay->addWidget(m_darkModeChk);
        lay->addWidget(hint);
        lay->addStretch();

        connect(m_darkModeChk, &QCheckBox::toggled,
                this, &SettingsWidget::onThemeToggled);
        root->addWidget(box);
    }

    // ── 6. Database ───────────────────────────────────────────────────────────
    {
        auto* box  = new QGroupBox("Database", this);
        auto* form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setHorizontalSpacing(16); form->setVerticalSpacing(10);
        form->setContentsMargins(16,12,16,12);

        auto* pathRow = new QHBoxLayout;
        m_dbPath = new QLineEdit(box);
        m_dbPath->setMinimumWidth(280);
        auto* browseBtn = new QPushButton("Browse...", box);
        browseBtn->setFixedWidth(80);
        connect(browseBtn, &QPushButton::clicked,
                this, &SettingsWidget::onBrowseDb);
        pathRow->addWidget(m_dbPath, 1);
        pathRow->addWidget(browseBtn);
        form->addRow("DB path:", pathRow);

        m_dbSizeLabel = new QLabel("—", box);
        m_dbSizeLabel->setStyleSheet(
            "font-size:11px;color:rgba(150,150,150,0.7);");
        form->addRow("DB size:", m_dbSizeLabel);

        auto* dbNote = new QLabel(
            "Changes to DB path take effect after restart.", box);
        dbNote->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
        form->addRow("", dbNote);
        root->addWidget(box);
    }

    // ── Save / Reset ──────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    m_saveBtn = new QPushButton("Save settings", this);
    m_saveBtn->setMinimumHeight(36); m_saveBtn->setMinimumWidth(130);
    m_saveBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#1d4ed8;}");
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsWidget::onSave);

    m_resetBtn = new QPushButton("Reset to defaults", this);
    m_resetBtn->setMinimumHeight(36);
    m_resetBtn->setStyleSheet(
        "QPushButton{border:1px solid palette(mid);border-radius:6px;padding:0 12px;}"
        "QPushButton:hover{background:palette(midlight);}");
    connect(m_resetBtn, &QPushButton::clicked, this, &SettingsWidget::onReset);

    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_resetBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);
    root->addStretch();

    root->addStretch();

    // 把 container 放進 scrollArea
    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::loadFromSettings()
{
    auto& cfg = AppSettings::instance();

    // Provider
    int provIdx = cfg.quoteProvider() == "polygon" ? 1 : 0;
    m_provider->setCurrentIndex(provIdx);
    onProviderChanged(provIdx);

    m_apiKey->setText(cfg.apiKey());
    m_refreshSec->setValue(cfg.quoteRefreshSec());
    m_riskFree->setValue(cfg.riskFreeRate());
    m_defaultVol->setValue(cfg.defaultVolatility());
    m_dbPath->setText(cfg.dbPath());

    // Watchlist
    if (m_watchSymList) {
        m_watchSymList->clear();
        for (const auto& sym : cfg.watchlist())
            m_watchSymList->addItem(sym);
    }

    // Theme
    if (m_darkModeChk)
        m_darkModeChk->setChecked(cfg.darkMode());

    // DB size
    QFileInfo fi(cfg.dbPath());
    if (fi.exists())
        m_dbSizeLabel->setText(
            QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1));
    else
        m_dbSizeLabel->setText("File not found");
}

// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::onSave()
{
    auto& cfg = AppSettings::instance();
    cfg.setQuoteProvider(m_provider->currentData().toString());
    cfg.setApiKey(m_apiKey->text().trimmed());
    cfg.setQuoteRefreshSec(m_refreshSec->value());
    cfg.setRiskFreeRate(m_riskFree->value());
    cfg.setDefaultVolatility(m_defaultVol->value());
    cfg.setDbPath(m_dbPath->text().trimmed());
    emit settingsChanged();
    emit statusMessage("Settings saved.");
}

void SettingsWidget::onReset()
{
    AppSettings::instance().reset();
    loadFromSettings();
    emit statusMessage("Settings reset to defaults.");
}

void SettingsWidget::onProviderChanged(int index)
{
    bool isPolygon = (index == 1);
    m_apiKey->setEnabled(isPolygon);
    m_toggleKeyBtn->setEnabled(isPolygon);
    m_apiKey->setStyleSheet(isPolygon ? "" : "color:palette(mid);");
}

void SettingsWidget::onBrowseDb()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Select database file", m_dbPath->text(),
        "SQLite database (*.db);;All files (*)");
    if (!path.isEmpty())
        m_dbPath->setText(path);
}

// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::onThemeToggled(bool dark)
{
    AppSettings::instance().setDarkMode(dark);
    emit themeChanged(dark);
    emit statusMessage(dark ? "Dark mode enabled." : "Light mode enabled.");
}

void SettingsWidget::onAddWatchSymbol()
{
    if (!m_watchSymInput || !m_watchSymList) return;
    QString sym = m_watchSymInput->text().trimmed().toUpper();
    if (sym.isEmpty()) return;
    for (int i = 0; i < m_watchSymList->count(); ++i)
        if (m_watchSymList->item(i)->text() == sym) return;
    m_watchSymList->addItem(sym);
    m_watchSymInput->clear();
    QStringList syms;
    for (int i = 0; i < m_watchSymList->count(); ++i)
        syms << m_watchSymList->item(i)->text();
    AppSettings::instance().setWatchlist(syms);
    emit statusMessage("Added " + sym + " to watchlist (restart to apply).");
}

void SettingsWidget::onRemoveWatchSymbol()
{
    if (!m_watchSymList) return;
    QListWidgetItem* item = m_watchSymList->currentItem();
    if (!item) return;
    QString sym = item->text();
    delete item;
    QStringList syms;
    for (int i = 0; i < m_watchSymList->count(); ++i)
        syms << m_watchSymList->item(i)->text();
    AppSettings::instance().setWatchlist(syms);
    emit statusMessage("Removed " + sym + " from watchlist (restart to apply).");
}
