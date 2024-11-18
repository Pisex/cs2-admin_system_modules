#include <stdio.h>
#include "as_votebkm.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

VoteBKM g_Sample;
PLUGIN_EXPOSE(VoteBKM, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenusApi;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

const char* g_szReason;
const char* g_szImmunityFlag;
const char* g_szDisableFlag;
int g_iImmunityLimit;
float g_fPercents;
int g_iMinPlayers;
int g_iTime;
int g_iCooldown;
int g_iTypeVote;
int g_iResultTimer;

int g_iTarget[64][4];

int g_iLastVote;
bool g_bActiveVote;
CTimer* g_pTimer;

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();

	for (int i = 0; i < 64; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			g_iTarget[i][j] = -1;
		}
	}
}

bool VoteBKM::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	for (int i = 0; i < 64; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			g_iTarget[i][j] = -1;
		}
	}

	return true;
}

bool VoteBKM::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void VoteMenu(int iSlot);

int GetPlayersCount()
{
	int iCount = 0;
	for (int i = 0; i < 64; i++)
	{
		if(g_pPlayersApi->IsFakeClient(i)) continue;
		if(!g_pPlayersApi->IsConnected(i)) continue;
		if(!g_pPlayersApi->IsInGame(i)) continue;
		if(g_szDisableFlag[0] && g_pAdminApi->HasPermission(i, g_szDisableFlag)) return -1;
		iCount++;
	}
	return iCount;
}

void CheckPunishment(int iTarget)
{
	int iCount = GetPlayersCount();
	if(iCount < g_iMinPlayers) return;
	int iNeeded = ceil(iCount*g_fPercents);
	if(iNeeded <= 0) return;
	int iBans = 0;
	int iKicks = 0;
	int iMutes = 0;
	int iGags = 0;
	for (int i = 0; i < 64; i++)
	{
		if(g_pPlayersApi->IsFakeClient(i)) continue;
		if(!g_pPlayersApi->IsConnected(i)) continue;
		if(!g_pPlayersApi->IsInGame(i)) continue;
		if(g_iTarget[i][0] == iTarget) iBans++;
		if(g_iTarget[i][3] == iTarget) iKicks++;
		if(g_iTarget[i][1] == iTarget) iMutes++;
		if(g_iTarget[i][2] == iTarget) iGags++;
	}
	bool bPunish = false;
	if(iBans >= iNeeded)
	{
		g_pAdminApi->AddPlayerPunishment(iTarget, RT_BAN, g_iTime, g_szReason);
		bPunish = true;
	}
	else if(iKicks >= iNeeded)
	{
		g_pAdminApi->SendAction(-1, "kick", std::to_string(iTarget).c_str());
		bPunish = true;
	}
	else if(iMutes >= iNeeded)
	{
		g_pAdminApi->AddPlayerPunishment(iTarget, RT_MUTE, g_iTime, g_szReason);
		bPunish = true;
	}
	else if(iGags >= iNeeded)
	{
		g_pAdminApi->AddPlayerPunishment(iTarget, RT_GAG, g_iTime, g_szReason);
		bPunish = true;
	}
	if(bPunish)
	{
		g_bActiveVote = false;
		g_iLastVote = std::time(nullptr) + g_iCooldown;
		for (int i = 0; i < 64; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if(g_iTarget[i][j] == iTarget) g_iTarget[i][j] = -1;
			}
		}
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Success"), g_pPlayersApi->GetPlayerName(iTarget));
		if(iKicks >= iNeeded) engine->DisconnectClient(CPlayerSlot(iTarget), NETWORK_DISCONNECT_KICKED);
	}
}

