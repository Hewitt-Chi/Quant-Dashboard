#include <QApplication>
#include <QDebug>

#include "infra/AppSettings.h"
#include "infra/AsyncWorker.h"
#include "infra/DatabaseManager.h"
#include "infra/QuoteFetcher.h"

// ─────────────────────────────────────────────────────────────────────────────
// main — 骨架 demo，確認各模組可正常連接
// 正式版請在此啟動 MainWindow
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QuantDashboard");
    app.setOrganizationName("QuantDashboard");

    // ── 1. AppSettings ────────────────────────────────────────────────────────
    auto& settings = AppSettings::instance();
    qDebug() << "[Settings] DB path:" << settings.dbPath();
    qDebug() << "[Settings] Risk-free rate:" << settings.riskFreeRate();
    qDebug() << "[Settings] Quote provider:" << settings.quoteProvider();

    // ── 2. DatabaseManager ────────────────────────────────────────────────────
    DatabaseManager db;
    if (!db.open(settings.dbPath())) {
        qWarning() << "[DB] Failed to open:" << db.lastError();
    }
    else {
        qDebug() << "[DB] Opened. Size:" << db.dbSizeBytes() << "bytes";
        qDebug() << "[DB] Symbols:" << db.availableSymbols();
    }

    // ── 3. QuoteFetcher ───────────────────────────────────────────────────────
    QuoteFetcher fetcher;
    fetcher.setProvider(QuoteFetcher::Provider::Yahoo);  // 免 key

    // 收到即時報價 → 印出並存最新收盤進 DB
    QObject::connect(&fetcher, &QuoteFetcher::quoteReceived,
        [&db](const Quote& q) {
            qDebug() << "[Quote]" << q.symbol
                << "price:" << q.price
                << "chg%:" << QString::number(q.changePct, 'f', 2) << "%";

            // 存一筆收盤到 DB（即時模式下只有一根 bar）
            OhlcBar bar;
            bar.symbol = q.symbol;
            bar.datetime = q.timestamp;
            bar.close = q.price;
            bar.open = bar.high = bar.low = q.price;
            bar.volume = q.volume;
            db.insertBars(q.symbol, { bar });
        });

    // 收到歷史資料 → 存入 DB
    QObject::connect(&fetcher, &QuoteFetcher::historyReceived,
        [&db](const QString& sym, const QVector<OhlcBar>& bars) {
            qDebug() << "[History]" << sym << "bars:" << bars.size();
            db.insertBars(sym, bars);
        });

    QObject::connect(&fetcher, &QuoteFetcher::fetchError,
        [](const QString& sym, const QString& err) {
            qWarning() << "[FetchError]" << sym << err;
        });

    // 先抓一年歷史，再開始 60 秒輪詢
    const QStringList watchList = { "SOXX", "SMH", "QQQI" };
    for (const auto& sym : watchList)
        fetcher.fetchHistory(sym, "1y");
    fetcher.startPolling(watchList, settings.quoteRefreshSec());

    // ── 4. AsyncWorker ────────────────────────────────────────────────────────
    AsyncWorker worker;

    // 收到定價結果 → 印出
    QObject::connect(&worker, &AsyncWorker::pricingFinished,
        [](const PricingResult& r) {
            if (!r.success) {
                qWarning() << "[Pricing] Error:" << r.errorMsg;
                return;
            }
            qDebug() << "[Pricing]"
                << "Price:" << QString::number(r.price, 'f', 4)
                << "Delta:" << QString::number(r.delta, 'f', 4)
                << "Gamma:" << QString::number(r.gamma, 'f', 6)
                << "Vega:" << QString::number(r.vega, 'f', 4)
                << "Theta:" << QString::number(r.theta, 'f', 4);
        });

    // 回測結果
    QObject::connect(&worker, &AsyncWorker::backtestFinished,
        [](const BacktestResult& r) {
            if (!r.success) {
                qWarning() << "[Backtest] Error:" << r.errorMsg;
                return;
            }
            qDebug() << "[Backtest]"
                << "Return:" << QString::number(r.totalReturn * 100, 'f', 2) << "%"
                << "Sharpe:" << QString::number(r.sharpeRatio, 'f', 3)
                << "MaxDD:" << QString::number(r.maxDrawdown * 100, 'f', 2) << "%"
                << "Premium:" << QString::number(r.premiumCollected, 'f', 2);
        });

    // 送出一筆 BSM 定價請求（非阻塞）
    PricingRequest req;
    req.spot = 540.0;     // 假設 SOXX 現價
    req.strike = 560.0;     // 5% OTM call
    req.riskFree = settings.riskFreeRate();
    req.volatility = settings.defaultVolatility();
    req.maturityYears = 30.0 / 365.0;
    req.optionType = PricingRequest::OptionType::Call;
    req.model = PricingRequest::Model::BlackScholes;
    worker.submitPricing(req);

    // ── 事件迴圈 ───────────────────────────────────────────────────────────────
    return app.exec();
}