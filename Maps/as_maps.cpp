#include <stdio.h>
#include "as_maps.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Maps g_Maps;
PLUGIN_EXPOSE(Maps, g_Maps);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenus;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

std::map<std::string, std::string> g_mapMaps;

void ChangeMap(int iSlot, const char* mapname)
{
	char szBuffer[256];
	if(!engine->IsMapValid(mapname))
	{
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), "ds_workshop_changelevel %s", mapname);
		g_pUtils->CreateTimer(5.0f, [szBuffer](){
			engine->ServerCommand(szBuffer);
			return -1.0f;
		});
	}
	else
	{
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s", mapname);
		g_pUtils->CreateTimer(5.0f, [szBuffer](){
			engine->ChangeLevel(szBuffer, nullptr);
			return -1.0f;
		});
	}
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("Changing_Map_Admin"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_mapMaps[mapname].c_str());
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("Changing_Map"), g_mapMaps[mapname].c_str());
	}
	g_pAdminApi->SendAction(iSlot, "map", mapname);
}

CON_COMMAND_F(mm_map, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/maps") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageChangeMap"), args.Arg(0));
		if(bConsole)
		{
			META_CONPRINTF("%s\n", szBuffer);
		}
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	ChangeMap(iSlot, args.Arg(1));
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

bool Maps::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
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

bool Maps::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnMapsSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("SelectMap"));
	for(auto& it : g_mapMaps)
	{
		g_pMenus->AddItemMenu(hMenu, it.first.c_str(), it.second.c_str());
	}
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			ChangeMap(iSlot, szBack);
		}
		else if(iItem == 7)
		{
			g_pAdminApi->ShowAdminLastCategoryMenu(iSlot);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void Maps::AllPluginsLoaded()
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
	g_pAdminApi->OnCoreLoaded(g_PLID, [](){
		g_pAdminApi->RegisterCategory("server", "Category_Server", nullptr);
		g_pAdminApi->RegisterItem("maps", "Item_Maps", "server", "@admin/maps", nullptr, OnMapsSelect);
	});
	
	g_pUtils->StartupServer(g_PLID, StartupServer);
	// g_pUtils->RegCommand(g_PLID, {}, {"!map"}, [](int iSlot, const char* szContent) {
	// 	if(!g_pAdminApi->HasPermission(iSlot, "@admin/maps")) return true;
	// 	CCommand arg;
	// 	arg.Tokenize(szContent);
	// 	if(arg.ArgC() < 3)
	// 	{
	// 		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageChangeMap"), arg[0]);
	// 		return true;
	// 	}
	// 	ChangeMap(iSlot, arg[1]);
	// 	return true;
	// });

	
	{
		KeyValues* hKv = new KeyValues("Maps");
		const char *pszPath = "addons/configs/admin_system/maps.ini";

		if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			g_pUtils->ErrorLog("[%s] Failed to load %s", GetLogTag(), pszPath);
			return;
		}
		FOR_EACH_VALUE(hKv, pValue)
		{
			g_mapMaps[pValue->GetString(nullptr)] = pValue->GetName();
		}
	}
}

///////////////////////////////////////
const char* Maps::GetLicense()
{
	return "GPL";
}

const char* Maps::GetVersion()
{
	return "1.0";
}

const char* Maps::GetDate()
{
	return __DATE__;
}

const char *Maps::GetLogTag()
{
	return "Maps";
}

const char* Maps::GetAuthor()
{
	return "Pisex";
}

const char* Maps::GetDescription()
{
	return "AS Maps";
}

const char* Maps::GetName()
{
	return "[AS] Maps";
}

const char* Maps::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
