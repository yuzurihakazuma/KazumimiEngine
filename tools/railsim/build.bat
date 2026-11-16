@echo off
rem Rail junction simulator: builds the game's real SplineRail / RailConnections / road builders / Player
rem without rendering. See main.cpp for the commands.
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set P=%~dp0..\..\project
cd /d "%~dp0"
if not exist obj mkdir obj
set INC=/I"%P%" /I"%P%\engine\graphics" /I"%P%\engine\base" /I"%P%\engine\audio" /I"%P%\engine\2d" /I"%P%\engine\3d\obj" /I"%P%\engine\3d\model" /I"%P%\engine\3d" /I"%P%\engine\math" /I"%P%\engine\particle" /I"%P%\engine\utils" /I"%P%\game\scene" /I"%P%\game" /I"%P%\game\base" /I"%P%\externals\assimp\include" /I"%P%\engine\collision" /I"%P%\engine\postEffect" /I"%P%\engine\3d\animation" /I"%P%\externals\DirectXTex" /I"%P%\externals\imgui" /I"%P%\externals"
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /D_ALLOW_KEYWORD_MACROS /DDIRECTINPUT_VERSION=0x0800 %INC% /Foobj\ ^
  main.cpp ..\blocksim\stubs.cpp ^
  "%P%\engine\rail\SplineRail.cpp" "%P%\engine\math\VectorMath.cpp" ^
  "%P%\game\rail\RailConnections.cpp" ^
  "%P%\game\rail\road\RoadJunctionDetector.cpp" "%P%\game\rail\road\RoadJunctionPatchBuilder.cpp" ^
  "%P%\game\rail\road\RoadSweepBuilder.cpp" "%P%\game\rail\road\RoadClothBuilder.cpp" "%P%\game\rail\road\RoadMeshBuilder.cpp" ^
  "%P%\game\stage\BlockSystem.cpp" "%P%\game\stage\BlockShape.cpp" "%P%\game\stage\BlockGrid.cpp" ^
  "%P%\game\player\Player.cpp" "%P%\game\player\PlayerInput.cpp" "%P%\game\player\PlayerJump.cpp" ^
  "%P%\game\player\PlayerBlockContact.cpp" "%P%\game\player\PlayerFacing.cpp" ^
  "%P%\game\player\PlayerKnockback.cpp" "%P%\game\player\PlayerRailQuery.cpp" ^
  /Fe:railsim.exe > build.txt 2>&1 || (type build.txt & exit /b 1)
echo built railsim.exe
