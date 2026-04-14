#include <QApplication>
#include "ui/QuantMainDlg.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Quant-Dashboard");
    app.setOrganizationName("Quant-Dashboard");
    app.setStyle("Fusion");

    QuantMainDlg w;
    w.show();
    return app.exec();
}
