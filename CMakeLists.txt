# ¢w¢w ¥²¶·¬OÀÉ®×ªº²Ä¤@¦æ¦³®Äµ{¦¡½X ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

cmake_minimum_required(VERSION 3.25)

message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "VCPKG_INSTALLED_DIR = ${VCPKG_INSTALLED_DIR}")

# ¢w¢w ±M®×¦WºÙ»P»y¨¥ ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
project(MyQuantLibApp LANGUAGES CXX)

# Åý MSVC runtime »P vcpkg x64-windows-static ¤@­P
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

# ¢w¢w Qt ³]©w ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
# ±Ò¥Î Qt ¦Û°Ê¤Æ¤u¨ã
set(CMAKE_AUTOMOC ON)    # ¦Û°Ê³B²z Q_OBJECT
set(CMAKE_AUTORCC ON)    # ¦Û°Ê³B²z .qrc ¸ê·½ÀÉ
set(CMAKE_AUTOUIC ON)    # ¦Û°Ê³B²z .ui ÀÉ

# ´M§ä Qt6¡]»Ý³]©w CMAKE_PREFIX_PATH ©ÎÀô¹ÒÅÜ¼Æ Qt6_DIR¡^
find_package(Qt6 COMPONENTS Core Widgets Charts Network Sql Concurrent REQUIRED)

find_package(Qt6 QUIET COMPONENTS DataVisualization)
if(Qt6DataVisualization_FOUND)
    message(STATUS "Qt6 DataVisualization found ??VolSurface 3D enabled")
    add_compile_definitions(HAS_QT_DATA_VISUALIZATION)
else()
    message(STATUS "Qt6 DataVisualization NOT found ??VolSurface 3D disabled")
endif()

# ¢w¢w QuantLib¡]¥Ñ vcpkg ºÞ²z¡^¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
find_package(QuantLib CONFIG REQUIRED)

# ¢w¢w ­ì©l½X ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
# ¢w¢w Sources ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MonteCarloWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MonteCarloWidget.cpp
)

if(Qt6DataVisualization_FOUND)
    list(APPEND UI_SOURCES
        src/ui/VolSurfaceWidget.h
        src/ui/VolSurfaceWidget.cpp
    )
endif()

if(WIN32)
    set(PLATFORM_SOURCES app.rc)
endif()

# ¢w¢w «Ø¸m¥Ø¼Ð ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
qt_add_executable(Quant-Dashboard
    src/main.cpp
    ${INFRA_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
    resources.qrc  
)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(Quant-Dashboard PROPERTIES
        WIN32_EXECUTABLE FALSE  # Debug ?‚é¡¯ç¤?console
    )
endif()


target_link_libraries(Quant-Dashboard PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network      # ¡ö ½T»{³o¦æ¦s¦b
    Qt6::Sql
    Qt6::Concurrent
    QuantLib::QuantLib
    $<$<BOOL:${Qt6DataVisualization_FOUND}>:Qt6::DataVisualization>
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
