#include <stdio.h>
#include "as_admin_time.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

ATime g_ATime;
PLUGIN_EXPOSE(ATime, g_ATime);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

IMySQLConnection* g_pConnection;

int g_iServerID;
bool g_bReset;

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

void LoadConfig()
{
	KeyValues* hKv = new KeyValues("Config");
	const char *pszPath = "addons/configs/admin_system/admin_time.ini";

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
		return;
	}
	g_iServerID = hKv->GetInt("server_id");
	g_bReset = hKv->GetBool("reset", true);
}

bool ATime::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool ATime::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnAdminCoreLoaded()
{
	g_pConnection = g_pAdminApi->GetMySQLConnection();
	if(g_pConnection)
	{
		g_pConnection->Query("CREATE TABLE IF NOT EXISTS `as_admin_time` (\
			`id` INT PRIMARY KEY AUTO_INCREMENT,\
			`admin_id` VARCHAR(32) NOT NULL,\
			`admin_name` VARCHAR(64) NOT NULL,\
			`connect_time` INT NOT NULL,\
			`disconnect_time` INT NOT NULL DEFAULT -1 COMMENT '-1 = Admin online',\
			`played_time` INT NOT NULL DEFAULT 0,\
			`server_id` VARCHAR(32) NOT NULL\
		);",[](ISQLQuery *){});

		
		char szQuery[256];
		g_SMAPI->Format(szQuery, sizeof(szQuery), "UPDATE `as_admin_time` SET `disconnect_time` = %d, `played_time` = %d - `connect_time` WHERE `disconnect_time` = -1 AND `server_id` = '%d';", time(nullptr), time(nullptr), g_iServerID);
		g_pConnection->Query(szQuery,[](ISQLQuery *){});
	}
}

void OnPlayerDisconnect(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
	int iSlot = pEvent->GetInt("userid");
	uint64 iSteamID64 = g_pPlayersApi->GetSteamID64(iSlot);
	
	if(g_bReset) {
		char szQuery[256];
		g_SMAPI->Format(szQuery, sizeof(szQuery), "UPDATE `as_admin_time` SET `disconnect_time` = %d, `played_time` = %d - `connect_time` WHERE `admin_id` = '%llu' AND `disconnect_time` = -1 AND `server_id` = '%d';", time(nullptr), time(nullptr), iSteamID64, g_iServerID);
		g_pConnection->Query(szQuery,[](ISQLQuery *){});
	}
}

void OnAdminConnect(int iSlot) {
	if(!g_pAdminApi->IsAdmin(iSlot)) return;
	uint64 iSteamID64 = g_pPlayersApi->GetSteamID64(iSlot);
	char szQuery[256];
	g_SMAPI->Format(szQuery, sizeof(szQuery), "INSERT INTO `as_admin_time` (`admin_id`, `admin_name`, `connect_time`, `server_id`) VALUES ('%llu', '%s', %d, '%d');", iSteamID64, g_pPlayersApi->GetPlayerName(iSlot), time(nullptr), g_iServerID);
	g_pConnection->Query(szQuery,[](ISQLQuery *){});
}

void ATime::AllPluginsLoaded()
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
	LoadConfig();
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->HookEvent(g_PLID, "player_disconnect", OnPlayerDisconnect);
	g_pAdminApi->OnCoreLoaded(g_PLID, OnAdminCoreLoaded);
	g_pAdminApi->OnAdminConnected(g_PLID, OnAdminConnect);
}

///////////////////////////////////////
const char* ATime::GetLicense()
{
	return "GPL";
}

const char* ATime::GetVersion()
{
	return "1.0.2";
}

const char* ATime::GetDate()
{
	return __DATE__;
}

const char *ATime::GetLogTag()
{
	return "ATime";
}

const char* ATime::GetAuthor()
{
	return "Pisex";
}

const char* ATime::GetDescription()
{
	return "AS ATime";
}

const char* ATime::GetName()
{
	return "[AS] Admin Time";
}

const char* ATime::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
