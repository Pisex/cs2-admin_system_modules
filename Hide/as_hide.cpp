#include <stdio.h>
#include "as_hide.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Hide g_Hide;
PLUGIN_EXPOSE(Hide, g_Hide);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

bool g_bHide[64];

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

bool Hide::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	return true;
}

bool Hide::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnClientAuthorized(int iSlot, uint64 iSteamID64)
{
	g_bHide[iSlot] = false;
}

void Hide::AllPluginsLoaded()
{
	char error[64];
	int ret;
	g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Utils system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pPlayersApi = (IPlayersApi *)g_SMAPI->MetaFactory(Players_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Players system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pAdminApi = (IAdminApi *)g_SMAPI->MetaFactory(Admin_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Admin system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pPlayersApi->HookOnClientAuthorized(g_PLID, OnClientAuthorized);
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->RegCommand(g_PLID, {"jointeam"}, {}, [](int iSlot, const char* szContent) {
		if(g_bHide[iSlot])
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Hide_Off"));
			g_bHide[iSlot] = false;
		}
		return false;
	});
	g_pUtils->RegCommand(g_PLID, {"mm_hide"}, {"!hide"}, [](int iSlot, const char* szContent) {
		if(g_pAdminApi->HasPermission(iSlot, "@admin/hide"))
		{
			if(g_bHide[iSlot])
			{
				g_bHide[iSlot] = false;
				g_pPlayersApi->ChangeTeam(iSlot, 1);
				g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Hide_Off"));
			}
			else
			{
				g_bHide[iSlot] = true;
				engine->ServerCommand("sv_disable_teamselect_menu 1");
				g_pUtils->CreateTimer(0.1f, [iSlot]() {
					CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(iSlot);
					if(!pPlayer) return -1.0f;
					CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
					if(!pPawn) return -1.0f;
					if(pPawn->IsAlive()) g_pPlayersApi->CommitSuicide(iSlot, false, true);
					g_pPlayersApi->ChangeTeam(iSlot, 1);
					g_pPlayersApi->ChangeTeam(iSlot, 0);
					g_pUtils->CreateTimer(0.1f, []() {
						engine->ServerCommand("sv_disable_teamselect_menu 0");
						return -1.0f;
					});
					g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Hide_On"));
					return -1.0f;
				});
			}
		}
		return true;
	});
}

///////////////////////////////////////
const char* Hide::GetLicense()
{
	return "GPL";
}

const char* Hide::GetVersion()
{
	return "1.0";
}

const char* Hide::GetDate()
{
	return __DATE__;
}

const char *Hide::GetLogTag()
{
	return "Hide";
}

const char* Hide::GetAuthor()
{
	return "Pisex";
}

const char* Hide::GetDescription()
{
	return "AS Hide";
}

const char* Hide::GetName()
{
	return "[AS] Hide";
}

const char* Hide::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
