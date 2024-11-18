#include <stdio.h>
#include "as_admins.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Admins g_Sample;
PLUGIN_EXPOSE(Admins, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenus;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

void Get_Admins(int iSlot, bool bConsole)
{
	if(bConsole)
	{
		if(iSlot == -1) META_CONPRINTF("ADMINS\nID. Name - SteamID\n");
		else g_pUtils->PrintToConsole(iSlot, "ADMINS\nID. Name - SteamID\n");
		for (int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if(!pPlayer) continue;
			if(!g_pAdminApi->IsAdmin(i)) continue;
			if(iSlot == -1) META_CONPRINTF("%i. %s - %lld\n", i, g_pPlayersApi->GetPlayerName(i), pPlayer->m_steamID());
			else g_pUtils->PrintToConsole(iSlot, "%i. %s - %lld\n", i, g_pPlayersApi->GetPlayerName(i), pPlayer->m_steamID());
		}
	}
	else
	{
		Menu hMenu;
		g_pMenus->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("Admins_Title"));
		for (int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if(!pPlayer) continue;
			if(!g_pAdminApi->IsAdmin(i)) continue;
			g_pMenus->AddItemMenu(hMenu, "", g_pPlayersApi->GetPlayerName(i), ITEM_DISABLED);
		}
		g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {

			return true;
		});
		g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
	}
}

CON_COMMAND_F(mm_admins, "", FCVAR_GAMEDLL)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/admins") && !bConsole) return;
	Get_Admins(iSlot, true);
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

bool Admins::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool Admins::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void Admins::AllPluginsLoaded()
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
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->RegCommand(g_PLID, {}, {"!admins"}, [](int iSlot, const char* szContent) {
		if(g_pAdminApi->HasPermission(iSlot, "@admin/admins")) Get_Admins(iSlot, false);
		return true;
	});
}

///////////////////////////////////////
const char* Admins::GetLicense()
{
	return "GPL";
}

const char* Admins::GetVersion()
{
	return "1.0";
}

const char* Admins::GetDate()
{
	return __DATE__;
}

const char *Admins::GetLogTag()
{
	return "Admins";
}

const char* Admins::GetAuthor()
{
	return "Pisex";
}

const char* Admins::GetDescription()
{
	return "AS Admins";
}

const char* Admins::GetName()
{
	return "[AS] Admins";
}

const char* Admins::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
