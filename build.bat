@echo off
setlocal

if "%VCPKG_ROOT%"=="" (
  echo VCPKG_ROOT not set. Please set it to your vcpkg path.
  exit /b 1
)

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

endlocal
