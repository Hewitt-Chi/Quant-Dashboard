@echo off
:: -----------------------------------------------------------------------------
:: deploy.bat - QuantLib-QT Dashboard Release Packaging Script
::
:: Usage:
::   1. Build Release in Visual Studio first
::   2. Run this script from project root: deploy.bat
::   3. Output will be in dist\ folder, ready to zip and distribute
::
:: Edit the paths below to match your environment before running
:: -----------------------------------------------------------------------------

setlocal EnableDelayedExpansion

:: -- EDIT THESE PATHS ----------------------------------------------------------
set QTDIR=C:\Qt\6.9.3\msvc2022_64
set BUILD_DIR=I:\WorkSpace_Repo\QuantLib-QT\build\release\Release
set DIST_DIR=%~dp0dist\QuantLib-QT-Dashboard
:: -----------------------------------------------------------------------------

set EXE=%BUILD_DIR%\Quant-Dashboard.exe

if not exist "%EXE%" (
    echo [ERROR] Release exe not found: %EXE%
    echo         Please build Release configuration in Visual Studio first.
    pause
    exit /b 1
)

echo.
echo =============================================================
echo  QuantLib-QT Dashboard - Release Packaging
echo =============================================================
echo  EXE  : %EXE%
echo  Qt   : %QTDIR%
echo  Output: %DIST_DIR%
echo =============================================================
echo.

:: Create output directory
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

:: [1/4] Copy exe
echo [1/4] Copying executable...
copy "%EXE%" "%DIST_DIR%\" >nul
echo       Done: Quant-Dashboard.exe

:: Copy icon if exists
if exist "%~dp0app_icon.ico" (
    copy "%~dp0app_icon.ico" "%DIST_DIR%\" >nul
    echo       Done: app_icon.ico
)

:: [2/4] Run windeployqt
echo [2/4] Running windeployqt...
set WINDEPLOYQT=%QTDIR%\bin\windeployqt.exe

if not exist "%WINDEPLOYQT%" (
    echo [ERROR] windeployqt not found: %WINDEPLOYQT%
    echo         Please check your QTDIR path setting.
    pause
    exit /b 1
)

"%WINDEPLOYQT%" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --compiler-runtime ^
    "%DIST_DIR%\Quant-Dashboard.exe"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] windeployqt failed
    pause
    exit /b 1
)
echo       Done: Qt DLLs deployed

:: [3/4] Copy MSVC runtime DLLs
echo [3/4] Checking MSVC runtime...
for %%f in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll concrt140.dll) do (
    if exist "%WINDIR%\System32\%%f" (
        copy "%WINDIR%\System32\%%f" "%DIST_DIR%\" >nul 2>&1
    )
)
echo       Done: MSVC runtime

:: [4/4] Create README
echo [4/4] Creating README...
(
echo QuantLib-QT Dashboard
echo =====================
echo.
echo Features:
echo   - Watchlist    : Real-time quotes + sparkline charts
echo   - Option pricer: BSM/Heston pricing + Greeks visualization
echo   - Yield curve  : QuantLib bootstrap + scenario analysis
echo   - Option chain : Strike scanner + IV smile
echo   - Vol surface  : 3D implied vol surface
echo   - Backtest     : 5 strategies (CC/PP/IC/Collar/CSP)
echo   - Settings     : Quote provider / DB path configuration
echo.
echo First Run:
echo   1. Open Settings, configure Quote provider
echo   2. Go to Watchlist and wait for history to load (~5-10 sec)
echo   3. All features are now ready to use
echo.
echo Requirements:
echo   Windows 10/11 64-bit
echo   Internet connection (for market data)
echo.
echo Database:
echo   Default location: same folder as exe (quant.db)
echo   Can be changed in Settings
) > "%DIST_DIR%\README.txt"
echo       Done: README.txt

:: Summary
echo.
echo =============================================================
echo  Packaging complete!
echo  Output: %DIST_DIR%
echo =============================================================
echo.
echo File list:
dir /b "%DIST_DIR%"
echo.
echo Tip: Compress dist\QuantLib-QT-Dashboard\ to a ZIP to distribute.
pause