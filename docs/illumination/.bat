@echo off
:: ---------------------------------------------------------------
::  Re-encode: Polyrhythmic Illuminations - Trim.mp4  ->  4K
::  Place this .bat in the same folder as the video and run it.
::  Requires ffmpeg: https://ffmpeg.org/download.html
:: ---------------------------------------------------------------

set INPUT=Polyrhythmic Illuminations - Trim.mp4
set OUTPUT=Polyrhythmic Illuminations - Trim_4K.mp4

echo.
echo  Input  : %INPUT%
echo  Output : %OUTPUT%
echo.

where ffmpeg >nul 2>&1
if errorlevel 1 (
    echo  ERROR: ffmpeg not found in PATH.
    echo  Download: https://ffmpeg.org/download.html
    pause
    exit /b 1
)

if not exist "%INPUT%" (
    echo  ERROR: "%INPUT%" not found in this folder.
    pause
    exit /b 1
)

if exist "%OUTPUT%" del "%OUTPUT%"

ffmpeg -i "%INPUT%" -vf "scale=3840:-2:flags=lanczos" -c:v libx264 -crf 17 -preset slow -profile:v high -level:v 5.1 -pix_fmt yuv420p -movflags +faststart -c:a copy -y "%OUTPUT%"

if errorlevel 1 (
    echo.
    echo  ERROR: Encode failed.
    if exist "%OUTPUT%" del "%OUTPUT%"
    pause
    exit /b 1
)

for %%F in ("%OUTPUT%") do set OUTSIZE=%%~zF
if "%OUTSIZE%"=="0" (
    echo  ERROR: Output file is empty.
    del "%OUTPUT%"
    pause
    exit /b 1
)

echo.
echo  Done.
echo  Saved: %OUTPUT%
echo.
for %%F in ("%INPUT%")  do echo  Original : %%~zF bytes
for %%F in ("%OUTPUT%") do echo  Encoded  : %%~zF bytes
echo.
pause