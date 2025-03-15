@echo off
echo Building Enhanced PureDoom BSP Implementation with Renderer...

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
cmake -DCMAKE_BUILD_TYPE=Debug -DSDL2_DIR="%SDL2_DIR%" ..
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM Build the project
cmake --build . --config Debug
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM Copy SDL2 DLL to the output directory
copy "%SDL2_DIR%\lib\x86\SDL2.dll" bin\Debug\

echo Build successful! Running Enhanced PureDoom...
echo.
echo Controls:
echo - WASD: Move player
echo - QE: Rotate view
echo - SPACE: Trigger door
echo - ESC: Quit
echo.
echo Press any key to continue after the program finishes...
bin\Debug\PureDoom.exe
echo.
echo Program finished.
cd ..
pause 