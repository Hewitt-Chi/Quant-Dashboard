#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <optional>

// ─────────────────────────────────────────────────────────────────────────────
// OhlcBar — 一根 K 線
// ─────────────────────────────────────────────────────────────────────────────
struct OhlcBar
{
    QString   symbol;
    QDateTime datetime;
    double    open   = 0.0;
    double    high   = 0.0;
    double    low    = 0.0;
    double    close  = 0.0;
    qint64    volume = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// DatabaseManager
//
// 管理 SQLite 本地快取，儲存歷史 OHLCV 和定價快取。
// 使用「連線名稱」設計，支援多執行緒情境下各自開連線。
//
// 使用範例：
//   DatabaseManager db;
//   db.open(AppSettings::instance().dbPath());
//   db.insertBars("SOXX", bars);
//   auto bars = db.queryBars("SOXX", from, to);
// ─────────────────────────────────────────────────────────────────────────────
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    // ── 開關連線 ──────────────────────────────────────────────────────────────
    bool open(const QString& dbPath);
    void close();
    bool isOpen() const;

    // ── OHLCV ─────────────────────────────────────────────────────────────────
    bool insertBars(const QString& symbol, const QVector<OhlcBar>& bars);
    QVector<OhlcBar> queryBars(const QString& symbol,
                               const QDateTime& from,
                               const QDateTime& to) const;

    // 刪除某 symbol 所有資料
    bool deleteBars(const QString& symbol);

    // 查詢 DB 內有哪些 symbol
    QStringList availableSymbols() const;

    // 最新一筆的時間（用來決定要補哪段資料）
    std::optional<QDateTime> latestBarTime(const QString& symbol) const;

    // ── Pricing cache ─────────────────────────────────────────────────────────
    // key = 用來唯一識別一組定價參數的 hash 字串（呼叫端自行生成）
    bool   savePricingCache(const QString& key, double price,
                            const QString& greeksJson);
    bool   loadPricingCache(const QString& key,
                            double& price, QString& greeksJson) const;

    // ── 維護 ──────────────────────────────────────────────────────────────────
    bool vacuum();   // 整理 SQLite 碎片
    qint64 dbSizeBytes() const;

    QString lastError() const;

private:
    bool createTables();

    QString m_connectionName;
    QString m_lastError;
};
