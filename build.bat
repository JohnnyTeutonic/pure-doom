@echo off
echo Building Enhanced PureDoom BSP Implementation...

if not exist build mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Debug ..
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

cmake --build . --config Debug
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

echo Build successful! Running Enhanced PureDoom...
echo.
echo Press any key to continue after the program finishes...
bin\Debug\PureDoom.exe
echo.
echo Program finished.
cd ..
pause 