# QuantLib-QT
# QuantLib Qt 專案建置指南

> **環境目標：** Visual Studio 2022 + Qt Visual Studio Tools + CMake + vcpkg  
> **跨平台可複製性：** 任何人 clone 此 repo 後只需執行少數幾個指令即可完成建置  
> **QuantLib / Boost 自動管理：** 透過 vcpkg manifest mode，缺少套件時自動下載安裝

---

## 目錄

1. [架構概覽](#1-架構概覽)
2. [前置需求（一次性安裝）](#2-前置需求一次性安裝)
3. [專案目錄結構](#3-專案目錄結構)
4. [建立新專案的完整步驟](#4-建立新專案的完整步驟)
5. [vcpkg 設定](#5-vcpkg-設定)
6. [CMakeLists.txt 設定](#6-cmakeliststxt-設定)
7. [CMakePresets.json 設定](#7-cmakepresetsjson-設定)
8. [原始碼檔案範例](#8-原始碼檔案範例)
9. [Visual Studio 2022 開啟與建置](#9-visual-studio-2022-開啟與建置)
10. [GitHub 上傳與 .gitignore](#10-github-上傳與-gitignore)
11. [新電腦 Clone 後的完整建置流程](#11-新電腦-clone-後的完整建置流程)
12. [常見錯誤與解決方式](#12-常見錯誤與解決方式)

---

## 1. 架構概覽

```
本專案採用以下技術棧：

  ┌─────────────────────────────────────────────────┐
  │              Visual Studio 2022                 │
  │         (Qt Visual Studio Tools 已安裝)          │
  └────────────────────┬────────────────────────────┘
                       │ 開啟 CMake 專案
  ┌────────────────────▼────────────────────────────┐
  │                   CMake                         │
  │  • CMakeLists.txt 定義建置目標                   │
  │  • CMakePresets.json 定義建置參數                │
  └──────────┬──────────────────┬───────────────────┘
             │                  │
  ┌──────────▼──────┐  ┌────────▼────────────────────┐
  │     vcpkg       │  │          Qt6                │
  │  (git submodule)│  │  (透過 CMAKE_PREFIX_PATH)    │
  │  • quantlib     │  └─────────────────────────────┘
  │    (含 boost)   │
  └─────────────────┘
```

**重要觀念：QuantLib 與你的專案的關係**

QuantLib 對你的專案來說，跟 Qt 一樣，**只是一個相依函式庫**，不是你的程式碼的一部分。
不需要把 QuantLib 的原始碼或 CMakeLists.txt 放進你的專案，也不需要修改它。

```
你的 Qt 專案（你寫的程式碼）
    │
    │  使用（#include <ql/quantlib.hpp>）
    ▼
QuantLib（由 vcpkg 自動安裝管理）
    │
    ▼
Boost（QuantLib 的相依，也由 vcpkg 自動安裝）
```

**為何選擇 CMake + vcpkg？**

- **CMake**：VS2022 原生支援，不需 .sln / .vcxproj 手動維護
- **vcpkg manifest mode**（`vcpkg.json`）：套件版本鎖定在 repo 內，clone 後自動安裝
- **git submodule**：vcpkg 本身版本也被鎖定，確保跨機器一致性
- **Boost 不需要單獨管理**：vcpkg 安裝 QuantLib 時會自動拉取所需的 Boost 子套件

---

## 2. 前置需求（一次性安裝）

每台開發機器只需安裝一次以下工具：

### 2.1 Visual Studio 2022

安裝時需勾選以下工作負載：
- ✅ **使用 C++ 的桌面開發**
- ✅ 元件：**CMake 工具**（通常預設勾選）
- ✅ 元件：**Git for Windows**（或另行安裝 Git）

### 2.2 Qt 安裝

前往 [https://www.qt.io/download](https://www.qt.io/download) 下載 Qt Online Installer，建議安裝：
- Qt 6.7.x 以上 → 勾選 **MSVC 2022 64-bit**

安裝完成後，記下 Qt 的安裝路徑，例如：
```
C:\Qt\6.9.3\msvc2022_64
```
這個路徑在後面的 `CMakePresets.json` 中會用到。

### 2.3 Qt Visual Studio Tools

在 VS2022 中安裝：
`延伸模組` → `管理延伸模組` → 搜尋 **Qt Visual Studio Tools** → 安裝

安裝後至 `Extensions` → `Qt VS Tools` → `Qt Versions` 新增 Qt 路徑。

---

## 3. 專案目錄結構

```
QuantLib-QT/                          ← 專案根目錄（GitHub repo 根）
│
├── .gitmodules                        ← git submodule 設定（自動產生）
├── .gitignore
├── CMakeLists.txt                     ← 主要建置設定
├── CMakePresets.json                  ← VS2022 建置 preset
├── CMakeUserPresets.json              ← 本機 Qt 路徑覆寫（不提交到 git）
├── vcpkg.json                         ← vcpkg manifest（QuantLib、Boost 等）
├── vcpkg-configuration.json          ← vcpkg baseline 版本鎖定
│
├── app.rc                             ← Windows exe 圖示資源（IDI_ICON1）
├── app_icon.ico                       ← 應用程式圖示
├── resources.qrc                      ← Qt 資源檔（嵌入 icon 進 exe）
├── deploy.bat                         ← Release 一鍵打包腳本（windeployqt）
│
├── vcpkg/                             ← git submodule（vcpkg 本體）
│
└── src/                               ← 所有原始碼
    │
    ├── main.cpp                       ← 程式進入點（QApplication + MainWindow）
    │
    ├── infra/                         ← 基礎設施層（無 UI 依賴）
    │   ├── AppSettings.h/.cpp         ← QSettings 包裝器（全域設定單例）
    │   ├── AsyncWorker.h/.cpp         ← QtConcurrent 背景計算引擎
    │   │                                  PricingRequest / BacktestRequest
    │   │                                  OptionChainRequest / YieldCurveResult
    │   │                                  VolSurfaceResult / OptionChainResult
    │   ├── DatabaseManager.h/.cpp     ← SQLite OHLCV 快取（QSqlDatabase WAL）
    │   └── QuoteFetcher.h/.cpp        ← 行情抓取（Yahoo Finance / Polygon.io）
    │                                      crumb/cookie 認證 + chart fallback
    │
    └── ui/                            ← UI 層（Qt Widgets）
    ├── QuantMainDlg.h/.cpp          ← 主視窗：導覽列 + QStackedWidget
    │
    ├── WatchlistWidget.h/.cpp     ← MARKET → 即時報價 + Sparkline
    │
    ├── PricerWidget.h/.cpp        ← ANALYSIS → 選擇權定價
    │                                  BSM / Binomial / Heston
    │                                  Greeks cards + 十字準線
    │                                  Tab1: Greek vs Spot
    │                                  Tab2: Vol Smile（BSM bisection IV）
    │                                  Tab3: BSM vs Heston 對比
    │
    ├── YieldCurveWidget.h/.cpp    ← ANALYSIS → 殖利率曲線
    │                                  QuantLib PiecewiseYieldCurve bootstrap
    │                                  Spot / Zero / Forward（3M smooth）
    │                                  情境分析：平移 / 扭轉 / 蝶形
    │                                  Export CSV
    │
    ├── OptionChainWidget.h/.cpp   ← ANALYSIS → 選擇權鏈掃描
    │                                  Call/Put Price/Delta/IV × 21 Strikes
    │                                  ATM 標黃 + IV Smile 圖
    │
    ├── VolSurfaceWidget.h/.cpp    ← ANALYSIS → IV 曲面 3D（選用）
    │                                  Qt DataVisualization Q3DSurface
    │                                  需要 Qt DataVisualization + Vulkan SDK
    │
    ├── BacktestWidget.h/.cpp      ← STRATEGY → 回測引擎
    │                                  5 種策略：
    │                                    Covered Call / Protective Put
    │                                    Iron Condor / Collar / Cash-Secured Put
    │                                  PnL vs B&H + Drawdown 比較圖
    │                                  Trade log（含 assignment 標紅）
    │
    └── SettingsWidget.h/.cpp      ← SYSTEM → 全域設定
                                       Quote provider（Yahoo / Polygon）
                                       API key / Refresh 間隔
                                       Pricing 預設值 / DB 路徑
```

> **注意：** 原始碼直接放在專案根目錄（Qt Wizard 產生的預設結構），不一定需要 `src/` 子目錄。`CMakeLists.txt` 中的路徑需與實際檔案位置一致。

---

## 4. 建立新專案的完整步驟

### Step 1：在 VS2022 建立 Qt Widgets 專案

1. `檔案` → `新增` → `專案`
2. 搜尋 **Qt Widgets Application**
3. 設定專案名稱與位置
4. VS2022 會產生初始的 `.cpp`、`.h`、`.ui` 檔

### Step 2：初始化 Git repo

Qt Wizard 產生的專案**沒有** git repo，需要手動初始化：

```powershell
# 進入專案根目錄
cd MyQuantLibApp

# 初始化 git（必須先做這步，才能加 submodule）
git init
```

或在 VS2022 中：`Git` → `建立 Git 存放庫`

### Step 3：加入 vcpkg 為 git submodule

```powershell
cd E:/WorkSpace/Quant-Dashboard
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat

# Bootstrap vcpkg（產生 vcpkg.exe）
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

### Step 4：取得 vcpkg 的 commit hash（用於版本鎖定）

```powershell
git -C vcpkg rev-parse HEAD
# 範例輸出：e80346e570b767219f85dc95e3736853bfec3a70
# 把這個 hash 填入 vcpkg.json 的 builtin-baseline
```

### Step 5：建立設定檔

依照第 5、6、7 節建立 `vcpkg.json`、`CMakeLists.txt`、`CMakePresets.json`。

### Step 6：在 VS2022 開啟 CMake 專案

`檔案` → `開啟` → `CMake...` → 選取 `CMakeLists.txt`

> ⚠️ **如果專案原本是用 Qt Wizard 建立的（.vcxproj 格式），需要刪除舊的專案檔後，重新以 CMake 方式開啟。**

---

## 5. vcpkg 設定

### 5.1 vcpkg.json（Manifest 檔）

在專案根目錄建立 `vcpkg.json`：

```json
{
  "name": "my-quantlib-app",
  "version": "1.0.0",
  "dependencies": [
    "quantlib"
  ],
  "builtin-baseline": "填入你的 git -C vcpkg rev-parse HEAD 輸出"
}
```

**重要說明：**
- **不需要單獨列出 `boost`**：vcpkg 安裝 `quantlib` 時會自動安裝所有需要的 Boost 子套件
- **不要加 `$schema` 欄位**：VS2022 有時無法連線下載 schema 而產生警告（無害但干擾）
- **`builtin-baseline` 必須是真實的 hash**：不能使用範例中的假 hash，需執行上方指令取得

### 5.2 vcpkg-configuration.json

```json
{
  "default-registry": {
    "kind": "git",
    "baseline": "填入與 vcpkg.json 相同的 hash",
    "repository": "https://github.com/microsoft/vcpkg"
  },
  "registries": []
}
```

---

## 6. CMakeLists.txt 設定

以下是**完整且正確**的 `CMakeLists.txt`，整合了所有已知問題的修正：

```cmake
# ── 必須是檔案的第一行有效程式碼 ──────────────────────────────────────────
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

cmake_minimum_required(VERSION 3.25)

message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "VCPKG_INSTALLED_DIR = ${VCPKG_INSTALLED_DIR}")

# ── 專案名稱與語言 ─────────────────────────────────────────────────────────────
project(MyQuantLibApp LANGUAGES CXX)

# 讓 MSVC runtime 與 vcpkg x64-windows-static 一致
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# Example CMakePresets.json:
# {
#   "version": 3,
#   "configurePresets": [{
#     "name": "default",
#     "cacheVariables": {
#       "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
#     }
#   }]
# }

# ── Qt 設定 ───────────────────────────────────────────────────────────────────
# 啟用 Qt 自動化工具
set(CMAKE_AUTOMOC ON)    # 自動處理 Q_OBJECT
set(CMAKE_AUTORCC ON)    # 自動處理 .qrc 資源檔
set(CMAKE_AUTOUIC ON)    # 自動處理 .ui 檔

# 尋找 Qt6（需設定 CMAKE_PREFIX_PATH 或環境變數 Qt6_DIR）
find_package(Qt6 COMPONENTS Core Widgets Charts Network Sql Concurrent REQUIRED)

# ── Boost（由 vcpkg 管理，缺少時自動安裝）───────────────────────────────────
#find_package(Boost REQUIRED COMPONENTS date_time math_c99)

# ── QuantLib（由 vcpkg 管理）─────────────────────────────────────────────────
find_package(QuantLib CONFIG REQUIRED)

# ── 原始碼 ────────────────────────────────────────────────────────────────────
# ── Sources ───────────────────────────────────────────────────────────────────
set(INFRA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/AppSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/AppSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/AsyncWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/AsyncWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/DatabaseManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/DatabaseManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/QuoteFetcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/infra/QuoteFetcher.cpp
)

set(UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/QuantMainDlg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/QuantMainDlg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PricerWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PricerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PricerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/WatchlistWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/WatchlistWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/BacktestWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/BacktestWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/YieldCurveWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/YieldCurveWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/OptionChainWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/OptionChainWidget.cpp
)

if(WIN32)
    set(PLATFORM_SOURCES app.rc)
endif()

# ── 建置目標 ──────────────────────────────────────────────────────────────────
qt_add_executable(Quant-Dashboard
    src/main.cpp
    ${INFRA_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
    resources.qrc  
)

target_link_libraries(Quant-Dashboard PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network      # ← 確認這行存在
    Qt6::Sql
    Qt6::Concurrent
    QuantLib::QuantLib
)

if(MSVC)
    target_compile_options(Quant-Dashboard PRIVATE /utf-8)
endif()

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT)
    add_custom_command(TARGET Quant-Dashboard POST_BUILD
        COMMAND ${WINDEPLOYQT}
            --no-translations
            "$<TARGET_FILE:Quant-Dashboard>"
        COMMENT "Deploying Qt DLLs..."
    )
endif()
```

**各設定的說明：**

| 設定 | 原因 |
|------|------|
| `CMAKE_TOOLCHAIN_FILE` 在 `project()` 前 | CMake 在初始化編譯器時就需要 vcpkg，晚設定無效 |
| `VCPKG_TARGET_TRIPLET = x64-windows-static` | QuantLib 在 vcpkg 不支援動態連結（x64-windows） |
| `cmake_policy(SET CMP0091 NEW)` | 讓 `CMAKE_MSVC_RUNTIME_LIBRARY` 變數生效 |
| `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded...` | 靜態 runtime 必須與 QuantLib 的靜態 triplet 一致，否則 LNK2038 |
| `/utf-8` | 避免含中文註解的原始碼產生 C4828 警告 |
| `CMAKE_CURRENT_SOURCE_DIR` 前綴 | 明確指定來源目錄，避免 CMake 在 build 目錄找原始碼 |
| 不需要 `find_package(Boost)` | Boost 透過 QuantLib 的相依關係自動安裝，不需要單獨引用 |

---

## 7. CMakePresets.json 設定

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-msvc2022-debug",
      "displayName": "Windows MSVC 2022 - Debug",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/debug",
      "toolchainFile": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.9.3/msvc2022_64",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static"
      }
    },
    {
      "name": "windows-msvc2022-release",
      "displayName": "Windows MSVC 2022 - Release",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/release",
      "toolchainFile": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.9.3/msvc2022_64",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "build-debug",
      "configurePreset": "windows-msvc2022-debug"
    },
    {
      "name": "build-release",
      "configurePreset": "windows-msvc2022-release"
    }
  ]
}
```

**重要：**
- **Generator 必須用 `"Visual Studio 17 2022"`**，不可用 `"Ninja"`。Ninja 在 VS2022 環境下會找不到 MSVC 編譯器（`CMAKE_CXX_COMPILER not set`）
- `CMAKE_PREFIX_PATH` 填入你的 Qt 實際安裝路徑（用正斜線 `/`）
- `VCPKG_TARGET_TRIPLET` 必須是 `x64-windows-static`

Qt 路徑若與他人不同，可用 `CMakeUserPresets.json` 本機覆寫（見第 10 節）。

---

## 8. 原始碼檔案範例

### main.cpp

```cpp
#include "QuantLibQT.h"
#include <QtWidgets/QApplication>
#include <ql/quantlib.hpp>

int main(int argc, char *argv[])
{
    using namespace QuantLib;

    // QuantLib 基本測試：歐式選擇權定價參數
    Calendar calendar = TARGET();
    Date settlementDate(18, September, 2015);
    Settings::instance().evaluationDate() = settlementDate;

    Option::Type type(Option::Call);
    Real underlying      = 36;
    Real strike          = 40;
    Spread dividendYield = 0.00;
    Rate riskFreeRate    = 0.06;
    Volatility volatility = 0.20;
    Date maturity(17, December, 2015);
    DayCounter dayCounter = Actual365Fixed();

    QApplication app(argc, argv);
    QuantLibQT w;
    w.show();
    return app.exec();
}
```

### QuantLibQT.h

```cpp
#pragma once
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class QuantLibQT; }
QT_END_NAMESPACE

class QuantLibQT : public QMainWindow
{
    Q_OBJECT

public:
    explicit QuantLibQT(QWidget *parent = nullptr);
    ~QuantLibQT();

private:
    Ui::QuantLibQT *ui;
};
```

### QuantLibQT.cpp

```cpp
#include "QuantLibQT.h"
#include "ui_QuantLibQT.h"

QuantLibQT::QuantLibQT(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::QuantLibQT)
{
    ui->setupUi(this);
}

QuantLibQT::~QuantLibQT()
{
    delete ui;
}
```

> ⚠️ `#include "..."` 中的檔案名稱必須與實際磁碟上的 `.h` / `.ui` 檔名完全一致（包含大小寫）。

---

## 9. Visual Studio 2022 開啟與建置

1. `檔案` → `開啟` → `CMake...` → 選取 `CMakeLists.txt`
2. 工具列的設定下拉選單選擇 `Windows MSVC 2022 - Debug`
3. `專案` → `刪除快取並重新設定`
4. `建置` → `全部建置`

**第一次建置時間：**
vcpkg 需要從原始碼編譯 QuantLib（含 Boost），耗時約 **30–60 分鐘**。後續建置會使用快取，速度正常。

**如果設定沒有生效：**
檢查專案根目錄是否有 `CMakeSettings.json`（舊格式），有的話直接刪除。它會覆蓋 `CMakePresets.json` 的設定。

---

## 10. GitHub 上傳與 .gitignore

### .gitignore

```gitignore
# 建置輸出
/build/
/out/

# vcpkg 安裝目錄（由 manifest 自動還原，不需提交）
/vcpkg_installed/

# VS2022 CMake 快取與舊格式設定檔
/.vs/
/CMakeSettings.json

# 使用者本機 Qt 路徑設定（不同機器路徑不同）
CMakeUserPresets.json

# 一般 C++/Windows
*.obj
*.pdb
*.ilk
*.exp
*.lib
*.exe
*.dll
*.user
```

### CMakeUserPresets.json（本機覆寫，不提交到 Git）

若其他開發者的 Qt 安裝路徑不同，在本機建立此檔案：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-msvc2022-debug",
      "inherits": "windows-msvc2022-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/Qt/6.9.3/msvc2022_64"
      }
    },
    {
      "name": "windows-msvc2022-release",
      "inherits": "windows-msvc2022-release",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/Qt/6.9.3/msvc2022_64"
      }
    }
  ]
}
```

### 提交到 GitHub

```bash
git init                          # 若尚未初始化
git add .
git commit -m "Initial commit: QuantLib Qt CMake project"
git remote add origin https://github.com/你的帳號/MyQuantLibApp.git
git push -u origin main
```

> **確認 `.gitmodules` 有被提交**，否則其他人 clone 後 `vcpkg/` 會是空目錄。

---

## 11. 新電腦 Clone 後的完整建置流程

```powershell
# Step 1：Clone repo（--recurse-submodules 一併初始化 vcpkg submodule）
git clone --recurse-submodules https://github.com/你的帳號/MyQuantLibApp.git
cd MyQuantLibApp

# Step 2：Bootstrap vcpkg（產生 vcpkg.exe）
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# Step 3：若 Qt 安裝路徑與 CMakePresets.json 不同，建立本機覆寫檔
# 依第 10 節建立 CMakeUserPresets.json

# Step 4：以 VS2022 開啟 CMakeLists.txt，選擇 preset 後建置
# 或使用 CLI：
cmake --preset windows-msvc2022-release
cmake --build --preset build-release
```

vcpkg 會自動安裝 QuantLib 與所有 Boost 相依套件，不需手動操作。

---

## 12. 常見錯誤與解決方式

---

### ❌ `fatal: not a git repository`

執行 `git submodule add` 前忘記初始化 repo。

```powershell
git init    # 先執行這行
git submodule add https://github.com/microsoft/vcpkg.git vcpkg
```

---

### ❌ `Could NOT find Boost` 或 `Could NOT find QuantLib`

vcpkg toolchain 沒有被 CMake 載入，有兩種可能：

**原因 A：`CMakeLists.txt` 中 toolchain 設定的位置不對**

`set(CMAKE_TOOLCHAIN_FILE ...)` 必須在 `project()` **之前**，否則 CMake 初始化完編譯器才載入 vcpkg，已經太晚。

**原因 B：`CMakeSettings.json` 覆蓋了 `CMakePresets.json`**

刪除專案根目錄中的 `CMakeSettings.json`，重新設定 CMake。

---

### ❌ `quantlib is only supported on '!(windows & !static)'`

QuantLib 在 vcpkg 只支援靜態連結。triplet 必須改為 `x64-windows-static`。

在 `CMakeLists.txt` 的 `project()` 前加上：
```cmake
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)
```
在 `CMakePresets.json` 的 `cacheVariables` 也加上：
```json
"VCPKG_TARGET_TRIPLET": "x64-windows-static"
```

---

### ❌ `CMAKE_CXX_COMPILER not set, after EnableLanguage`

`CMakePresets.json` 的 generator 設為 `"Ninja"`，但 Ninja 在 VS2022 環境下找不到 MSVC 編譯器。

改為 Visual Studio generator：
```json
"generator": "Visual Studio 17 2022",
"architecture": "x64"
```

---

### ❌ `LNK2038: mismatch detected for 'RuntimeLibrary': MTd_StaticDebug doesn't match MDd_DynamicDebug`

QuantLib（靜態 triplet）使用 `/MTd`，但你的 App 預設使用 `/MDd`，runtime 不符導致連結失敗。

在 `CMakeLists.txt` 中加上（`cmake_policy CMP0091 NEW` 也必須設定）：
```cmake
cmake_policy(SET CMP0091 NEW)
...
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
```

---

### ❌ `Cannot find source file: .../build/debug/xxx.cpp`

CMake 在 build 目錄找原始碼，代表 SOURCES 的路徑沒有明確指向 source 目錄。

在 SOURCES 中加上 `${CMAKE_CURRENT_SOURCE_DIR}/` 前綴：
```cmake
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/QuantLibQT.cpp
    ...
)
```

---

### ❌ `error C1083: Cannot open include file: 'Xxx.h': No such file or directory`

`#include "..."` 中的檔名與實際磁碟上的檔名不符。請先確認實際檔名：

```powershell
dir *.cpp
dir *.h
```

include 的檔名必須與 `dir` 列出的完全一致。

---

### ❌ `Does not match the platform used previously` / CMake 快取衝突

更改 generator 或其他設定後，舊的快取與新設定衝突。

```
專案 → 刪除快取並重新設定
```
或手動刪除整個 `build/` 資料夾後重新設定。

---

### ❌ `warning C4828: illegal character` / 中文亂碼

原始碼含中文但 MSVC 不認識編碼。在 `CMakeLists.txt` 加上：
```cmake
if(MSVC)
    add_compile_options(/utf-8)
endif()
```
或在 VS2022 中以 `UTF-8 with BOM` 格式儲存所有原始碼檔案。

---

### ❌ 執行時找不到 `Qt6Widgetsd.dll` / `Qt6Cored.dll`

編譯成功但執行時找不到 Qt DLL，代表 windeployqt 沒有執行。

手動執行（開啟 Qt 命令提示字元）：
```powershell
windeployqt.exe "你的專案路徑\out\build\debug\QuantLibQT.exe"
```
或確認 `CMakeLists.txt` 中的 `add_custom_command` 目標名稱與 `qt_add_executable` 的名稱一致。

---

### ❌ `git clone` 後 `vcpkg/` 目錄是空的

clone 時未帶入 submodule：
```bash
git submodule update --init --recursive
```

---

### ❌ `Error loading schema` (vcpkg.json)

VS2022 無法連線到 GitHub 下載 JSON schema 驗證檔，為無害的警告。
把 `vcpkg.json` 中的 `"$schema"` 欄位移除即可消除警告。

---

## 附錄：建置流程示意圖

```
git clone --recurse-submodules
        │
        ▼
.\vcpkg\bootstrap-vcpkg.bat
        │
        ▼
VS2022 開啟 CMakeLists.txt
        │
        ▼
CMake 設定（讀取 CMakePresets.json）
        │
        ├─ 載入 vcpkg toolchain（CMAKE_TOOLCHAIN_FILE）
        │       │
        │       ▼
        │  vcpkg install（讀取 vcpkg.json）
        │  ┌──────────────────────────────────┐
        │  │ quantlib 已安裝?                  │
        │  │  否 ──▶ 自動下載並編譯             │
        │  │         QuantLib + Boost          │
        │  │         （首次約 30–60 分鐘）      │
        │  └──────────────────────────────────┘
        │
        ├─ 找到 Qt6（透過 CMAKE_PREFIX_PATH）
        │
        ▼
建置（Build）
        │
        ▼
windeployqt 自動複製 Qt DLL
        │
        ▼
   QuantLibQT.exe  ✅
```
