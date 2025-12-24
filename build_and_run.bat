@echo off
if not exist build mkdir build
cd build
cmake ..
if %errorlevel% neq 0 (
    echo [Error] CMake configuration failed.
    pause
    exit /b %errorlevel%
)
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo [Error] Build failed.
    pause
    exit /b %errorlevel%
)
echo [Success] Build passed.
cd ..
pause
