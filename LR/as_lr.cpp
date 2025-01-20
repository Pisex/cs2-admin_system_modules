#include <stdio.h>
#include "as_lr.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

LR_Admin g_LR_Admin;
PLUGIN_EXPOSE(LR_Admin, g_LR_Admin);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenus;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;
ILRApi* g_pLRApi;

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

bool LR_Admin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register( FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY ); 

	return true;
}

bool LR_Admin::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void CheckAccessAndGive(int iSlot)
{
	if(g_pAdminApi->HasPermission(iSlot, "@admin/lr")) {
		char szBuffer[64];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), "mm_lvl_give_admin %i", iSlot);
		engine->ServerCommand(szBuffer);
	}
}

void OnAdminConnect(int iSlot)
{
	CheckAccessAndGive(iSlot);
}

void OnPlayerLoad(int iSlot, const char* SteamID)
{
	CheckAccessAndGive(iSlot);
}

void LR_Admin::AllPluginsLoaded()
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
	g_pMenus = (IMenusApi *)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Menus system plugin", GetLogTag());

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
	g_pLRApi = (ILRApi *)g_SMAPI->MetaFactory(LR_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Levels Ranks plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pAdminApi->OnAdminConnected(g_PLID, OnAdminConnect);
	g_pLRApi->HookOnPlayerLoaded(g_PLID, OnPlayerLoad);
}

///////////////////////////////////////
const char* LR_Admin::GetLicense()
{
	return "GPL";
}

const char* LR_Admin::GetVersion()
{
	return "1.0";
}

const char* LR_Admin::GetDate()
{
	return __DATE__;
}

const char *LR_Admin::GetLogTag()
{
	return "LR Admin";
}

const char* LR_Admin::GetAuthor()
{
	return "Pisex";
}

const char* LR_Admin::GetDescription()
{
	return "AS LR";
}

const char* LR_Admin::GetName()
{
	return "[AS][LR] Admin";
}

const char* LR_Admin::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
