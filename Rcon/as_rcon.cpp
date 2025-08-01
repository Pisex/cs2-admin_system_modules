#include <stdio.h>
#include "as_rcon.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Rcon g_Rcon;
PLUGIN_EXPOSE(Rcon, g_Rcon);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

CON_COMMAND_F(mm_rcon, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/rcon") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[128];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageRcon"), args.Arg(0));
		if(bConsole) META_CONPRINTF("%s\n", szBuffer);
		else g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}

	std::string szCommand = args.ArgS();
	engine->ServerCommand(szCommand.c_str());
	g_pAdminApi->SendAction(iSlot, "rcon", szCommand.c_str());
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

bool Rcon::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool Rcon::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void Rcon::AllPluginsLoaded()
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
	// g_pUtils->RegCommand(g_PLID, {}, {"!rcon"}, [](int iSlot, const char* szContent) {
	// 	if(g_pAdminApi->HasPermission(iSlot, "@admin/rcon"))
	// 	{
	// 		CCommand arg;
	// 		arg.Tokenize(szContent);
	// 		if(arg.ArgC() < 2)
	// 		{
	// 			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageRcon"), arg.Arg(0));
	// 			return true;
	// 		}
	// 		std::string szCommand = arg.ArgS();
	// 		if (!szCommand.empty()) {
	// 			szCommand.pop_back();
	// 		}
	// 		engine->ServerCommand(szCommand.c_str());
	// 		g_pAdminApi->SendAction(iSlot, "rcon", szCommand.c_str());
	// 	}
	// 	return true;
	// });
}

///////////////////////////////////////
const char* Rcon::GetLicense()
{
	return "GPL";
}

const char* Rcon::GetVersion()
{
	return "1.0";
}

const char* Rcon::GetDate()
{
	return __DATE__;
}

const char *Rcon::GetLogTag()
{
	return "Rcon";
}

const char* Rcon::GetAuthor()
{
	return "Pisex";
}

const char* Rcon::GetDescription()
{
	return "AS Rcon";
}

const char* Rcon::GetName()
{
	return "[AS] Rcon";
}

const char* Rcon::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
