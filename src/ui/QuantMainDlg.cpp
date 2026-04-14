#include "QuantMainDlg.h"
#include "ui_QuantMainDlg.h"   // ← 加上這行
#include <QApplication>
#include "PricerWidget.h"
#include "../infra/AppSettings.h"
#include "../infra/AsyncWorker.h"
#include "../infra/DatabaseManager.h"
#include "../infra/QuoteFetcher.h"

#include <QDockWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <qoverload.h>


QuantMainDlg::QuantMainDlg(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QuantLib-QT Dashboard");
    setMinimumSize(1000, 680);
    resize(1200, 780);

    setupInfra();
    setupUi();
}

QuantMainDlg::~QuantMainDlg()
{
    // QuoteFetcher 先停，避免 callback 打到已釋放的 DB
    if (m_fetcher) m_fetcher->stopPolling();
}
// ─────────────────────────────────────────────────────────────────────────────
// setupInfra
// ─────────────────────────────────────────────────────────────────────────────
void QuantMainDlg::setupInfra()
{
    auto& cfg = AppSettings::instance();

    // ── Worker ────────────────────────────────────────────────────────────────
    m_worker = new AsyncWorker(this);

    // ── Database ──────────────────────────────────────────────────────────────
    m_db = new DatabaseManager(this);
    if (!m_db->open(cfg.dbPath()))
        statusBar()->showMessage("DB error: " + m_db->lastError(), 5000);

    // ── QuoteFetcher ──────────────────────────────────────────────────────────
    m_fetcher = new QuoteFetcher(this);
    m_fetcher->setProvider(QuoteFetcher::Provider::Yahoo);

    connect(m_fetcher, &QuoteFetcher::historyReceived,
        [this](const QString& sym, const QVector<OhlcBar>& bars) {
            m_db->insertBars(sym, bars);
        });
    connect(m_fetcher, &QuoteFetcher::fetchError,
        [this](const QString& sym, const QString& err) {
            statusBar()->showMessage(sym + ": " + err, 3000);
        });

    // 啟動時抓一年歷史，之後每 60 秒輪詢即時報價
    const QStringList watchList = { "SOXX", "SMH", "QQQI" };
    for (const auto& sym : watchList)
        m_fetcher->fetchHistory(sym, "1y");
    m_fetcher->startPolling(watchList, cfg.quoteRefreshSec());
}

// ─────────────────────────────────────────────────────────────────────────────
// setupUi
// ─────────────────────────────────────────────────────────────────────────────
void QuantMainDlg::setupUi()
{
    // ── 左側導覽列（QListWidget）──────────────────────────────────────────────
    auto* navList = new QListWidget(this);
    navList->setFixedWidth(160);
    navList->setSpacing(2);
    navList->setFrameShape(QFrame::NoFrame);
    navList->setStyleSheet(
        "QListWidget { background: palette(window); border-right: 1px solid palette(mid); }"
        "QListWidget::item { padding: 8px 14px; border-radius: 6px; margin: 2px 6px; }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
    );

    // 導覽項目（之後加頁面時在這裡新增）
    navList->addItem("Option pricer");
    // navList->addItem("Greeks chart");    // Phase 2
    // navList->addItem("Yield curve");
    // navList->addItem("Backtest");
    // navList->addItem("Watchlist");
    navList->setCurrentRow(0);

    // ── 右側頁面容器（QStackedWidget）────────────────────────────────────────
    auto* stack = new QStackedWidget(this);

    m_pricer = new PricerWidget(m_worker, this);
    stack->addWidget(m_pricer);

    connect(m_pricer, &PricerWidget::statusMessage,
        this, [this](const QString& msg) {
            statusBar()->showMessage(msg);
        });

    // 導覽切換
    connect(navList, &QListWidget::currentRowChanged,
        stack, &QStackedWidget::setCurrentIndex);

    // ── 主分割佈局 ────────────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(navList);
    splitter->addWidget(stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->handle(1)->setEnabled(false);   // 固定導覽列寬度

    setCentralWidget(splitter);

    // ── Status bar ────────────────────────────────────────────────────────────
    auto* dbInfo = new QLabel(
        QString("DB: %1").arg(AppSettings::instance().dbPath()), this);
    dbInfo->setStyleSheet("font-size:11px; color:gray; margin-right:8px;");
    statusBar()->addPermanentWidget(dbInfo);
    statusBar()->showMessage("Ready", 2000);
}

