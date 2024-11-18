#include <stdio.h>
#include "as_status.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Status g_Sample;
PLUGIN_EXPOSE(Status, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

void Get_Status(int iSlot, bool bConsole)
{
	if(bConsole)
		g_pUtils->PrintToConsole(iSlot, "ID. Name - SteamID\n");
	else
		g_pUtils->PrintToChat(iSlot, "ID. Name - SteamID\n");
	for (int i = 0; i < 64; i++)
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
		if(!pPlayer) continue;
		if(bConsole)
			g_pUtils->PrintToConsole(iSlot, "%i. %s - %lld\n", i, g_pPlayersApi->GetPlayerName(i), pPlayer->m_steamID());
		else
			g_pUtils->PrintToChat(iSlot, "%i. %s - %lld\n", i, g_pPlayersApi->GetPlayerName(i), pPlayer->m_steamID());
	}
}

CON_COMMAND_F(mm_status, "", FCVAR_GAMEDLL)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/status") && !bConsole) return;
	if(bConsole)
	{
		META_CONPRINTF("ID. Name - SteamID\n");
		for (int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if(!pPlayer) continue;
			META_CONPRINTF("%i. %s - %lld\n", i, g_pPlayersApi->GetPlayerName(i), pPlayer->m_steamID());
		}
	}
	else Get_Status(iSlot, true);
}

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

bool Status::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool Status::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void Status::AllPluginsLoaded()
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
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->RegCommand(g_PLID, {}, {"!status"}, [](int iSlot, const char* szContent) {
		if(g_pAdminApi->HasPermission(iSlot, "@admin/status")) Get_Status(iSlot, false);
		return true;
	});
}

///////////////////////////////////////
const char* Status::GetLicense()
{
	return "GPL";
}

const char* Status::GetVersion()
{
	return "1.0";
}

const char* Status::GetDate()
{
	return __DATE__;
}

const char *Status::GetLogTag()
{
	return "Status";
}

const char* Status::GetAuthor()
{
	return "Pisex";
}

const char* Status::GetDescription()
{
	return "AS Status";
}

const char* Status::GetName()
{
	return "[AS] Status";
}

const char* Status::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
