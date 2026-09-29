@echo off
rem Block collision simulator: builds the game's real Player / BlockSystem / SplineRail
rem without rendering, runs scripted scenarios and reports where the player gets stuck.
rem   build_and_run.bat            ... run all scenarios (summary)
rem   build_and_run.bat v "C:"     ... per-frame log of the scenario whose name starts with "C:"
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set P=%~dp0..\..\project
cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /D_ALLOW_KEYWORD_MACROS /DDIRECTINPUT_VERSION=0x0800 /I"%P%" /Foobj\ ^
  main.cpp stubs.cpp ^
  "%P%\engine\rail\SplineRail.cpp" "%P%\engine\math\VectorMath.cpp" ^
  "%P%\game\stage\BlockSystem.cpp" "%P%\game\stage\BlockShape.cpp" "%P%\game\stage\BlockGrid.cpp" ^
  "%P%\game\player\Player.cpp" "%P%\game\player\PlayerInput.cpp" "%P%\game\player\PlayerJump.cpp" ^
  "%P%\game\player\PlayerBlockContact.cpp" "%P%\game\player\PlayerFacing.cpp" ^
  "%P%\game\player\PlayerKnockback.cpp" "%P%\game\player\PlayerRailQuery.cpp" ^
  /Fe:blocksim.exe > build.txt 2>&1 || (type build.txt & exit /b 1)
"%~dp0blocksim.exe" %*
