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
)

# ── 建置目標 ──────────────────────────────────────────────────────────────────
qt_add_executable(Quant-Dashboard
    src/main.cpp
    ${INFRA_SOURCES}
    ${UI_SOURCES}
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
