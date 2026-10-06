@echo off
setlocal

echo =======================================================
echo               FLUX - Native Desktop Player
echo =======================================================

:: 1. Close any running FLUX instance to release file locks on FLUX.exe
taskkill /f /im FLUX.exe >nul 2>&1

:: 2. Initialize MSVC 64-bit environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Visual Studio Build Tools environment could not be initialized.
    pause
    exit /b 1
)

:: 3. Configure CMake if build folder does not exist
if not exist "build\build.ninja" if not exist "build\FLUX.sln" (
    echo [*] Configuring CMake project...
    "C:\Program Files\CMake\bin\cmake.exe" -B build -S . -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64" -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 (
        echo [ERROR] CMake configuration failed.
        pause
        exit /b 1
    )
)

:: 4. Build FLUX
echo [*] Building FLUX (Release)...
"C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --target FLUX
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

:: 5. Deploy Qt runtime dependencies if not already done
if not exist "build\Release\Qt6Quick.dll" (
    echo [*] Deploying Qt runtime dependencies...
    "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --qmldir qml build\Release\FLUX.exe
)

:: 6. Launch FLUX
echo [OK] Launching FLUX.exe...
start "" "build\Release\FLUX.exe"

endlocal
