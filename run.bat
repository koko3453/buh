@echo off
setlocal EnableExtensions EnableDelayedExpansion
pushd "%~dp0"

set BUILD_DIR=build
set EXE_RELEASE=%BUILD_DIR%\Release\buh.exe
set EXE_DEBUG=%BUILD_DIR%\Debug\buh.exe
set EXE_PLAIN=%BUILD_DIR%\buh.exe

if exist "%EXE_RELEASE%" (
  echo Running %EXE_RELEASE%
  "%EXE_RELEASE%"
  goto :done
)
if exist "%EXE_DEBUG%" (
  echo Running %EXE_DEBUG%
  "%EXE_DEBUG%"
  goto :done
)
if exist "%EXE_PLAIN%" (
  echo Running %EXE_PLAIN%
  "%EXE_PLAIN%"
  goto :done
)

echo Built executable not found. Build first.
exit /b 1

:done
popd
exit /b 0
