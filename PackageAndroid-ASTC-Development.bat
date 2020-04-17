set UE4_ROOT=D:\UnrealEngine\
set PROJECT_FILE=G:\TestUE\TestUE.uproject
set ARCHIVED_DIR=G:\TestUE\Build


%UE4_ROOT%Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -project=%PROJECT_FILE% -clientconfig=Development -archivedirectory=%ARCHIVED_DIR% -cookflavor=ASTC -targetplatform=Android -skipserver -nop4 -nocompileeditor -build -cook -cookall -stage -archive -package -pak -prereqs -compressed -unattended -utf8output -nodebuginfo


pause