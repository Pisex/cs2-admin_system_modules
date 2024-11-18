#include <stdio.h>
#include "as_chat.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Chat g_Sample;
PLUGIN_EXPOSE(Chat, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

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

bool Chat::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool Chat::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void Chat::AllPluginsLoaded()
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
	g_pUtils->AddChatListenerPre(g_PLID, [](int iSlot, const char* szContent, bool bTeam) {
		if(bTeam && g_pAdminApi->HasPermission(iSlot, "@admin/chat"))
		{
			char szMessage[512];
			g_SMAPI->Format(szMessage, sizeof(szMessage), "%s", szContent+1);
			szMessage[V_strlen(szMessage) - 1] = 0;
			if(szMessage[0] == '@')
			{
				const char* szName = g_pPlayersApi->GetPlayerName(iSlot);
				for(int i = 0; i < 64; i++)
				{
					if(g_pPlayersApi->IsFakeClient(i)) continue;
					if(!g_pPlayersApi->IsConnected(i)) continue;
					if(!g_pPlayersApi->IsInGame(i)) continue;
					if(!g_pPlayersApi->IsAuthenticated(i)) continue;
					if(g_pAdminApi->IsAdmin(i) && g_pAdminApi->HasPermission(i, "@admin/chat"))
						g_pUtils->PrintToChat(i, g_pAdminApi->GetTranslation("AdminChat"), szName, szMessage+1);
				}
				return false;
			}
		}
		return true;
	});
}

///////////////////////////////////////
const char* Chat::GetLicense()
{
	return "GPL";
}

const char* Chat::GetVersion()
{
	return "1.0";
}

const char* Chat::GetDate()
{
	return __DATE__;
}

const char *Chat::GetLogTag()
{
	return "Chat";
}

const char* Chat::GetAuthor()
{
	return "Pisex";
}

const char* Chat::GetDescription()
{
	return "AS Chat";
}

const char* Chat::GetName()
{
	return "[AS] Chat";
}

const char* Chat::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
