#include "QuantMainDlg.h"
#include "ui_QuantMainDlg.h"   // ← 加上這行
#include <QApplication>
#include "PricerWidget.h"
#include "WatchlistWidget.h"
#include "BacktestWidget.h"
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
#include <QMap>


QuantMainDlg::QuantMainDlg(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QuantLib-QT Dashboard");
    setMinimumSize(1080, 700);
    resize(1280, 820);
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
    const QStringList watch = {"SOXX","SMH","QQQI"};
    for (const auto& s : watch) m_fetcher->fetchHistory(s,"1y");
    m_fetcher->startPolling(watch, cfg.quoteRefreshSec());
}

// ─────────────────────────────────────────────────────────────────────────────
// setupUi
// ─────────────────────────────────────────────────────────────────────────────
void QuantMainDlg::setupUi()
{
    auto* nav = new QListWidget(this);
    nav->setFixedWidth(164);
    nav->setSpacing(2);
    nav->setFrameShape(QFrame::NoFrame);
    nav->setStyleSheet(
        "QListWidget { background: palette(window); border-right: 1px solid palette(mid); }"
        "QListWidget::item{padding:9px 14px;border-radius:6px;margin:2px 6px;font-size:13px;}"
        "QListWidget::item:selected{background:palette(highlight);color:palette(highlighted-text);}");

    auto* grpItem1 = new QListWidgetItem("  MARKET",  nav);
    grpItem1->setFlags(Qt::NoItemFlags);
    grpItem1->setForeground(QColor(110,110,110));
    QFont gf; gf.setPointSize(9); grpItem1->setFont(gf);
    nav->addItem("  Watchlist");

    auto* grpItem2 = new QListWidgetItem("  ANALYSIS", nav);
    grpItem2->setFlags(Qt::NoItemFlags);
    grpItem2->setForeground(QColor(110,110,110));
    grpItem2->setFont(gf);
    nav->addItem("  Option pricer");

    auto* grpItem3 = new QListWidgetItem("  STRATEGY", nav);
    grpItem3->setFlags(Qt::NoItemFlags);
    grpItem3->setForeground(QColor(110,110,110));
    grpItem3->setFont(gf);
    nav->addItem("  Backtest");

    auto* stack = new QStackedWidget(this);
    m_watchlist = new WatchlistWidget(m_fetcher, m_db, this);
    m_pricer = new PricerWidget(m_worker, this);
    m_backtest  = new BacktestWidget(m_worker, m_db, this);
    stack->addWidget(m_watchlist);
    stack->addWidget(m_pricer);
    stack->addWidget(m_backtest);

    QMap<int,int> rowToPage = {{1,0},{3,1},{5,2}};
    connect(nav, &QListWidget::currentRowChanged, [stack, rowToPage](int row){
        if (rowToPage.contains(row))
            stack->setCurrentIndex(rowToPage[row]);
        });

    connect(m_watchlist, &WatchlistWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_pricer, &PricerWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_backtest, &BacktestWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });

    nav->setCurrentRow(1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(nav);
    splitter->addWidget(stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->handle(1)->setEnabled(false);
    setCentralWidget(splitter);

    auto* dbLbl = new QLabel("DB: " + AppSettings::instance().dbPath(), this);
    dbLbl->setStyleSheet("font-size:10px;color:gray;margin-right:6px;");
    statusBar()->addPermanentWidget(dbLbl);
    statusBar()->showMessage("Ready", 2000);
}

