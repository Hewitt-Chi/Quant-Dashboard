#include "DatabaseManager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QUuid>

// ─────────────────────────────────────────────────────────────────────────────
// 每個 DatabaseManager 實例使用唯一連線名稱，
// 避免多執行緒使用同一個 QSqlDatabase 連線（Qt 不允許跨執行緒共享）
// ─────────────────────────────────────────────────────────────────────────────
DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

// ─────────────────────────────────────────────────────────────────────────────
// open / close
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::open(const QString& dbPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    // WAL 模式：允許讀寫並行，適合多執行緒場景
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    q.exec("PRAGMA foreign_keys=ON");

    return createTables();
}

void DatabaseManager::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseManager::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
           && QSqlDatabase::database(m_connectionName).isOpen();
}

// ─────────────────────────────────────────────────────────────────────────────
// createTables — DDL
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    // OHLCV 表
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS ohlcv (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol   TEXT    NOT NULL,
            datetime INTEGER NOT NULL,   -- Unix timestamp (ms)
            open     REAL    NOT NULL,
            high     REAL    NOT NULL,
            low      REAL    NOT NULL,
            close    REAL    NOT NULL,
            volume   INTEGER NOT NULL DEFAULT 0,
            UNIQUE(symbol, datetime)
        )
    )")) {
        m_lastError = q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_ohlcv_sym_dt ON ohlcv(symbol, datetime)");

    // Pricing cache 表
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS pricing_cache (
            key         TEXT PRIMARY KEY,
            price       REAL NOT NULL,
            greeks_json TEXT NOT NULL,
            cached_at   INTEGER NOT NULL  -- Unix timestamp
        )
    )")) {
        m_lastError = q.lastError().text();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// insertBars
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::insertBars(const QString& symbol, const QVector<OhlcBar>& bars)
{
    if (bars.isEmpty()) return true;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    // INSERT OR REPLACE（重複 (symbol,datetime) 時覆蓋）
    q.prepare(R"(
        INSERT OR REPLACE INTO ohlcv
            (symbol, datetime, open, high, low, close, volume)
        VALUES
            (:sym, :dt, :o, :h, :l, :c, :v)
    )");

    db.transaction();
    for (const OhlcBar& bar : bars) {
        q.bindValue(":sym", symbol);
        q.bindValue(":dt",  bar.datetime.toMSecsSinceEpoch());
        q.bindValue(":o",   bar.open);
        q.bindValue(":h",   bar.high);
        q.bindValue(":l",   bar.low);
        q.bindValue(":c",   bar.close);
        q.bindValue(":v",   bar.volume);
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    db.commit();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// queryBars
// ─────────────────────────────────────────────────────────────────────────────
QVector<OhlcBar> DatabaseManager::queryBars(const QString&   symbol,
                                            const QDateTime& from,
                                            const QDateTime& to) const
{
    QVector<OhlcBar> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    q.prepare(R"(
        SELECT symbol, datetime, open, high, low, close, volume
        FROM   ohlcv
        WHERE  symbol   = :sym
          AND  datetime >= :from
          AND  datetime <= :to
        ORDER  BY datetime ASC
    )");
    q.bindValue(":sym",  symbol);
    q.bindValue(":from", from.toMSecsSinceEpoch());
    q.bindValue(":to",   to.toMSecsSinceEpoch());

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        OhlcBar bar;
        bar.symbol   = q.value(0).toString();
        bar.datetime = QDateTime::fromMSecsSinceEpoch(q.value(1).toLongLong());
        bar.open     = q.value(2).toDouble();
        bar.high     = q.value(3).toDouble();
        bar.low      = q.value(4).toDouble();
        bar.close    = q.value(5).toDouble();
        bar.volume   = q.value(6).toLongLong();
        result.append(bar);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// deleteBars / availableSymbols / latestBarTime
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::deleteBars(const QString& symbol)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("DELETE FROM ohlcv WHERE symbol = :sym");
    q.bindValue(":sym", symbol);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

QStringList DatabaseManager::availableSymbols() const
{
    QStringList list;
    QSqlQuery q("SELECT DISTINCT symbol FROM ohlcv ORDER BY symbol",
                QSqlDatabase::database(m_connectionName));
    while (q.next()) list << q.value(0).toString();
    return list;
}

std::optional<QDateTime> DatabaseManager::latestBarTime(const QString& symbol) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("SELECT MAX(datetime) FROM ohlcv WHERE symbol = :sym");
    q.bindValue(":sym", symbol);
    if (!q.exec() || !q.next() || q.value(0).isNull()) return std::nullopt;
    return QDateTime::fromMSecsSinceEpoch(q.value(0).toLongLong());
}

// ─────────────────────────────────────────────────────────────────────────────
// Pricing cache
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::savePricingCache(const QString& key, double price,
                                       const QString& greeksJson)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(R"(
        INSERT OR REPLACE INTO pricing_cache (key, price, greeks_json, cached_at)
        VALUES (:key, :price, :json, :ts)
    )");
    q.bindValue(":key",   key);
    q.bindValue(":price", price);
    q.bindValue(":json",  greeksJson);
    q.bindValue(":ts",    QDateTime::currentMSecsSinceEpoch());
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

bool DatabaseManager::loadPricingCache(const QString& key,
                                       double& price, QString& greeksJson) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare("SELECT price, greeks_json FROM pricing_cache WHERE key = :key");
    q.bindValue(":key", key);
    if (!q.exec() || !q.next()) return false;
    price      = q.value(0).toDouble();
    greeksJson = q.value(1).toString();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 維護
// ─────────────────────────────────────────────────────────────────────────────
bool DatabaseManager::vacuum()
{
    return QSqlQuery(QSqlDatabase::database(m_connectionName)).exec("VACUUM");
}

qint64 DatabaseManager::dbSizeBytes() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    return QFileInfo(db.databaseName()).size();
}

QString DatabaseManager::lastError() const { return m_lastError; }
