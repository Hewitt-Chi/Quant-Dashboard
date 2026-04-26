#pragma once
#include <QtCharts/QChartView>
#include <QMenu>
#include <QMouseEvent>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QPixmap>
#include <QFont>

class ChartContextMenu : public QObject
{
 //   Q_OBJECT
public:
    static void install(QChartView* view, const QString& defaultName = "chart")
    {
        auto* h = new ChartContextMenu(view, defaultName, view);
        view->installEventFilter(h);
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::RightButton) {
                auto* view = qobject_cast<QChartView*>(obj);
                if (view) {
                    showMenu(view, me->globalPosition().toPoint());
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    explicit ChartContextMenu(QChartView*, const QString& name, QObject* parent)
        : QObject(parent), m_name(name) {
    }

    void showMenu(QChartView* view, const QPoint& pos)
    {
        QMenu menu(view);
        menu.setStyleSheet(
            "QMenu{background:#1e1e2e;border:1px solid #555;color:#cdd6f4;"
            "padding:4px;border-radius:6px;}"
            "QMenu::item{padding:6px 20px;border-radius:4px;}"
            "QMenu::item:selected{background:#313244;}"
            "QMenu::separator{height:1px;background:#444;margin:4px 8px;}");

        auto* saveAct = menu.addAction("Save as PNG...");
        auto* copyAct = menu.addAction("Copy to clipboard");
        menu.addSeparator();
        auto* infoAct = menu.addAction(m_name);
        infoAct->setEnabled(false);

        auto* chosen = menu.exec(pos);
        if (chosen == saveAct) {
            QString path = QFileDialog::getSaveFileName(
                view, "Save chart", m_name + ".png",
                "PNG (*.png);;All files (*)");
            if (!path.isEmpty())
                view->grab().save(path);
        }
        else if (chosen == copyAct) {
            QApplication::clipboard()->setPixmap(view->grab());
        }
    }

    QString m_name;
};