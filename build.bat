@echo off
echo Building PureDoom and CUDA Test Map...

REM Set SDL2 path - adjust this to your SDL2 installation path
set SDL2_DIR=C:\SDL2

REM Check if SDL2 exists at the specified path
if not exist "%SDL2_DIR%\include\SDL.h" (
    echo SDL2 not found at %SDL2_DIR%
    echo Please install SDL2 or update the SDL2_DIR variable in this script.
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake -DCMAKE_BUILD_TYPE=Release -DSDL2_DIR="%SDL2_DIR%" ..
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM Build the project
echo Building projects...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM Copy SDL2 DLL to the output directory
echo Copying SDL2 DLL to output directories...
copy "%SDL2_DIR%\lib\x86\SDL2.dll" bin\Release\
copy "%SDL2_DIR%\lib\x86\SDL2_image.dll" bin\Release\

echo.
echo Build successful!
echo.
echo What would you like to run?
echo 1. Main PureDoom Game
echo 2. CUDA Test Map
echo 3. Exit
echo.

choice /c 123 /n /m "Enter your choice (1-3): "

if %ERRORLEVEL% == 1 (
    echo.
    echo Running PureDoom...
    echo.
    echo Controls:
    echo - WASD: Move player
    echo - QE: Rotate view
    echo - SPACE: Trigger door
    echo - ESC: Quit
    echo.
    bin\Release\PureDoom.exe
) else if %ERRORLEVEL% == 2 (
    echo.
    echo Running CUDA Test Map...
    echo.
    echo Controls:
    echo - WASD: Move player
    echo - QE: Rotate view
    echo - ESC: Quit
    echo.
    bin\Release\cuda_test_map.exe
)

echo.
cd ..
pause 