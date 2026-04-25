#include "QuantMainDlg.h"
#include "ui_QuantMainDlg.h"   // ¡ö ¥[¤W³o¦æ
#include <QApplication>
#include "PricerWidget.h"
#include "WatchlistWidget.h"
#include "BacktestWidget.h"
#include "SettingsWidget.h"
#include "YieldCurveWidget.h"
#include "OptionChainWidget.h"
#include "VolSurfaceWidget.h"
#include "../infra/AppSettings.h"
#include "../infra/AsyncWorker.h"
#include "../infra/DatabaseManager.h"
#include "../infra/QuoteFetcher.h"
#include "MonteCarloWidget.h"


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
#include <QTimer>


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
    // QuoteFetcher ¥ý°±¡AÁ×§K callback ¥´¨ì¤wÄÀ©ñªº DB
    if (m_fetcher) m_fetcher->stopPolling();
}
// ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
// setupInfra
// ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
void QuantMainDlg::setupInfra()
{
    auto& cfg = AppSettings::instance();

    // ¢w¢w Worker ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
    m_worker = new AsyncWorker(this);

    // ¢w¢w Database ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
    m_db = new DatabaseManager(this);
    if (!m_db->open(cfg.dbPath()))
        statusBar()->showMessage("DB error: " + m_db->lastError(), 5000);

    // ¢w¢w QuoteFetcher ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
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
// ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
// setupUi
// ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w

void QuantMainDlg::onSettingsChanged()
{
    // ­«·s®M¥Î provider ³]©w
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
    statusBar()->showMessage("Settings applied ¡X polling restarted.", 3000);
}

void QuantMainDlg::setupUi()
{
    auto* nav = new QListWidget(this);
    nav->setFixedWidth(220);
    nav->setSpacing(2);
    nav->setFrameShape(QFrame::NoFrame);
    nav->setStyleSheet(
        "QListWidget{background:palette(window);border-right:1px solid palette(mid);}"
        "QListWidget::item{padding:9px 10px;border-radius:6px;margin:2px 4px;font-size:13px;}"
        "QListWidget::item:selected{background:palette(highlight);color:palette(highlighted-text);}");

    QFont gf; gf.setPointSize(9);

    auto addGroup = [&](const QString& label) {
        auto* it = new QListWidgetItem(label, nav);
        it->setFlags(Qt::NoItemFlags);
        it->setForeground(QColor(110, 110, 110));
        it->setFont(gf);
        it->setData(Qt::UserRole, -1);
        };
    auto addPage = [&](const QString& label, int pageIndex) {
        auto* it = new QListWidgetItem(label, nav);
        it->setData(Qt::UserRole, pageIndex);
        };

    /*addGroup(" MARKET");
    addPage(" Watchlist", 0);
    addGroup(" ANALYSIS");
    addPage(" Option pricer", 1);
    addPage(" Backtest", 2);
    addPage(" Yield curve", 3);
    addPage(" Option chain",  4);
    addPage(" Monte Carlo",   5);
    addGroup(" STRATEGY");
    addGroup(" SYSTEM");
    addPage(" Settings", 6);*/
    
    addGroup("  ANALYSIS");
    addPage(" Watchlist", 0);
    addPage("  Option pricer", 1);
    addPage("  Yield curve", 3);
    addPage("  Option chain", 4);
    addPage("  Monte Carlo", 5);  // ¡ö ½T»{¦³³o¦æ
    addGroup("  STRATEGY");
    addPage("  Backtest", 2);
    addGroup("  SYSTEM");
    addPage("  Vol surface", 6);  // ¡ö index §ï¦¨ 6
    addPage("  Settings", 7);  // ¡ö index §ï¦¨ 7

    auto* stack = new QStackedWidget(this);
    m_watchlist = new WatchlistWidget(m_fetcher, m_db, this);
    m_pricer = new PricerWidget(m_worker, this);
    m_backtest = new BacktestWidget(m_worker, m_db, this);
    m_yieldCurve = new YieldCurveWidget(m_worker, this);
    m_settings = new SettingsWidget(this);
    m_yieldCurve = new YieldCurveWidget(m_worker, this);
    m_monteCarlo = new MonteCarloWidget(m_worker, this);
    m_volSurface = new VolSurfaceWidget(m_worker, this);
    // ¦b setupUi() ¸Ì
    m_optionChain = new OptionChainWidget(m_worker, this);
    stack->addWidget(m_watchlist);     // 0
    stack->addWidget(m_pricer);        // 1
    stack->addWidget(m_backtest);      // 2
    stack->addWidget(m_yieldCurve);    // 3
    stack->addWidget(m_optionChain);   // 4
    stack->addWidget(m_monteCarlo);    // 5  ¡ö ½T»{¦³³o¦æ
    stack->addWidget(m_volSurface);    // 6  ¡ö Vol surface ©¹«á²¾
    stack->addWidget(m_settings);      // 7  ¡ö Settings ©¹«á²¾

    connect(nav, &QListWidget::currentItemChanged,
        [stack](QListWidgetItem* item, QListWidgetItem*) {
            if (!item) return;
            int page = item->data(Qt::UserRole).toInt();
            if (page >= 0) stack->setCurrentIndex(page);
        });

    // Status bar
    connect(m_watchlist, &WatchlistWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_pricer,    &PricerWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_backtest,  &BacktestWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_settings,  &SettingsWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_yieldCurve, &YieldCurveWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_optionChain, &OptionChainWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_volSurface, &VolSurfaceWidget::statusMessage,
        this, [this](const QString& m) { statusBar()->showMessage(m); });
    connect(m_monteCarlo, &MonteCarloWidget::statusMessage,
            this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(m_settings,  &SettingsWidget::settingsChanged,
            this, &QuantMainDlg::onSettingsChanged);

    // ?è¨­??Watchlist
    nav->setCurrentItem(nav->item(1));

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(nav);
        splitter->addWidget(stack);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->handle(1)->setEnabled(false);
        splitter->setSizes({ 220, 1060 });
        setCentralWidget(splitter);

        QTimer::singleShot(0, this, [splitter]() {
            splitter->setSizes({ 220, splitter->width() - 220 });
            });                                                        // ¡ö ³o¦æ­n¦³

        auto* dbLbl = new QLabel("DB: " + AppSettings::instance().dbPath(), this);
        dbLbl->setStyleSheet("font-size:10px;color:gray;margin-right:6px;");
        statusBar()->addPermanentWidget(dbLbl);
        statusBar()->showMessage("Ready", 2000);
     
}
