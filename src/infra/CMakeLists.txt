cmake_minimum_required(VERSION 3.21)
project(QuantDashboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── vcpkg toolchain (set via CMakePresets.json or -DCMAKE_TOOLCHAIN_FILE=...) ──
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

# ── Qt6 ───────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Charts
    Network
    Sql
    Concurrent
)

# ── QuantLib (installed via vcpkg: vcpkg install quantlib) ────────────────────
find_package(QuantLib REQUIRED)

# ── Sources ───────────────────────────────────────────────────────────────────
set(INFRA_SOURCES
    src/infra/AppSettings.h
    src/infra/AppSettings.cpp
    src/infra/AsyncWorker.h
    src/infra/AsyncWorker.cpp
    src/infra/DatabaseManager.h
    src/infra/DatabaseManager.cpp
    src/infra/QuoteFetcher.h
    src/infra/QuoteFetcher.cpp
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    ${INFRA_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    QuantLib::QuantLib
)
