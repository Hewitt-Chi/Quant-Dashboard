#include "QuantMainDlg.h"
#include <QtWidgets/QApplication>
// QuantLib headers
#include <ql/quantlib.hpp>

int main(int argc, char *argv[])
{
    using namespace QuantLib;

    // ── 簡單測試：歐式選擇權定價 ──────────────────────────────────────────
    Calendar calendar = TARGET();
    Date settlementDate(18, September, 2015);
    Settings::instance().evaluationDate() = settlementDate;

    Option::Type type(Option::Call);
    Real underlying = 36;
    Real strike = 40;
    Spread dividendYield = 0.00;
    Rate riskFreeRate = 0.06;
    Volatility volatility = 0.20;
    Date maturity(17, December, 2015);
    DayCounter dayCounter = Actual365Fixed();

    // （詳細定價程式碼略）

    QApplication app(argc, argv);
    QuantMainDlg w;
    w.show();
    return app.exec();
}
