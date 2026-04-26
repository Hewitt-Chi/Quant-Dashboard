#include "WatchlistWidget.h"
#include "../infra/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QTimer>
#include <QScrollArea>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QTimer>
#include <QSplitter>
#include <QDateTime>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

// ═════════════════════════════════════════════════════════════════════════════
// QuoteCard
// ═════════════════════════════════════════════════════════════════════════════
QuoteCard::QuoteCard(const QString& symbol, QWidget* parent)
    : QWidget(parent), m_symbol(symbol)
{
    setMinimumHeight(130);
    setStyleSheet(
        "QuoteCard{"
        "  background:rgba(128,128,128,0.1);"
        "  border:1px solid rgba(128,128,128,0.25);"
        "  border-radius:10px;"
        "}");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(16);

    // ── 左側：文字資訊 ────────────────────────────────────────────────────────
    auto* info = new QVBoxLayout;
    info->setSpacing(4);

    m_symLabel = new QLabel(symbol, this);
    m_symLabel->setStyleSheet("font-size:18px;font-weight:500;");

    m_priceLabel = new QLabel("—", this);
    m_priceLabel->setStyleSheet("font-size:26px;font-weight:500;color:#f59e0b;");

    m_changeLabel = new QLabel("—", this);
    m_changeLabel->setStyleSheet("font-size:13px;");

    m_volumeLabel = new QLabel("Vol: —", this);
    m_volumeLabel->setStyleSheet("font-size:11px;color:rgba(180,180,180,0.6);");

    m_timeLabel = new QLabel("", this);
    m_timeLabel->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.5);");

    info->addWidget(m_symLabel);
    info->addWidget(m_priceLabel);
    info->addWidget(m_changeLabel);
    info->addWidget(m_volumeLabel);
    info->addWidget(m_timeLabel);
    info->addStretch();

    root->addLayout(info, 0);

    // ── 右側：Sparkline ───────────────────────────────────────────────────────
    m_series = new QLineSeries;
    QPen pen(QColor("#3b82f6")); pen.setWidth(1);
    m_series->setPen(pen);

    m_chart = new QChart;
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(0,0,0,0));
    m_chart->setBackgroundVisible(false);
    m_chart->setPlotAreaBackgroundVisible(false);

    // 隱藏軸（只顯示曲線形狀）
    auto* axX = new QValueAxis; axX->setVisible(false);
    auto* axY = new QValueAxis; axY->setVisible(false);
    m_chart->addAxis(axX, Qt::AlignBottom);
    m_chart->addAxis(axY, Qt::AlignLeft);
    m_series->attachAxis(axX);
    m_series->attachAxis(axY);

    m_view = new QChartView(m_chart, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setFixedSize(180, 80);
    m_view->setStyleSheet("background:transparent;border:none;");

    root->addWidget(m_view, 1, Qt::AlignRight | Qt::AlignVCenter);
}

void QuoteCard::setAlertLevels(double upper, double lower)
{
    m_alertUpper = upper;
    m_alertLower = lower;
    m_alertFired = false;

    // 在 symLabel 旁邊顯示小鈴鐺圖示
    bool hasAlert = (upper > 0 || lower > 0);
    QString alertStr = hasAlert ? " 🔔" : "";
    m_symLabel->setText(m_symbol + alertStr);
}

void QuoteCard::clearAlerts()
{
    m_alertUpper = m_alertLower = 0.0;
    m_alertFired = false;
    m_symLabel->setText(m_symbol);
}

void QuoteCard::updateQuote(const Quote& q)
{
    m_priceLabel->setText(QString::number(q.price, 'f', 2));

    // 價格警示檢查
    if (!m_alertFired) {
        if (m_alertUpper > 0 && q.price >= m_alertUpper) {
            m_alertFired = true;
            // 卡片邊框閃紅
            setStyleSheet("QuoteCard{background:rgba(239,68,68,0.15);"
                          "border:2px solid #ef4444;border-radius:10px;}");
            emit priceAlert(m_symbol, q.price, "above");
        } else if (m_alertLower > 0 && q.price <= m_alertLower) {
            m_alertFired = true;
            setStyleSheet("QuoteCard{background:rgba(239,68,68,0.15);"
                          "border:2px solid #ef4444;border-radius:10px;}");
            emit priceAlert(m_symbol, q.price, "below");
        }
    }

    bool up = q.change >= 0;
    QString color = up ? "#22c55e" : "#ef4444";
    QString sign  = up ? "+" : "";
    m_changeLabel->setText(
        QString("<span style='color:%1'>%2%3  (%4%5%)</span>")
        .arg(color)
        .arg(sign).arg(q.change, 0,'f',2)
        .arg(sign).arg(q.changePct, 0,'f',2));
    m_changeLabel->setTextFormat(Qt::RichText);

    if (q.volume > 0) {
        double vol = q.volume;
        QString volStr = vol > 1e6
            ? QString::number(vol/1e6,'f',1)+"M"
            : QString::number(vol/1e3,'f',0)+"K";
        m_volumeLabel->setText("Vol: " + volStr);
    }
    m_timeLabel->setText(q.timestamp.toString("hh:mm:ss"));
}

