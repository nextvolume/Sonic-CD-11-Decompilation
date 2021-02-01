#include "RetroEngine.hpp"

#if RETRO_PLATFORM == RETRO_WIN && _MSC_VER
#include <Windows.h>
#include <codecvt>
#include "../dependencies/windows/ValveFileVDF/vdf_parser.hpp"

HKEY hKey;

LONG GetDWORDRegKey(HKEY hKey, const std::wstring &strValueName, DWORD &nValue, DWORD nDefaultValue)
{
    nValue = nDefaultValue;
    DWORD dwBufferSize(sizeof(DWORD));
    DWORD nResult(0);
    LONG nError = ::RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, reinterpret_cast<LPBYTE>(&nResult), &dwBufferSize);
    if (ERROR_SUCCESS == nError) {
        nValue = nResult;
    }
    return nError;
}

LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue, const std::wstring &strDefaultValue)
{
    strValue = strDefaultValue;
    WCHAR szBuffer[512];
    DWORD dwBufferSize = sizeof(szBuffer);
    ULONG nError;
    nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
    if (ERROR_SUCCESS == nError) {
        strValue = szBuffer;
    }
    return nError;
}

inline std::string utf16ToUtf8(const std::wstring &utf16Str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(utf16Str);
}

inline bool dirExists(const std::wstring &dirName_in)
{
    DWORD ftyp = GetFileAttributesW(dirName_in.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES)
        return false; // something is wrong with your path!

    if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
        return true; // this is a directory!

    return false; // this is not a directory!
}
#endif

int globalVariablesCount;
int globalVariables[GLOBALVAR_COUNT];
char globalVariableNames[GLOBALVAR_COUNT][0x20];

char gamePath[0x100];
int saveRAM[SAVEDATA_MAX];
Achievement achievements[ACHIEVEMENT_MAX];
LeaderboardEntry leaderboard[LEADERBOARD_MAX];

int controlMode = -1;

