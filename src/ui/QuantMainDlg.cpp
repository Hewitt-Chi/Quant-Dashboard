#include "QuantMainDlg.h"
#include "ui_QuantMainDlg.h"   // ← 加上這行
#include <QApplication>
#include "PricerWidget.h"
#include "WatchlistWidget.h"
#include "BacktestWidget.h"
#include "SettingsWidget.h"
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

    if (cfg.quoteProvider() == "polygon") {
        m_fetcher->setProvider(QuoteFetcher::Provider::Polygon);
        m_fetcher->setApiKey(cfg.apiKey());
    } else {
    m_fetcher->setProvider(QuoteFetcher::Provider::Yahoo);
    }

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
   
    m_fetcher->stopPolling();
    if (cfg.quoteProvider() == "polygon") {
        m_fetcher->setProvider(QuoteFetcher::Provider::Polygon);
        m_fetcher->setApiKey(cfg.apiKey());
    } else {
        m_fetcher->setProvider(QuoteFetcher::Provider::Yahoo);
    }
    m_fetcher->startPolling(watch, cfg.quoteRefreshSec());
    statusBar()->showMessage("Settings applied ??polling restarted.", 3000);
}
// ─────────────────────────────────────────────────────────────────────────────
// setupUi
// ─────────────────────────────────────────────────────────────────────────────

void QuantMainDlg::onSettingsChanged()
{
    // 重新套用 provider 設定
    auto& cfg = AppSettings::instance();
    m_fetcher->stopPolling();
    if (cfg.quoteProvider() == "polygon") {
        m_fetcher->setProvider(QuoteFetcher::Provider::Polygon);
        m_fetcher->setApiKey(cfg.apiKey());
    }
    else {
        m_fetcher->setProvider(QuoteFetcher::Provider::Yahoo);
    }
    const QStringList watch = { "SOXX","SMH","QQQI" };
    m_fetcher->startPolling(watch, cfg.quoteRefreshSec());
    statusBar()->showMessage("Settings applied — polling restarted.", 3000);
}

void QuantMainDlg::setupUi()
{
    auto* nav = new QListWidget(this);
    nav->setFixedWidth(164);
    nav->setSpacing(2);
    nav->setFrameShape(QFrame::NoFrame);
    nav->setStyleSheet(
        "QListWidget{background:palette(window);border-right:1px solid palette(mid);}"
        "QListWidget::item{padding:9px 14px;border-radius:6px;margin:2px 6px;font-size:13px;}"
        "QListWidget::item:selected{background:palette(highlight);color:palette(highlighted-text);}");

    QFont gf; gf.setPointSize(9);

    // ── 加 group 標籤（不可選）────────────────────────────────────────────────
    auto addGroup = [&](const QString& label) {
        auto* it = new QListWidgetItem(label, nav);
        it->setFlags(Qt::NoItemFlags);
        it->setForeground(QColor(110, 110, 110));
        it->setFont(gf);
        it->setData(Qt::UserRole, -1);   // -1 = 不對應任何頁面
        };

    addGroup("  MARKET");       // row 0
    nav->addItem("  Watchlist");   // row 1
    addGroup("  ANALYSIS");     // row 2
    nav->addItem("  Option pricer");// row 3
    addGroup("  STRATEGY");     // row 4
    nav->addItem("  Backtest");    // row 5
    addGroup("  SYSTEM");       // row 6
    nav->addItem("  Settings");    // row 7

    auto* stack = new QStackedWidget(this);
    m_watchlist = new WatchlistWidget(m_fetcher, m_db, this);
    m_pricer    = new PricerWidget(m_worker, this);
    m_backtest  = new BacktestWidget(m_worker, m_db, this);
    m_settings  = new SettingsWidget(this);
    stack->addWidget(m_watchlist);   // 0
    stack->addWidget(m_pricer);      // 1
    stack->addWidget(m_backtest);    // 2
    stack->addWidget(m_settings);    // 3

    // row ??page index
    const QMap<int,int> rowToPage = {{1,0},{3,1},{5,2},{7,3}};
    connect(nav, &QListWidget::currentRowChanged, [stack, rowToPage](int row){
        if (rowToPage.contains(row))
            stack->setCurrentIndex(rowToPage[row]);
    });

    // Status bar
   /* auto connectStatus = [this](QObject* w, const char* sig) {
        connect(w, sig, this, [this](const QString& m){
            statusBar()->showMessage(m);
        });
        };*/
    connect(m_watchlist, &WatchlistWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_pricer,    &PricerWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_backtest,  &BacktestWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_settings,  &SettingsWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_settings,  &SettingsWidget::settingsChanged,
            this, &QuantMainDlg::onSettingsChanged);

    //nav->setCurrentRow(1);
    nav->setCurrentItem(nav->item(1));

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(nav);
    splitter->addWidget(stack);
    splitter->setStretchFactor(0,0);
    splitter->setStretchFactor(1,1);
    splitter->handle(1)->setEnabled(false);
    setCentralWidget(splitter);

    auto* dbLbl = new QLabel("DB: " + AppSettings::instance().dbPath(), this);
    dbLbl->setStyleSheet("font-size:10px;color:gray;margin-right:6px;");
    statusBar()->addPermanentWidget(dbLbl);
    statusBar()->showMessage("Ready", 2000);
}
