#include <QApplication>
#include <QDebug>

#include "infra/AppSettings.h"
#include "infra/AsyncWorker.h"
#include "infra/DatabaseManager.h"
#include "infra/QuoteFetcher.h"

#include <QApplication>
#include "ui/QuantMainDlg.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QuantDashboard");
    app.setOrganizationName("QuantDashboard");
    app.setStyle("Fusion");

    QuantMainDlg w;
    w.show();
    return app.exec();
  //  return 1;
}