void InitUserdata()
{
    // userdata files are loaded from this directory
    sprintf(gamePath, BASE_PATH);

    char buffer[0x200];
#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_UWP
    if (!usingCWD)
        sprintf(buffer, "%s/settings.ini", getResourcesPath());
    else
        sprintf(buffer, "%ssettings.ini", gamePath);
#elif RETRO_PLATFORM == RETRO_iOS
    sprintf(buffer, "%s/settings.ini", getDocumentsPath());
#else
    sprintf(buffer, BASE_PATH"settings.ini");
#endif
    FileIO *file = fOpen(buffer, "rb");
    IniParser ini;
    if (!file) {
        ini.SetBool("Dev", "DevMenu", Engine.devMenu = false);
        ini.SetBool("Dev", "EngineDebugMode", engineDebugMode = false);
        ini.SetInteger("Dev", "StartingCategory", Engine.startList = 0);
        ini.SetInteger("Dev", "StartingScene", Engine.startStage = 0);
        ini.SetInteger("Dev", "FastForwardSpeed", Engine.fastForwardSpeed = 8);
        ini.SetBool("Dev", "UseSteamDir", Engine.useSteamDir = true);
        ini.SetBool("Dev", "UseHQModes", Engine.useHQModes = true);

        ini.SetInteger("Game", "Language", Engine.language = RETRO_EN);
        ini.SetInteger("Game", "OriginalControls", -1);

        ini.SetBool("Window", "FullScreen", Engine.startFullScreen = DEFAULT_FULLSCREEN);
        ini.SetBool("Window", "Borderless", Engine.borderless = false);
        ini.SetBool("Window", "VSync", Engine.vsync = false);
        ini.SetBool("Window", "EnhancedScaling", Engine.enhancedScaling = true);
        ini.SetInteger("Window", "WindowScale", Engine.windowScale = 2);
        ini.SetInteger("Window", "ScreenWidth", SCREEN_XSIZE = DEFAULT_SCREEN_XSIZE);
        ini.SetInteger("Window", "RefreshRate", Engine.refreshRate = 60);

#if RETRO_DOS
	ini.SetInteger("Window", "UseVGAMode", useVGAMode=0);
#endif
	    
        ini.SetFloat("Audio", "BGMVolume", bgmVolume / (float)MAX_VOLUME);
        ini.SetFloat("Audio", "SFXVolume", sfxVolume / (float)MAX_VOLUME);

#if RETRO_USING_SDL2
        ini.SetComment("Keyboard 1", "IK1Comment", "Keyboard Mappings for P1 (Based on: https://wiki.libsdl.org/SDL_Scancode)");
        ini.SetInteger("Keyboard 1", "Up", inputDevice[INPUT_UP].keyMappings = SDL_SCANCODE_UP);
        ini.SetInteger("Keyboard 1", "Down", inputDevice[INPUT_DOWN].keyMappings = SDL_SCANCODE_DOWN);
        ini.SetInteger("Keyboard 1", "Left", inputDevice[INPUT_LEFT].keyMappings = SDL_SCANCODE_LEFT);
        ini.SetInteger("Keyboard 1", "Right", inputDevice[INPUT_RIGHT].keyMappings = SDL_SCANCODE_RIGHT);
        ini.SetInteger("Keyboard 1", "A", inputDevice[INPUT_BUTTONA].keyMappings = SDL_SCANCODE_Z);
        ini.SetInteger("Keyboard 1", "B", inputDevice[INPUT_BUTTONB].keyMappings = SDL_SCANCODE_X);
        ini.SetInteger("Keyboard 1", "C", inputDevice[INPUT_BUTTONC].keyMappings = SDL_SCANCODE_C);
        ini.SetInteger("Keyboard 1", "Start", inputDevice[INPUT_START].keyMappings = SDL_SCANCODE_RETURN);

        ini.SetComment("Controller 1", "IC1Comment", "Controller Mappings for P1 (Based on: https://wiki.libsdl.org/SDL_GameControllerButton)");
        ini.SetInteger("Controller 1", "Up", inputDevice[INPUT_UP].contMappings = SDL_CONTROLLER_BUTTON_DPAD_UP);
        ini.SetInteger("Controller 1", "Down", inputDevice[INPUT_DOWN].contMappings = SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        ini.SetInteger("Controller 1", "Left", inputDevice[INPUT_LEFT].contMappings = SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        ini.SetInteger("Controller 1", "Right", inputDevice[INPUT_RIGHT].contMappings = SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        ini.SetInteger("Controller 1", "A", inputDevice[INPUT_BUTTONA].contMappings = SDL_CONTROLLER_BUTTON_A);
        ini.SetInteger("Controller 1", "B", inputDevice[INPUT_BUTTONB].contMappings = SDL_CONTROLLER_BUTTON_B);
        ini.SetInteger("Controller 1", "C", inputDevice[INPUT_BUTTONC].contMappings = SDL_CONTROLLER_BUTTON_X);
        ini.SetInteger("Controller 1", "Start", inputDevice[INPUT_START].contMappings = SDL_CONTROLLER_BUTTON_START);
#endif

        StrCopy(Engine.dataFile, "Data.rsdk");
        ini.SetString("Dev", "DataFile", Engine.dataFile);

        ini.Write(BASE_PATH"settings.ini");
    }
    else {
        fClose(file);
        ini = IniParser(BASE_PATH"settings.ini");

        if (!ini.GetBool("Dev", "DevMenu", &Engine.devMenu))
            Engine.devMenu = false;
        if (!ini.GetBool("Dev", "EngineDebugMode", &engineDebugMode))
            engineDebugMode = false;
        if (!ini.GetInteger("Dev", "StartingCategory", &Engine.startList))
            Engine.startList = 0;
        if (!ini.GetInteger("Dev", "StartingScene", &Engine.startStage))
            Engine.startStage = 0;
        if (!ini.GetInteger("Dev", "FastForwardSpeed", &Engine.fastForwardSpeed))
            Engine.fastForwardSpeed = 8;
        if (!ini.GetBool("Dev", "UseSteamDir", &Engine.useSteamDir))
            Engine.useSteamDir = true;
        if (!ini.GetBool("Dev", "UseHQModes", &Engine.useHQModes))
            Engine.useHQModes = true;
	
        if (!ini.GetString("Dev", "DataFile", Engine.dataFile))
            StrCopy(Engine.dataFile, "Data.rsdk");

        if (!ini.GetInteger("Game", "Language", &Engine.language))
            Engine.language = RETRO_EN;
        
        if (!ini.GetInteger("Game", "OriginalControls", &controlMode))
            controlMode = -1;

        if (!ini.GetBool("Window", "FullScreen", &Engine.startFullScreen))
            Engine.startFullScreen = DEFAULT_FULLSCREEN;
        if (!ini.GetBool("Window", "Borderless", &Engine.borderless))
            Engine.borderless = false;
        if (!ini.GetBool("Window", "VSync", &Engine.vsync))
            Engine.vsync = false;
        if (!ini.GetBool("Window", "EnhancedScaling", &Engine.enhancedScaling))
            Engine.enhancedScaling = true;
        if (!ini.GetInteger("Window", "WindowScale", &Engine.windowScale))
            Engine.windowScale = 2;
        if (!ini.GetInteger("Window", "ScreenWidth", &SCREEN_XSIZE))
            SCREEN_XSIZE = DEFAULT_SCREEN_XSIZE;
        if (!ini.GetInteger("Window", "RefreshRate", &Engine.refreshRate))
            Engine.refreshRate = 60;

#ifdef RETRO_DOS
        if (!ini.GetInteger("Window", "UseVGAMode", &useVGAMode))
	    useVGAMode = 0;
#endif
	
        float bv = 0, sv = 0;
        if (!ini.GetFloat("Audio", "BGMVolume", &bv))
            bv = 1.0f;
        if (!ini.GetFloat("Audio", "SFXVolume", &sv))
            sv = 1.0f;

        bgmVolume = bv * MAX_VOLUME;
        sfxVolume = sv * MAX_VOLUME;

        if (bgmVolume > MAX_VOLUME)
            bgmVolume = MAX_VOLUME;
        if (bgmVolume < 0)
            bgmVolume = 0;

        if (sfxVolume > MAX_VOLUME)
            sfxVolume = MAX_VOLUME;
        if (sfxVolume < 0)
            sfxVolume = 0;

#if RETRO_USING_SDL2	
        if (!ini.GetInteger("Keyboard 1", "Up", &inputDevice[0].keyMappings))
            inputDevice[0].keyMappings = SDL_SCANCODE_UP;
        if (!ini.GetInteger("Keyboard 1", "Down", &inputDevice[1].keyMappings))
            inputDevice[1].keyMappings = SDL_SCANCODE_DOWN;
        if (!ini.GetInteger("Keyboard 1", "Left", &inputDevice[2].keyMappings))
            inputDevice[2].keyMappings = SDL_SCANCODE_LEFT;
        if (!ini.GetInteger("Keyboard 1", "Right", &inputDevice[3].keyMappings))
            inputDevice[3].keyMappings = SDL_SCANCODE_RIGHT;
        if (!ini.GetInteger("Keyboard 1", "A", &inputDevice[4].keyMappings))
            inputDevice[4].keyMappings = SDL_SCANCODE_Z;
        if (!ini.GetInteger("Keyboard 1", "B", &inputDevice[5].keyMappings))
            inputDevice[5].keyMappings = SDL_SCANCODE_X;
        if (!ini.GetInteger("Keyboard 1", "C", &inputDevice[6].keyMappings))
            inputDevice[6].keyMappings = SDL_SCANCODE_C;
        if (!ini.GetInteger("Keyboard 1", "Start", &inputDevice[7].keyMappings))
            inputDevice[7].keyMappings = SDL_SCANCODE_RETURN;

        if (!ini.GetInteger("Controller 1", "Up", &inputDevice[0].contMappings))
            inputDevice[0].contMappings = SDL_CONTROLLER_BUTTON_DPAD_UP;
        if (!ini.GetInteger("Controller 1", "Down", &inputDevice[1].contMappings))
            inputDevice[1].contMappings = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        if (!ini.GetInteger("Controller 1", "Left", &inputDevice[2].contMappings))
            inputDevice[2].contMappings = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        if (!ini.GetInteger("Controller 1", "Right", &inputDevice[3].contMappings))
            inputDevice[3].contMappings = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        if (!ini.GetInteger("Controller 1", "A", &inputDevice[4].contMappings))
            inputDevice[4].contMappings = SDL_CONTROLLER_BUTTON_A;
        if (!ini.GetInteger("Controller 1", "B", &inputDevice[5].contMappings))
            inputDevice[5].contMappings = SDL_CONTROLLER_BUTTON_B;
        if (!ini.GetInteger("Controller 1", "C", &inputDevice[6].contMappings))
            inputDevice[6].contMappings = SDL_CONTROLLER_BUTTON_X;
        if (!ini.GetInteger("Controller 1", "Start", &inputDevice[7].contMappings))
            inputDevice[7].contMappings = SDL_CONTROLLER_BUTTON_START;
#endif
    }
    SetScreenSize(SCREEN_XSIZE, SCREEN_YSIZE);

    // Support for extra controller types SDL doesn't recognise
#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_UWP
    if (!usingCWD)
        sprintf(buffer, "%s/controllerdb.txt", getResourcesPath());
    else
        sprintf(buffer, "%scontrollerdb.txt", gamePath);
#else
    sprintf(buffer, BASE_PATH "controllerdb.txt");
#endif
    file = fOpen(buffer, "rb");
    if (file) {
        fClose(file);

#if RETRO_USING_SDL2
        int nummaps = SDL_GameControllerAddMappingsFromFile(buffer);
        if (nummaps >= 0)
            printLog("loaded %d controller mappings from '%s'\n", buffer, nummaps);
#endif
    }

#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_UWP
    if (!usingCWD)
        sprintf(buffer, "%s/Udata.bin", getResourcesPath());
    else
        sprintf(buffer, "%sUdata.bin", gamePath);
#elif RETRO_PLATFORM == RETRO_iOS
    sprintf(buffer, "%s/UData.bin", getDocumentsPath());
#else
    sprintf(buffer, "%sUdata.bin", gamePath);
#endif
    file = fOpen(buffer, "rb");
    if (file) {
        fClose(file);
        ReadUserdata();
    }
    else {
        WriteUserdata();
    }

    StrCopy(achievements[0].name, "88 Miles Per Hour");
    StrCopy(achievements[1].name, "Just One Hug is Enough");
    StrCopy(achievements[2].name, "Paradise Found");
    StrCopy(achievements[3].name, "Take the High Road");
    StrCopy(achievements[4].name, "King of the Rings");
    StrCopy(achievements[5].name, "Statue Saviour");
    StrCopy(achievements[6].name, "Heavy Metal");
    StrCopy(achievements[7].name, "All Stages Clear");
    StrCopy(achievements[8].name, "Treasure Hunter");
    StrCopy(achievements[9].name, "Dr Eggman Got Served");
    StrCopy(achievements[10].name, "Just In Time");
    StrCopy(achievements[11].name, "Saviour of the Planet");

    // Loaded here so it can be disabled
#if RETRO_PLATFORM == RETRO_WIN && _MSC_VER
    if (Engine.useSteamDir) {
#if _WIN64
        LONG lRes             = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Valve\\Steam", 0, KEY_READ, &hKey);
        bool existsAndSuccess = lRes == ERROR_SUCCESS;
        std::wstring steamPath;

        if (existsAndSuccess) {
            GetStringRegKey(hKey, L"InstallPath", steamPath, L"");

            std::ifstream file(steamPath + L"/config/loginusers.vdf");
            auto root = tyti::vdf::read(file);

            std::vector<long long> SIDs;
            for (auto &child : root.childs) {
                long long sidVal = std::stoll(child.first);
                SIDs.push_back(sidVal & 0xFFFFFFFF);
            }

            for (auto &sid : SIDs) {
                std::wstring udataPath = steamPath.c_str() + std::wstring(L"/userdata/") + std::to_wstring(sid) + std::wstring(L"/200940/local/");

                if (dirExists(udataPath)) {
                    sprintf(gamePath, "%s", utf16ToUtf8(udataPath).c_str());
                    break;
                }
            }
        }

#elif _WIN32
        LONG lRes             = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey);
        bool existsAndSuccess = lRes == ERROR_SUCCESS;
        std::wstring steamPath;

        if (existsAndSuccess) {
            GetStringRegKey(hKey, L"InstallPath", steamPath, L"");

            std::ifstream file(steamPath + L"/config/loginusers.vdf");
            auto root = tyti::vdf::read(file);

            std::vector<long long> SIDs;
            for (auto &child : root.childs) {
                long long sidVal = std::stoll(child.first);
                SIDs.push_back(sidVal & 0xFFFFFFFF);
            }

            for (auto &sid : SIDs) {
                std::wstring udataPath = steamPath.c_str() + std::wstring(L"/userdata/") + std::to_wstring(sid) + std::wstring(L"/200940/local/");

                if (dirExists(udataPath)) {
                    sprintf(gamePath, "%s", utf16ToUtf8(udataPath).c_str());
                    break;
                }
            }
        }
#endif
    }
#endif
}

void writeSettings() {
    IniParser ini;

    ini.SetComment("Dev", "DevMenuComment", "Enable this flag to activate dev menu via the ESC key");
    ini.SetBool("Dev", "DevMenu", Engine.devMenu);
    ini.SetComment("Dev", "DebugModeComment", "Enable this flag to activate features used for debugging the engine (may result in slightly slower game speed)");
    ini.SetBool("Dev", "EngineDebugMode", engineDebugMode);
    ini.SetComment("Dev", "SCComment", "Sets the starting category ID");
    ini.SetInteger("Dev", "StartingCategory", Engine.startList);
    ini.SetComment("Dev", "SSComment", "Sets the starting scene ID");
    ini.SetInteger("Dev", "StartingScene", Engine.startStage);
    ini.SetComment("Dev", "FFComment", "Determines how fast the game will be when fastforwarding is active");
    ini.SetInteger("Dev", "FastForwardSpeed", Engine.fastForwardSpeed);
    ini.SetComment("Dev", "SDComment", "Determines if the game will try to use the steam directory for the game if it can locate it (windows only)");
    ini.SetBool("Dev", "UseSteamDir", Engine.useSteamDir);
    ini.SetComment("Dev", "UseHQComment","Determines if applicable rendering modes (such as 3D floor from special stages) will render in \"High Quality\" mode or standard mode");
    ini.SetBool("Dev", "UseHQModes", Engine.useHQModes);

    ini.SetComment("Dev", "DataFileComment", "Determines what RSDK file will be loaded");
    ini.SetString("Dev", "DataFile", Engine.dataFile);
	
    ini.SetComment("Game", "LangComment", "Sets the game language (0 = EN, 1 = FR, 2 = IT, 3 = DE, 4 = ES, 5 = JP)");
    ini.SetInteger("Game", "Language", Engine.language);
    ini.SetComment("Game", "OGCtrlComment", "Sets the game's spindash style (-1 = let save file decide, 0 = S2, 1 = CD)");
    ini.SetInteger("Game", "OriginalControls", controlMode);

    ini.SetComment("Window", "FSComment", "Determines if the window will be fullscreen or not");
    ini.SetBool("Window", "FullScreen", Engine.startFullScreen);
    ini.SetComment("Window", "BLComment", "Determines if the window will be borderless or not");
    ini.SetBool("Window", "Borderless", Engine.borderless);
    ini.SetComment("Window", "VSComment", "Determines if VSync will be active or not");
    ini.SetBool("Window", "VSync", Engine.vsync);
    ini.SetComment("Window", "ESComment", "Determines if Enhanced Scaling will be active or not. Only affects non-multiple resolutions.");
    ini.SetBool("Window", "EnhancedScaling", Engine.enhancedScaling);
    ini.SetComment("Window", "WSComment", "How big the window will be");
    ini.SetInteger("Window", "WindowScale", Engine.windowScale);
    ini.SetComment("Window", "SWComment", "How wide the base screen will be in pixels");
    ini.SetInteger("Window", "ScreenWidth", SCREEN_XSIZE);
    ini.SetComment("Window", "RRComment", "Determines the target FPS");
    ini.SetInteger("Window", "RefreshRate", Engine.refreshRate);
#if RETRO_DOS
    ini.SetComment("Window", "VGAComment", "Force VGA video mode (0=don't, 1=320x200[13h],2=320x240[X]),3=360x240[X], 4=376x282[X],5=400x300[X])");
    
    ini.SetInteger("Window", "UseVGAMode", useVGAMode);
#endif

    ini.SetFloat("Audio", "BGMVolume", bgmVolume / (float)MAX_VOLUME);
    ini.SetFloat("Audio", "SFXVolume", sfxVolume / (float)MAX_VOLUME);

#if RETRO_USING_SDL2
    ini.SetComment("Keyboard 1", "IK1Comment", "Keyboard Mappings for P1 (Based on: https://wiki.libsdl.org/SDL_Scancode)");
    ini.SetInteger("Keyboard 1", "Up", inputDevice[INPUT_UP].keyMappings);
    ini.SetInteger("Keyboard 1", "Down", inputDevice[INPUT_DOWN].keyMappings);
    ini.SetInteger("Keyboard 1", "Left", inputDevice[INPUT_LEFT].keyMappings);
    ini.SetInteger("Keyboard 1", "Right", inputDevice[INPUT_RIGHT].keyMappings);
    ini.SetInteger("Keyboard 1", "A", inputDevice[INPUT_BUTTONA].keyMappings);
    ini.SetInteger("Keyboard 1", "B", inputDevice[INPUT_BUTTONB].keyMappings);
    ini.SetInteger("Keyboard 1", "C", inputDevice[INPUT_BUTTONC].keyMappings);
    ini.SetInteger("Keyboard 1", "Start", inputDevice[INPUT_START].keyMappings);

    ini.SetComment("Controller 1", "IC1Comment", "Controller Mappings for P1 (Based on: https://wiki.libsdl.org/SDL_GameControllerButton)");
    ini.SetComment("Controller 1", "IC1Comment2", "Extra buttons can be mapped with the following IDs:");
    ini.SetComment("Controller 1", "IC1Comment3", "CONTROLLER_BUTTON_ZL             = 16");
    ini.SetComment("Controller 1", "IC1Comment4", "CONTROLLER_BUTTON_ZR             = 17");
    ini.SetComment("Controller 1", "IC1Comment5", "CONTROLLER_BUTTON_LSTICK_UP      = 18");
    ini.SetComment("Controller 1", "IC1Comment6", "CONTROLLER_BUTTON_LSTICK_DOWN    = 19");
    ini.SetComment("Controller 1", "IC1Comment7", "CONTROLLER_BUTTON_LSTICK_LEFT    = 20");
    ini.SetComment("Controller 1", "IC1Comment8", "CONTROLLER_BUTTON_LSTICK_RIGHT   = 21");
    ini.SetComment("Controller 1", "IC1Comment9", "CONTROLLER_BUTTON_RSTICK_UP      = 22");
    ini.SetComment("Controller 1", "IC1Comment10", "CONTROLLER_BUTTON_RSTICK_DOWN   = 23");
    ini.SetComment("Controller 1", "IC1Comment11", "CONTROLLER_BUTTON_RSTICK_LEFT   = 24");
    ini.SetComment("Controller 1", "IC1Comment12", "CONTROLLER_BUTTON_RSTICK_RIGHT  = 25");
    ini.SetInteger("Controller 1", "Up", inputDevice[INPUT_UP].contMappings);
    ini.SetInteger("Controller 1", "Down", inputDevice[INPUT_DOWN].contMappings);
    ini.SetInteger("Controller 1", "Left", inputDevice[INPUT_LEFT].contMappings);
    ini.SetInteger("Controller 1", "Right", inputDevice[INPUT_RIGHT].contMappings);
    ini.SetInteger("Controller 1", "A", inputDevice[INPUT_BUTTONA].contMappings);
    ini.SetInteger("Controller 1", "B", inputDevice[INPUT_BUTTONB].contMappings);
    ini.SetInteger("Controller 1", "C", inputDevice[INPUT_BUTTONC].contMappings);
    ini.SetInteger("Controller 1", "Start", inputDevice[INPUT_START].contMappings);
#endif

    ini.Write(BASE_PATH"settings.ini");
}

void ReadUserdata()
{
    char buffer[0x200];
#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_UWP
    if (!usingCWD)
        sprintf(buffer, "%s/Udata.bin", getResourcesPath());
    else
        sprintf(buffer, "%sUdata.bin", gamePath);
#else
    sprintf(buffer, "%sUdata.bin", gamePath);
#endif
    FileIO *userFile = fOpen(buffer, "rb");
    if (!userFile)
        return;

    int buf = 0;
    for (int a = 0; a < ACHIEVEMENT_MAX; ++a) {
        fRead(&buf, 4, 1, userFile);
        achievements[a].status = buf;
    }
    for (int l = 0; l < LEADERBOARD_MAX; ++l) {
        fRead(&buf, 4, 1, userFile);
        leaderboard[l].status = buf;
    }

    fClose(userFile);

    if (Engine.onlineActive) {
        // Load from online
    }
}

void WriteUserdata()
{
    char buffer[0x200];
#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_UWP
    if (!usingCWD)
        sprintf(buffer, "%s/Udata.bin", getResourcesPath());
    else
        sprintf(buffer, "%sUdata.bin", gamePath);
#else
    sprintf(buffer, "%sUdata.bin", gamePath);
#endif
    FileIO *userFile = fOpen(buffer, "wb");
    if (!userFile)
        return;

    for (int a = 0; a < ACHIEVEMENT_MAX; ++a) fWrite(&achievements[a].status, 4, 1, userFile);
    for (int l = 0; l < LEADERBOARD_MAX; ++l) fWrite(&leaderboard[l].status, 4, 1, userFile);

    fClose(userFile);

    if (Engine.onlineActive) {
        // Load from online
    }
}

void AwardAchievement(int id, int status)
{
    if (id < 0 || id >= ACHIEVEMENT_MAX)
        return;

        if (status != achievements[id].status)
            printLog("Achieved achievement: %s (%d)!", achievements[id].name, status);

    achievements[id].status = status;

    if (Engine.onlineActive) {
        // Set Achievement online
    }
    WriteUserdata();
}

void SetAchievement(int achievementID, int achievementDone)
{
    if (!Engine.trialMode && !debugMode) {
        AwardAchievement(achievementID, achievementDone);
    }
}
void SetLeaderboard(int leaderboardID, int result)
{
    if (!Engine.trialMode && !debugMode) {
        printLog("Set leaderboard (%d) value to %d", leaderboard, result);
        switch (leaderboardID) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
                leaderboard[leaderboardID].status = result;
                WriteUserdata();
                return;
        }
    }
}
