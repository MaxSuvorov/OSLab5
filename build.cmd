@echo off
echo Building Temperature Monitoring System v2.0...

echo.
echo Step 1: Downloading SQLite...
if not exist thirdparty mkdir thirdparty
cd thirdparty
if not exist sqlite mkdir sqlite
cd sqlite

powershell -Command "Invoke-WebRequest -Uri 'https://www.sqlite.org/2023/sqlite-amalgamation-3440000.zip' -OutFile 'sqlite.zip'"
powershell -Command "Expand-Archive -Path 'sqlite.zip' -DestinationPath '.' -Force"

for /r %%i in (*.c) do move "%%i" .
for /r %%i in (*.h) do move "%%i" .
del sqlite.zip
cd ..\..

echo.
echo Step 2: Building with CMake...
if not exist build mkdir build
cd build

echo Configuring...
cmake -G "MinGW Makefiles" ..

echo Building...
cmake --build .

echo.
echo Done!
echo.
echo Executables created:
echo   - temperature_server.exe (main server)
echo   - web_server.exe (web interface)
echo   - device_simulator.exe (device simulator)
echo.
echo Usage:
echo   1. Run temperature_server.exe
echo   2. Run web_server.exe (optional, for web interface)
echo   3. Open browser: http://localhost:8081
echo.
pause