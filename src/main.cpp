#include <QApplication>
#include <QDebug>

#include "infra/AppSettings.h"
#include "infra/AsyncWorker.h"
#include "infra/DatabaseManager.h"
#include "infra/QuoteFetcher.h"

// �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
// main �X ���[ demo�A�T�{�U�Ҳեi���`�s��
// �������Цb���Ұ� MainWindow
// �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QuantDashboard");
    app.setOrganizationName("QuantDashboard");

    // �w�w 1. AppSettings �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
    auto& settings = AppSettings::instance();
    qDebug() << "[Settings] DB path:" << settings.dbPath();
    qDebug() << "[Settings] Risk-free rate:" << settings.riskFreeRate();
    qDebug() << "[Settings] Quote provider:" << settings.quoteProvider();

    // �w�w 2. DatabaseManager �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
    DatabaseManager db;
    if (!db.open(settings.dbPath())) {
        qWarning() << "[DB] Failed to open:" << db.lastError();
    }
    else {
        qDebug() << "[DB] Opened. Size:" << db.dbSizeBytes() << "bytes";
        qDebug() << "[DB] Symbols:" << db.availableSymbols();
    }

    // �w�w 3. QuoteFetcher �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
    QuoteFetcher fetcher;
    fetcher.setProvider(QuoteFetcher::Provider::Yahoo);  // �K key

    // ����Y�ɳ��� �� �L�X�æs�̷s���L�i DB
    QObject::connect(&fetcher, &QuoteFetcher::quoteReceived,
        [&db](const Quote& q) {
            qDebug() << "[Quote]" << q.symbol
                << "price:" << q.price
                << "chg%:" << QString::number(q.changePct, 'f', 2) << "%";

            // �s�@�����L�� DB�]�Y�ɼҦ��U�u���@�� bar�^
            OhlcBar bar;
            bar.symbol = q.symbol;
            bar.datetime = q.timestamp;
            bar.close = q.price;
            bar.open = bar.high = bar.low = q.price;
            bar.volume = q.volume;
            db.insertBars(q.symbol, { bar });
        });

    // ������v��� �� �s�J DB
    QObject::connect(&fetcher, &QuoteFetcher::historyReceived,
        [&db](const QString& sym, const QVector<OhlcBar>& bars) {
            qDebug() << "[History]" << sym << "bars:" << bars.size();
            db.insertBars(sym, bars);
        });

    QObject::connect(&fetcher, &QuoteFetcher::fetchError,
        [](const QString& sym, const QString& err) {
            qWarning() << "[FetchError]" << sym << err;
        });

    // ����@�~���v�A�A�}�l 60 ������
    const QStringList watchList = { "SOXX", "SMH", "QQQI" };
    for (const auto& sym : watchList)
        fetcher.fetchHistory(sym, "1y");
    fetcher.startPolling(watchList, settings.quoteRefreshSec());

    // �w�w 4. AsyncWorker �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
    AsyncWorker worker;

    // ����w�����G �� �L�X
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

    // �^�����G
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

    // �e�X�@�� BSM �w���ШD�]�D����^
    PricingRequest req;
    req.spot = 540.0;     // ���] SOXX �{��
    req.strike = 560.0;     // 5% OTM call
    req.riskFree = settings.riskFreeRate();
    req.volatility = settings.defaultVolatility();
    req.maturityYears = 30.0 / 365.0;
    req.optionType = PricingRequest::OptionType::Call;
    req.model = PricingRequest::Model::BlackScholes;
    worker.submitPricing(req);

    // �w�w �ƥ�j�� �w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w
    return app.exec();
}