void QuoteCard::updateSparkline(const QVector<OhlcBar>& bars)
{
    if (bars.isEmpty()) return;

    QVector<QPointF> pts;
    pts.reserve(bars.size());
    double yMin = bars[0].close, yMax = bars[0].close;
    for (int i = 0; i < bars.size(); ++i) {
        pts.append({static_cast<double>(i), bars[i].close});
        yMin = qMin(yMin, bars[i].close);
        yMax = qMax(yMax, bars[i].close);
    }
    m_series->replace(pts);

    auto axX = m_chart->axes(Qt::Horizontal);
    auto axY = m_chart->axes(Qt::Vertical);
    if (!axX.isEmpty()) qobject_cast<QValueAxis*>(axX.first())
        ->setRange(0, pts.size()-1);
    if (!axY.isEmpty()) {
        double margin = (yMax-yMin) * 0.1;
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(yMin-margin, yMax+margin);
    }

    // 根據漲跌改顏色
    bool up = bars.last().close >= bars.first().close;
    QPen pen(QColor(up ? "#22c55e" : "#ef4444")); pen.setWidth(1);
    m_series->setPen(pen);
}

// ═════════════════════════════════════════════════════════════════════════════
// WatchlistWidget
// ═════════════════════════════════════════════════════════════════════════════
WatchlistWidget::WatchlistWidget(QuoteFetcher*    fetcher,
                                 DatabaseManager* db,
                                 QWidget*         parent)
    : QWidget(parent), m_fetcher(fetcher), m_db(db)
{
    buildUi();

    connect(m_fetcher, &QuoteFetcher::quoteReceived,
            this, &WatchlistWidget::onQuoteReceived);
    connect(m_fetcher, &QuoteFetcher::historyReceived,
            this, &WatchlistWidget::onHistoryReceived);

    // 倒數計時器（1 秒 tick）
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout,
            this, &WatchlistWidget::onCountdownTick);
    m_countdownTimer->start();

    // 初始化：從 DB 載入 sparkline
    loadSparklines();
    refreshDbStats();
}

void WatchlistWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    // ── 控制列 ────────────────────────────────────────────────────────────────
    auto* ctrlRow = new QHBoxLayout;
    m_symInput = new QLineEdit(this);
    m_symInput->setPlaceholderText("Add symbol (e.g. QQQ)");
    m_symInput->setMaximumWidth(180);

    auto* addBtn = new QPushButton("Add", this);
    addBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;padding:4px 12px;}"
        "QPushButton:hover{background:#1d4ed8;}");
    connect(addBtn, &QPushButton::clicked, this, &WatchlistWidget::onAddSymbol);
    connect(m_symInput, &QLineEdit::returnPressed,
            this, &WatchlistWidget::onAddSymbol);

    m_refreshBtn = new QPushButton("Refresh now", this);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &WatchlistWidget::onRefreshClicked);

    m_countdownLbl = new QLabel("Next refresh: —", this);
    m_countdownLbl->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");

    ctrlRow->addWidget(new QLabel("Watch:", this));
    ctrlRow->addWidget(m_symInput);
    ctrlRow->addWidget(addBtn);
    ctrlRow->addSpacing(16);
    ctrlRow->addWidget(m_refreshBtn);
    ctrlRow->addWidget(m_countdownLbl);
    ctrlRow->addStretch();
    root->addLayout(ctrlRow);

    // ── 主分割：左側報價卡 + 右側 DB 狀態 ────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // 左：報價卡 scroll area
    auto* scrollW = new QWidget;
    auto* scrollLay = new QVBoxLayout(scrollW);
    scrollLay->setContentsMargins(0,0,0,0);
    scrollLay->setSpacing(8);

    m_cardsContainer = new QWidget(scrollW);
    auto* cardsLay = new QVBoxLayout(m_cardsContainer);
    cardsLay->setContentsMargins(0,0,0,0);
    cardsLay->setSpacing(8);

    // 從 AppSettings 讀取持久化的 symbol 清單
    const QStringList savedSyms = AppSettings::instance().watchlist();
    for (const auto& sym : savedSyms) {
        auto* card = new QuoteCard(sym, m_cardsContainer);
        m_cards[sym] = card;
        cardsLay->addWidget(card);
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QWidget::customContextMenuRequested, this,
                [this, sym](const QPoint&) { onSetAlert(sym); });
        connect(card, &QuoteCard::priceAlert,
                this, &WatchlistWidget::onAlertTriggered);
    }
    cardsLay->addStretch();

    auto* sa = new QScrollArea(this);
    sa->setWidget(m_cardsContainer);
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    splitter->addWidget(sa);

    // 右：DB 狀態
    auto* dbBox = new QGroupBox("DB cache", this);
    auto* dbLay = new QVBoxLayout(dbBox);

    m_dbTable = new QTableWidget(0, 4, dbBox);
    m_dbTable->setHorizontalHeaderLabels({"Symbol","Bars","Latest","Size"});
    m_dbTable->horizontalHeader()->setStretchLastSection(true);
    m_dbTable->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_dbTable->verticalHeader()->setVisible(false);
    m_dbTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dbTable->setAlternatingRowColors(true);
    m_dbTable->setShowGrid(false);
    m_dbTable->setMaximumWidth(380);
    dbLay->addWidget(m_dbTable);

    auto* vacuumBtn = new QPushButton("Vacuum DB", dbBox);
    connect(vacuumBtn, &QPushButton::clicked, [this]{
        m_db->vacuum();
        refreshDbStats();
        emit statusMessage("DB vacuumed. Size: "
            + QString::number(m_db->dbSizeBytes()/1024) + " KB");
    });
    dbLay->addWidget(vacuumBtn);
    splitter->addWidget(dbBox);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void WatchlistWidget::onQuoteReceived(const Quote& q)
{
    if (m_cards.contains(q.symbol))
        m_cards[q.symbol]->updateQuote(q);

    // 重設倒數
    m_countdown = AppSettings::instance().quoteRefreshSec();
    refreshDbStats();
}

void WatchlistWidget::onHistoryReceived(const QString& symbol,
                                        const QVector<OhlcBar>& bars)
{
    if (m_cards.contains(symbol))
        m_cards[symbol]->updateSparkline(bars.mid(
            qMax(0, bars.size()-60)));  // 最近 60 根
    refreshDbStats();
}

void WatchlistWidget::onRefreshClicked()
{
    QStringList syms;
    for (const auto& sym : m_cards.keys()) syms << sym;
    m_fetcher->fetchQuotes(syms);
    emit statusMessage("Refreshing: " + syms.join(", "));
}

void WatchlistWidget::onAddSymbol()
{
    QString sym = m_symInput->text().trimmed().toUpper();
    if (sym.isEmpty() || m_cards.contains(sym)) return;

    auto* card = new QuoteCard(sym, m_cardsContainer);
    m_cards[sym] = card;
    qobject_cast<QVBoxLayout*>(m_cardsContainer->layout())
        ->insertWidget(m_cards.size()-1, card);
    card->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(card, &QWidget::customContextMenuRequested, this,
            [this, sym](const QPoint&) { onSetAlert(sym); });
    connect(card, &QuoteCard::priceAlert,
            this, &WatchlistWidget::onAlertTriggered);
    m_symInput->clear();

    // 持久化到 AppSettings
    AppSettings::instance().addWatchlistSymbol(sym);

    m_fetcher->fetchHistory(sym, "1y");
    m_fetcher->fetchQuotes({sym});
    emit statusMessage("Added " + sym + " (saved to watchlist), fetching data...");
}

void WatchlistWidget::onCountdownTick()
{
    if (m_countdown > 0) {
        m_countdown--;
        m_countdownLbl->setText(
            QString("Next refresh: %1s").arg(m_countdown));
    } else {
        m_countdownLbl->setText("Refreshing...");
    }
}