void CreateVoteMenu(int iSlot, int iTarget, int iType)
{
	if(g_iLastVote > std::time(nullptr))
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_CoolDown"), g_iLastVote - std::time(nullptr));
		return;
	}
	if(g_bActiveVote)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_AlreadyActive"));
		return;
	}
	g_bActiveVote = true;
	g_pTimer = g_pUtils->CreateTimer(g_iResultTimer, [iTarget, iType]() {
		g_pTimer = nullptr;
		g_bActiveVote = false;
		int iCount = 0;
		int iNeeded = ceil(GetPlayersCount()*g_fPercents);
		for (int i = 0; i < 64; i++)
		{
			if(g_pPlayersApi->IsFakeClient(i)) continue;
			if(!g_pPlayersApi->IsConnected(i)) continue;
			if(!g_pPlayersApi->IsInGame(i)) continue;
			if(g_iTarget[i][iType] == iTarget) iCount++;
		}
		if(iCount >= iNeeded)
		{
			CheckPunishment(iTarget);
		}
		else
		{
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Canceled"));
		}
		return -1.0f;
	});
	for (int i = 0; i < 64; i++)
	{
		if(g_pPlayersApi->IsFakeClient(i)) continue;
		if(!g_pPlayersApi->IsConnected(i)) continue;
		if(!g_pPlayersApi->IsInGame(i)) continue;
		Menu hMenu;
		char szBuffer[64];
		switch (iType)
		{
			case 0:
				g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("VoteBKM_Title_Ban"), g_pPlayersApi->GetPlayerName(iTarget));
				break;
			case 1:
				g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("VoteBKM_Title_Mute"), g_pPlayersApi->GetPlayerName(iTarget));
				break;
			case 2:
				g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("VoteBKM_Title_Gag"), g_pPlayersApi->GetPlayerName(iTarget));
				break;
			case 3:
				g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("VoteBKM_Title_Kick"), g_pPlayersApi->GetPlayerName(iTarget));
				break;
		}
		g_pMenusApi->SetTitleMenu(hMenu, szBuffer);
		g_pMenusApi->AddItemMenu(hMenu, "0", g_pAdminApi->GetTranslation("VoteBKM_Yes"));
		g_pMenusApi->AddItemMenu(hMenu, "1", g_pAdminApi->GetTranslation("VoteBKM_No"));
		g_pMenusApi->SetBackMenu(hMenu, false);
		g_pMenusApi->SetExitMenu(hMenu, true);
		g_pMenusApi->SetCallback(hMenu, [iTarget, iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
			if(iItem < 7)
			{
				int iVote = std::atoi(szBack);
				if(iVote == 0)
				{
					g_iTarget[iSlot][iType] = iTarget;
					g_pMenusApi->ClosePlayerMenu(iSlot);
					int iCount = 0;
					int iNeeded = ceil(GetPlayersCount()*g_fPercents);
					for (int i = 0; i < 64; i++)
					{
						if(g_pPlayersApi->IsFakeClient(i)) continue;
						if(!g_pPlayersApi->IsConnected(i)) continue;
						if(!g_pPlayersApi->IsInGame(i)) continue;
						if(g_iTarget[i][iType] == iTarget) iCount++;
					}
					switch (iType)
					{
						case 0:
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Ban"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
							break;
						case 1:
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Mute"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
							break;
						case 2:
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Gag"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
							break;
						case 3:
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Kick"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
							break;
					}
					if(iCount >= iNeeded)
					{
						if(g_pTimer)
						{
							g_pUtils->RemoveTimer(g_pTimer);
							g_pTimer = nullptr;
						}
						CheckPunishment(iTarget);
					}
				}
				else if(iVote == 1)
				{
					g_pMenusApi->ClosePlayerMenu(iSlot);
				}
			}
		});
		g_pMenusApi->DisplayPlayerMenu(hMenu, i);
	}
}

void PlayersMenu(int iSlot, int iType)
{
	if(g_iLastVote > std::time(nullptr))
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_CoolDown"), g_iLastVote - std::time(nullptr));
		return;
	}
	Menu hMenu;
	g_pMenusApi->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("VoteBKM_SelectPlayer"));
	bool bFound = false;
	for (int i = 0; i < 64; i++)
	{
		if(g_pPlayersApi->IsFakeClient(i)) continue;
		if(!g_pPlayersApi->IsConnected(i)) continue;
		if(!g_pPlayersApi->IsInGame(i)) continue;
		if(iType != 3 && g_pAdminApi->IsPlayerPunished(i, iType)) continue;
		if(g_pAdminApi->HasPermission(i, g_szImmunityFlag)) continue;
		if(g_pAdminApi->GetAdminImmunity(i) >= g_iImmunityLimit) continue;
		if(i == iSlot) continue;
		g_pMenusApi->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pPlayersApi->GetPlayerName(i));
		bFound = true;
	}
	if(!bFound)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_NoPlayers"));
		return;
	}
	g_pMenusApi->SetBackMenu(hMenu, true);
	g_pMenusApi->SetExitMenu(hMenu, true);
	g_pMenusApi->SetCallback(hMenu, [iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			if(g_iLastVote > std::time(nullptr))
			{
				g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_CoolDown"), g_iLastVote - std::time(nullptr));
				return;
			}
			int iTarget = std::atoi(szBack);
			g_iTarget[iSlot][iType] = iTarget;
			g_pMenusApi->ClosePlayerMenu(iSlot);
			if(!g_iTypeVote)
			{
				int iCount = 0;
				int iNeeded = ceil(GetPlayersCount()*g_fPercents);
				for (int i = 0; i < 64; i++)
				{
					if(g_pPlayersApi->IsFakeClient(i)) continue;
					if(!g_pPlayersApi->IsConnected(i)) continue;
					if(!g_pPlayersApi->IsInGame(i)) continue;
					if(g_iTarget[i][iType] == iTarget) iCount++;
				}
				switch (iType)
				{
					case 0:
						g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Ban"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
						break;
					case 1:
						g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Mute"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
						break;
					case 2:
						g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Gag"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
						break;
					case 3:
						g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Action_Kick"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iCount, iNeeded);
						break;
				}
				CheckPunishment(iTarget);
			} else {
				switch (iType)
				{
					case 0:
						if(g_pAdminApi->GetMessageType() == 1)
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Ban"), g_pPlayersApi->GetPlayerName(iTarget));
						else
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Ban_Admin"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
						break;
					case 1:
						if(g_pAdminApi->GetMessageType() == 1)
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Mute"), g_pPlayersApi->GetPlayerName(iTarget));
						else
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Mute_Admin"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
						break;
					case 2:
						if(g_pAdminApi->GetMessageType() == 1)
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Gag"), g_pPlayersApi->GetPlayerName(iTarget));
						else
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Gag_Admin"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
						break;
					case 3:
						if(g_pAdminApi->GetMessageType() == 1)
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Kick"), g_pPlayersApi->GetPlayerName(iTarget));
						else
							g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("VoteBKM_Created_Kick_Admin"), g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
						break;
				}
				CreateVoteMenu(iSlot, iTarget, iType);
			}
		}
		else if(iItem == 7)
		{
			VoteMenu(iSlot);
		}
	});
	g_pMenusApi->DisplayPlayerMenu(hMenu, iSlot);
}

