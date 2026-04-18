#pragma once
#include <QWidget>
#include <QMap>
#include "../infra/QuoteFetcher.h"
#include "../infra/DatabaseManager.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTimer;
class QTableWidget;
QT_END_NAMESPACE

class QChart;
class QChartView;
class QLineSeries;

// ─────────────────────────────────────────────────────────────────────────────
// QuoteCard  —  單一 symbol 的報價卡（含 mini sparkline）
// ─────────────────────────────────────────────────────────────────────────────
class QuoteCard : public QWidget
{
    Q_OBJECT
public:
    explicit QuoteCard(const QString& symbol, QWidget* parent = nullptr);

    void updateQuote(const Quote& q);
    void updateSparkline(const QVector<OhlcBar>& bars);  // 最近 30 根 K 線
    QString symbol() const { return m_symbol; }

private:
    QString     m_symbol;
    QLabel*     m_symLabel    = nullptr;
    QLabel*     m_priceLabel  = nullptr;
    QLabel*     m_changeLabel = nullptr;
    QLabel*     m_volumeLabel = nullptr;
    QLabel*     m_timeLabel   = nullptr;

    // Mini sparkline
    QChart*      m_chart  = nullptr;
    QChartView*  m_view   = nullptr;
    QLineSeries* m_series = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// WatchlistWidget
// ─────────────────────────────────────────────────────────────────────────────
class WatchlistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WatchlistWidget(QuoteFetcher*    fetcher,
                             DatabaseManager* db,
                             QWidget*         parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onQuoteReceived(const Quote& q);
    void onHistoryReceived(const QString& symbol, const QVector<OhlcBar>& bars);
    void onRefreshClicked();
    void onAddSymbol();
    void onCountdownTick();

private:
    void buildUi();
    void refreshDbStats();
    void loadSparklines();

    QuoteFetcher*    m_fetcher = nullptr;
    DatabaseManager* m_db      = nullptr;

    // 報價卡（symbol → card）
    QMap<QString, QuoteCard*> m_cards;
    QWidget*    m_cardsContainer = nullptr;

    // DB 狀態表
    QTableWidget* m_dbTable     = nullptr;

    // 控制列
    QPushButton* m_refreshBtn   = nullptr;
    QLabel*      m_countdownLbl = nullptr;
    QTimer*      m_countdownTimer = nullptr;
    int          m_countdown    = 0;

    // 新增 Symbol 輸入
    class QLineEdit* m_symInput = nullptr;
};