void WatchlistWidget::loadSparklines()
{
    QDateTime from = QDateTime::currentDateTime().addDays(-60);
    QDateTime to   = QDateTime::currentDateTime();
    for (auto& sym : m_cards.keys()) {
        auto bars = m_db->queryBars(sym, from, to);
        if (!bars.isEmpty())
            m_cards[sym]->updateSparkline(bars);
    }
    m_countdown = AppSettings::instance().quoteRefreshSec();
}

void WatchlistWidget::refreshDbStats()
{
    auto syms = m_db->availableSymbols();
    m_dbTable->setRowCount(syms.size());

    QDateTime from(QDate(2000,1,1), QTime(0,0));
    QDateTime to = QDateTime::currentDateTime();

    for (int i = 0; i < syms.size(); ++i) {
        auto bars    = m_db->queryBars(syms[i], from, to);
        auto latest  = m_db->latestBarTime(syms[i]);
        QString latestStr = latest
            ? latest->toString("MM/dd hh:mm")
            : "—";

        auto set = [&](int col, const QString& txt) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            m_dbTable->setItem(i, col, it);
        };
        set(0, syms[i]);
        set(1, QString::number(bars.size()));
        set(2, latestStr);
        set(3, QString::number(m_db->dbSizeBytes()/1024) + " KB");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 右鍵設定價格警示
// ─────────────────────────────────────────────────────────────────────────────
void WatchlistWidget::onSetAlert(const QString& symbol)
{
    auto* card = m_cards.value(symbol, nullptr);
    if (!card) return;

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Price alert — %1").arg(symbol));
    dlg->setMinimumWidth(300);

    auto* form = new QFormLayout(dlg);

    auto* upperSpin = new QDoubleSpinBox(dlg);
    upperSpin->setRange(0, 999999); upperSpin->setValue(card->alertUpper());
    upperSpin->setDecimals(2); upperSpin->setSpecialValueText("Disabled");
    upperSpin->setSuffix("  (alert when price ≥ this)");
    upperSpin->setMinimumWidth(200);
    form->addRow("Upper alert:", upperSpin);

    auto* lowerSpin = new QDoubleSpinBox(dlg);
    lowerSpin->setRange(0, 999999); lowerSpin->setValue(card->alertLower());
    lowerSpin->setDecimals(2); lowerSpin->setSpecialValueText("Disabled");
    lowerSpin->setSuffix("  (alert when price ≤ this)");
    lowerSpin->setMinimumWidth(200);
    form->addRow("Lower alert:", lowerSpin);

    auto* hint = new QLabel("Set to 0 to disable. Right-click card to change.", dlg);
    hint->setStyleSheet("font-size:10px;color:gray;");
    form->addRow("", hint);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    auto* clearBtn = btns->addButton("Clear alerts", QDialogButtonBox::ResetRole);
    form->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(clearBtn, &QPushButton::clicked, [card, dlg]{
        card->clearAlerts(); dlg->accept();
    });

    if (dlg->exec() == QDialog::Accepted) {
        double upper = upperSpin->value();
        double lower = lowerSpin->value();
        card->setAlertLevels(upper, lower);

        QString msg;
        if (upper > 0 && lower > 0)
            msg = QString("%1 alert set: above %2, below %3")
                  .arg(symbol).arg(upper,'f').arg(lower,'f');
        else if (upper > 0)
            msg = QString("%1 alert set: above %2").arg(symbol).arg(upper,'f');
        else if (lower > 0)
            msg = QString("%1 alert set: below %2").arg(symbol).arg(lower,'f');
        else
            msg = QString("%1 alerts cleared").arg(symbol);
        emit statusMessage(msg);
    }
    dlg->deleteLater();
}

// ─────────────────────────────────────────────────────────────────────────────
// 警示觸發：status bar 閃紅三次
// ─────────────────────────────────────────────────────────────────────────────
void WatchlistWidget::onAlertTriggered(const QString& symbol,
                                        double price,
                                        const QString& dir)
{
    QString msg = QString("🔔 ALERT: %1 is %2 target — Price: %3")
                  .arg(symbol)
                  .arg(dir == "above" ? "ABOVE" : "BELOW")
                  .arg(price, 0, 'f', 2);

    emit statusMessage(msg);

    // 閃爍 3 次（用 QTimer）
    for (int i = 0; i < 3; ++i) {
        QTimer::singleShot(i * 600, this, [this, msg, i]{
            emit statusMessage(i % 2 == 0 ? msg : "");
        });
    }
    QTimer::singleShot(1800, this, [this, msg]{
        emit statusMessage(msg + "  [click to dismiss]");
    });
}