void VoteMenu(int iSlot)
{
	if(g_iLastVote > std::time(nullptr))
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_CoolDown"), g_iLastVote - std::time(nullptr));
		return;
	}
	int iCount = GetPlayersCount();
	if(iCount == -1)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_Admin"));
		return;
	}
	if(iCount < g_iMinPlayers)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("VoteBKM_NotEnoughPlayers"));
		return;
	}
	Menu hMenu;
	g_pMenusApi->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("VoteBKM_Title"));
	g_pMenusApi->AddItemMenu(hMenu, "0", g_pAdminApi->GetTranslation("VoteBKM_Ban"));
	g_pMenusApi->AddItemMenu(hMenu, "3", g_pAdminApi->GetTranslation("VoteBKM_Kick"));
	g_pMenusApi->AddItemMenu(hMenu, "1", g_pAdminApi->GetTranslation("VoteBKM_Mute"));
	g_pMenusApi->AddItemMenu(hMenu, "2", g_pAdminApi->GetTranslation("VoteBKM_Gag"));
	g_pMenusApi->SetBackMenu(hMenu, false);
	g_pMenusApi->SetExitMenu(hMenu, true);
	g_pMenusApi->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iType = std::atoi(szBack);
			PlayersMenu(iSlot, iType);
		}
	});
	g_pMenusApi->DisplayPlayerMenu(hMenu, iSlot);
}

std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

void LoadConfig()
{
	KeyValues* hKv = new KeyValues("VoteBKM");
	const char *pszPath = "addons/configs/admin_system/votebkm.ini";

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
		return;
	}

	g_szImmunityFlag = hKv->GetString("immunity_flag", "@admin/votebkm_i");
	g_szDisableFlag = hKv->GetString("disable_flag", "@admin/votebkm_d");
	g_iImmunityLimit = hKv->GetInt("immunity_limit", 10);
	g_fPercents = static_cast<float>(hKv->GetInt("percents", 40)) / 100;
	g_iMinPlayers = hKv->GetInt("min_players", 4);
	g_iTime = hKv->GetInt("time_punish", 3600);
	g_iCooldown = hKv->GetInt("cooldown", 300);
	g_iTypeVote = hKv->GetInt("type_vote", 0);
	g_iResultTimer = hKv->GetInt("result_timer", 30);
	g_szReason = hKv->GetString("reason", "VoteBKM");
	const char* szCommands = hKv->GetString("commands", "");
	std::vector<std::string> vecCommands = split(std::string(szCommands), "|");
	g_pUtils->RegCommand(g_PLID, {}, vecCommands, [](int iSlot, const char* szContent) {
		if(g_pAdminApi->HasPermission(iSlot, "@admin/votebkm"))
			VoteMenu(iSlot);
		return true;
	});
}

void VoteBKM::AllPluginsLoaded()
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
	g_pMenusApi = (IMenusApi *)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Menus system plugin", GetLogTag());

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
	g_pUtils->HookEvent(g_PLID, "player_disconnect", [](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
		int iTarget = pEvent->GetInt("userid");
		for (int i = 0; i < 64; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if(g_iTarget[i][j] == iTarget) g_iTarget[i][j] = -1;
			}
			CheckPunishment(i);
		}
		return false;
	});
	LoadConfig();
}

///////////////////////////////////////
const char* VoteBKM::GetLicense()
{
	return "GPL";
}

const char* VoteBKM::GetVersion()
{
	return "1.0";
}

const char* VoteBKM::GetDate()
{
	return __DATE__;
}

const char *VoteBKM::GetLogTag()
{
	return "VoteBKM";
}

const char* VoteBKM::GetAuthor()
{
	return "Pisex";
}

const char* VoteBKM::GetDescription()
{
	return "AS VoteBKM";
}

const char* VoteBKM::GetName()
{
	return "[AS] VoteBKM";
}

const char* VoteBKM::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
