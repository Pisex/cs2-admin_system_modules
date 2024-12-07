#include <stdio.h>
#include "as_vip.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

VIP g_VIP;
PLUGIN_EXPOSE(VIP, g_VIP);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;
IVIPApi* g_pVIPApi;

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

std::map<std::string, std::string> g_mapGroups;

bool VIP::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	return true;
}

bool VIP::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnAdminConnected(int iSlot)
{
	bool bRoot = g_pAdminApi->HasPermission(iSlot, "@admin/root");
	if (bRoot)
	{
		if(g_mapGroups.find("@admin/root") != g_mapGroups.end())
		{
			g_pVIPApi->VIP_GiveClientVIP(iSlot, 0, g_mapGroups["@admin/root"].c_str(), false);
			return;
		}
	} else {
		for (auto it = g_mapGroups.begin(); it != g_mapGroups.end(); ++it)
		{
			if (g_pAdminApi->HasPermission(iSlot, it->first.c_str()))
			{
				g_pVIPApi->VIP_GiveClientVIP(iSlot, 0, it->second.c_str(), false);
				return;
			}
		}
	}
}

void LoadConfig()
{
	KeyValues* hKv = new KeyValues("VIP");
	const char *pszPath = "addons/configs/admin_system/vip.ini";

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
		return;
	}
	FOR_EACH_VALUE(hKv, pValue)
	{
		g_mapGroups[pValue->GetName()] = pValue->GetString(nullptr);
	}
}

void VIP::AllPluginsLoaded()
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
	g_pVIPApi = (IVIPApi *)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing VIP system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	LoadConfig();
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pAdminApi->OnAdminConnected(g_PLID, OnAdminConnected);
}

///////////////////////////////////////
const char* VIP::GetLicense()
{
	return "GPL";
}

const char* VIP::GetVersion()
{
	return "1.0";
}

const char* VIP::GetDate()
{
	return __DATE__;
}

const char *VIP::GetLogTag()
{
	return "VIP";
}

const char* VIP::GetAuthor()
{
	return "Pisex";
}

const char* VIP::GetDescription()
{
	return "AS VIP";
}

const char* VIP::GetName()
{
	return "[AS] VIP";
}

const char* VIP::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
