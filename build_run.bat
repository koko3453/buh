@echo off
setlocal EnableExtensions EnableDelayedExpansion
echo Starting build...

pushd "%~dp0"

set LOG_FILE=%~dp0compile_message.txt
echo Build log started at %DATE% %TIME%> "%LOG_FILE%"

set BUILD_DIR=build
set EXE_RELEASE=%BUILD_DIR%\Release\buh.exe
set EXE_DEBUG=%BUILD_DIR%\Debug\buh.exe
set EXE_PLAIN=%BUILD_DIR%\buh.exe

echo Validating data...
call "%~dp0tools\validate_data.bat" >> "%LOG_FILE%" 2>&1
if errorlevel 1 goto :fail

if "%VCPKG_ROOT%"=="" (
  set "VCPKG_ROOT=%~dp0tools\vcpkg"
)

if exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
  echo Using vcpkg toolchain at %VCPKG_ROOT%
) else (
  echo vcpkg not found. Bootstrapping to %VCPKG_ROOT%
  if not exist "%~dp0tools" mkdir "%~dp0tools"
  git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%"
  if errorlevel 1 goto :fail
  call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
  if errorlevel 1 goto :fail
)

echo Installing SDL2 + SDL2_ttf via vcpkg...
call "%VCPKG_ROOT%\vcpkg.exe" install sdl2 sdl2-ttf >> "%LOG_FILE%" 2>&1
if errorlevel 1 goto :fail

if exist %BUILD_DIR% (
  rmdir /s /q %BUILD_DIR%
)
cmake -S . -B %BUILD_DIR% -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_APPLOCAL_DEPS=ON >> "%LOG_FILE%" 2>&1
if errorlevel 1 goto :fail
cmake --build %BUILD_DIR% --config Release >> "%LOG_FILE%" 2>&1
if errorlevel 1 goto :fail

if not exist "%BUILD_DIR%\Release\data\assets" (
  mkdir "%BUILD_DIR%\Release\data\assets"
)
echo Copying assets...
robocopy "data\assets" "%BUILD_DIR%\Release\data\assets" /E /NFL /NDL /NJH /NJS /NC /NS >nul
if %ERRORLEVEL% GEQ 8 goto :fail
goto :run

:run
if exist %EXE_RELEASE% (
  echo Running %EXE_RELEASE%
  "%EXE_RELEASE%"
  goto :done
)
if exist %EXE_DEBUG% (
  echo Running %EXE_DEBUG%
  if not exist "%BUILD_DIR%\Debug\data\assets" (
    mkdir "%BUILD_DIR%\Debug\data\assets"
  )
  robocopy "data\assets" "%BUILD_DIR%\Debug\data\assets" /E /NFL /NDL /NJH /NJS /NC /NS >nul
  if %ERRORLEVEL% GEQ 8 goto :fail
  "%EXE_DEBUG%"
  goto :done
)
if exist %EXE_PLAIN% (
  echo Running %EXE_PLAIN%
  "%EXE_PLAIN%"
  goto :done
)
echo Build succeeded but executable not found.
goto :fail

:fail
echo.
echo Build failed. See messages above.
popd
pause
exit /b 1

:done
echo.
echo Done.
popd
exit /b 0
