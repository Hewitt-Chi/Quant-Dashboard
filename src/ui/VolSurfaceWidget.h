#pragma once
#include <QWidget>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QComboBox;
QT_END_NAMESPACE

// Qt DataVisualization — 需要安裝 Qt DataVisualization 模組 + Vulkan SDK
// 安裝：Qt Maintenance Tool → Qt 6.x → Qt Data Visualization
// Vulkan：https://vulkan.lunarg.com/sdk/home
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QValue3DAxis>

// ─────────────────────────────────────────────────────────────────────────────
// VolSurfaceWidget
//
// 顯示 Implied Vol 曲面（Strike × Maturity 二維網格）
// 使用 Qt DataVisualization Q3DSurface 做 3D 渲染
//
// 功能：
//   - 可調整 Spot / Risk-free / Base vol 參數
//   - 掃描 13 個 Strike × 10 個 Maturity = 130 個點
//   - 顏色映射：低 IV = 藍色，高 IV = 紅色（類 heatmap）
//   - 可滑鼠旋轉 / 縮放 3D 視角
// ─────────────────────────────────────────────────────────────────────────────
class VolSurfaceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VolSurfaceWidget(AsyncWorker* worker, QWidget* parent = nullptr);
    ~VolSurfaceWidget();

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCalculate();
    void onVolSurfaceFinished(const VolSurfaceResult& result);

private:
    QWidget* buildParamsPanel();
    QWidget* build3DSurface();

    AsyncWorker* m_worker = nullptr;

    // Params
    QDoubleSpinBox* m_spot   = nullptr;
    QDoubleSpinBox* m_rf     = nullptr;
    QDoubleSpinBox* m_vol    = nullptr;
    QPushButton*    m_calcBtn= nullptr;

    // 3D Surface
    Q3DSurface*          m_surface    = nullptr;
    QSurface3DSeries*    m_series     = nullptr;
    QSurfaceDataProxy*   m_proxy      = nullptr;
    QWidget*             m_container  = nullptr;   // Q3DSurface 的 QWidget 包裝
};
