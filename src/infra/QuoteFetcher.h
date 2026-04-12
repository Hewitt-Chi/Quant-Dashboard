#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QDateTime>
#include "DatabaseManager.h"   // OhlcBar

// ─────────────────────────────────────────────────────────────────────────────
// Quote — 即時報價快照
// ─────────────────────────────────────────────────────────────────────────────
struct Quote
{
    QString   symbol;
    double    price    = 0.0;
    double    change   = 0.0;   // 漲跌
    double    changePct= 0.0;   // 漲跌幅 %
    qint64    volume   = 0;
    QDateTime timestamp;
};

// ─────────────────────────────────────────────────────────────────────────────
// QuoteFetcher
//
// 封裝即時/歷史行情的 HTTP 抓取，支援：
//   - Yahoo Finance（無需 API key，適合開發測試）
//   - Polygon.io（需 API key，適合正式環境）
//
// 即時模式：呼叫 startPolling() 後每 N 秒自動發 signal。
// 歷史模式：呼叫 fetchHistory() 一次性取得歷史 K 線。
//
// 使用範例：
//   auto* fetcher = new QuoteFetcher(this);
//   connect(fetcher, &QuoteFetcher::quoteReceived,
//           this,    &MyWidget::onQuote);
//   fetcher->startPolling({"SOXX","SMH","QQQI"}, 60);
// ─────────────────────────────────────────────────────────────────────────────
class QNetworkAccessManager;
class QNetworkReply;

class QuoteFetcher : public QObject
{
    Q_OBJECT

public:
    enum class Provider { Yahoo, Polygon };

    explicit QuoteFetcher(QObject* parent = nullptr);
    ~QuoteFetcher();

    void setProvider(Provider p);
    void setApiKey(const QString& key);   // Polygon 專用

    // ── 即時輪詢 ──────────────────────────────────────────────────────────────
    void startPolling(const QStringList& symbols, int intervalSec = 60);
    void stopPolling();
    bool isPolling() const;

    // ── 手動單次抓取 ──────────────────────────────────────────────────────────
    void fetchQuotes(const QStringList& symbols);

    // ── 歷史 K 線（存入 DB）──────────────────────────────────────────────────
    // range: "1y" | "6mo" | "3mo" | "1mo"
    void fetchHistory(const QString& symbol, const QString& range = "1y");

signals:
    void quoteReceived(const Quote& quote);
    void historyReceived(const QString& symbol, const QVector<OhlcBar>& bars);
    void fetchError(const QString& symbol, const QString& errorMsg);

private slots:
    void onPollTimer();

private:
    // ── Yahoo Finance ─────────────────────────────────────────────────────────
    void fetchYahooQuote(const QString& symbol);
    void fetchYahooHistory(const QString& symbol, const QString& range);
    void parseYahooQuote(QNetworkReply* reply, const QString& symbol);
    void parseYahooHistory(QNetworkReply* reply, const QString& symbol);

    // ── Polygon.io ───────────────────────────────────────────────────────────
    void fetchPolygonQuote(const QString& symbol);
    void fetchPolygonHistory(const QString& symbol, const QString& range);
    void parsePolygonQuote(QNetworkReply* reply, const QString& symbol);
    void parsePolygonHistory(QNetworkReply* reply, const QString& symbol);

    QNetworkAccessManager* m_nam      = nullptr;
    QTimer*                m_timer    = nullptr;
    Provider               m_provider = Provider::Yahoo;
    QString                m_apiKey;
    QStringList            m_watchList;
};
