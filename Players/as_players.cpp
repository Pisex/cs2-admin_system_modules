#include <stdio.h>
#include "as_players.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Players g_Players;
PLUGIN_EXPOSE(Players, g_Players);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenusApi;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

const char* szLastItem[64];
void OnItemSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
void NoclipPlayer(int iSlot, int iTarget);
void WhoPlayer(int iSlot, int iTarget, bool bConsole);
void SlapPlayer(int iSlot, int iTarget, int iDamage);
void ChangePlayerTeam(int iSlot, int iTarget, int iTeam, bool bSwitch);
void KillPlayer(int iSlot, int iTarget);
void KickPlayer(int iSlot, int iTarget);
void ChangePlayerName(int iSlot, int iTarget, const char* szName);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

bool containsOnlyDigits(const std::string &str)
{
	return str.find_first_not_of("0123456789") == std::string::npos;
}

int FindUser(const char* szIdentity)
{
	int iSlot = -1;
	for (int i = 0; i < 64; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if(!pController) continue;
		uint m_steamID = pController->m_steamID();
		if(m_steamID == 0) continue;
		if(strcasestr(engine->GetClientConVarValue(i, "name"), szIdentity) || (containsOnlyDigits(szIdentity) && m_steamID == std::stoll(szIdentity)) || (containsOnlyDigits(szIdentity) && std::stoll(szIdentity) == i))
		{
			iSlot = i;
			break;
		}
	}
	return iSlot;
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

CON_COMMAND_F(mm_kill, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/kill") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageDefault"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	g_pPlayersApi->CommitSuicide(iTarget, false, true);
	g_pAdminApi->SendAction(iSlot, "kill", std::to_string(iTarget).c_str());
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminKillMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("KillMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	}
}

CON_COMMAND_F(mm_kick, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/kick") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageDefault"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	KickPlayer(iSlot, iTarget);
}

CON_COMMAND_F(mm_slap, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/slap") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSlap"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	std::string arg2 = args.Arg(2);
	if(!arg2.empty() && (arg2.length() > 3 || !std::all_of(arg2.begin(), arg2.end(), ::isdigit))) {
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSlap"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iDamage = std::atoi(args.Arg(2));
	SlapPlayer(iSlot, iTarget, iDamage);
}

CON_COMMAND_F(mm_rename, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/rename") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageRename"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args[1]);
	if(iTarget == -1) return;
	std::string sName = args.ArgS() + 1;
	std::string arg1 = args[1];
	sName.erase(0, arg1.length());
	const char* szName = g_pPlayersApi->GetPlayerName(iTarget);
	g_pPlayersApi->SetPlayerName(iTarget, sName.c_str());
	char szBuffer[128];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %s", iTarget, sName.c_str());
	g_pAdminApi->SendAction(iSlot, "rename", szBuffer);
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminRenameMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), szName, sName.c_str());
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("RenameMessage"), szName, sName.c_str());
	}
}

CON_COMMAND_F(mm_changeteam, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/changeteam") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSwitchTeam"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	std::string arg2 = args.Arg(2);
	if(!arg2.empty() && (arg2.length() != 1 || !isdigit(arg2.at(0)))) {
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSwitchTeam"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTeam = std::atoi(args.Arg(2));
	ChangePlayerTeam(iSlot, iTarget, iTeam, false);
}

CON_COMMAND_F(mm_switchteam, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/switchteam") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSwitchTeam"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	std::string arg2 = args.Arg(2);
	if(!arg2.empty() && (arg2.length() != 1 || !isdigit(arg2.at(0)))) {
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageSwitchTeam"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTeam = std::atoi(args.Arg(2));
	ChangePlayerTeam(iSlot, iTarget, iTeam, true);
}

CON_COMMAND_F(mm_noclip, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/noclip") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageDefault"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	NoclipPlayer(iSlot, iTarget);
}

CON_COMMAND_F(mm_who, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/who") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UsageDefault"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = FindUser(args.Arg(1));
	if(iTarget == -1) return;
	WhoPlayer(iSlot, iTarget, true);
}

bool Players::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

bool Players::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void KickPlayer(int iSlot, int iTarget)
{
	engine->DisconnectClient(CPlayerSlot(iTarget), NETWORK_DISCONNECT_KICKED);
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminKickMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("KickMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	}
	g_pAdminApi->SendAction(iSlot, "kick", std::to_string(iTarget).c_str());
}

void KillPlayer(int iSlot, int iTarget)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	g_pPlayersApi->CommitSuicide(iTarget, false, true);
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminKillMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("KillMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	}
	g_pAdminApi->SendAction(iSlot, "kill", std::to_string(iTarget).c_str());
}

void ChangePlayerName(int iSlot, int iTarget, const char* szName)
{
	const char* szOldName = g_pPlayersApi->GetPlayerName(iTarget);
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminRenameMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), szOldName, szName);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("RenameMessage"), szOldName, szName);
	g_pPlayersApi->SetPlayerName(iTarget, szName);
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %s", iTarget, szName);
	g_pAdminApi->SendAction(iSlot, "rename", szBuffer);
}

void NoclipPlayer(int iSlot, int iTarget)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	if(pPlayerPawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
		g_pPlayersApi->SetMoveType(iTarget, MOVETYPE_WALK);
	else
		g_pPlayersApi->SetMoveType(iTarget, MOVETYPE_NOCLIP);
	g_pAdminApi->SendAction(iSlot, "noclip", std::to_string(iTarget).c_str());
	if(g_pAdminApi->GetMessageType() == 2)
	{
		if(pPlayerPawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminNoclipMessageEnable"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminNoclipMessageDisable"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		if(pPlayerPawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("NoclipMessageEnable"), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("NoclipMessageDisable"), g_pPlayersApi->GetPlayerName(iTarget));
	}
}

void WhoPlayer(int iSlot, int iTarget, bool bConsole)
{
	std::vector<std::string> vFlags = g_pAdminApi->GetAdminFlags(iTarget);
	if(vFlags.size() > 0)
	{
		if(bConsole)
		{
			if(iSlot == -1) META_CONPRINTF("%s\n", g_pAdminApi->GetTranslation("Item_Who_Flags"), g_pPlayersApi->GetPlayerName(iTarget));
			else g_pUtils->PrintToConsole(iSlot, "%s\n", g_pAdminApi->GetTranslation("Item_Who_Flags"), g_pPlayersApi->GetPlayerName(iTarget));
		}
		else
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Item_Who_Flags"), g_pPlayersApi->GetPlayerName(iTarget));
		for(auto& flag : vFlags)
		{
			if(bConsole)
			{
				if(iSlot == -1)
					META_CONPRINTF("%s\n", g_pAdminApi->GetFlagName(flag.c_str()));
				else
					g_pUtils->PrintToConsole(iSlot, "%s\n", g_pAdminApi->GetFlagName(flag.c_str()));
			}
			else
				g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetFlagName(flag.c_str()));
		}
	}
	else
	{
		if(bConsole)
			g_pUtils->PrintToConsole(iSlot, "%s\n", g_pAdminApi->GetTranslation("Item_Who_NoFlags"));
		else
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Item_Who_NoFlags"));
	}
	g_pAdminApi->SendAction(iSlot, "who", std::to_string(iTarget).c_str());
}

void SlapPlayer(int iSlot, int iTarget, int iDamage)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	Vector velocity = pPlayerPawn->m_vecAbsVelocity;
	velocity.x += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
	velocity.y += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
	velocity.z += rand() % 200 + 100;
	pPlayerPawn->SetAbsVelocity(velocity);
	if(iDamage > 0)
	{
		pPlayerPawn->TakeDamage(iDamage);
		if(pPlayerPawn->m_iHealth() > 0)
			g_pUtils->SetStateChanged(pPlayerPawn, "CBaseEntity", "m_iHealth");
		else
			g_pPlayersApi->CommitSuicide(iTarget, false, true);
	}
	char szBuffer[128];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, iDamage);
	g_pAdminApi->SendAction(iSlot, "slap", szBuffer);
	if(g_pAdminApi->GetMessageType() == 2)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminSlapMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), std::to_string(iDamage).c_str());
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("SlapMessage"), g_pPlayersApi->GetPlayerName(iTarget), std::to_string(iDamage).c_str());
	}
}

void ChangePlayerTeam(int iSlot, int iTarget, int iTeam, bool bSwitch)
{
	if(iTeam < 0 || iTeam > 3) return;
	if(bSwitch)
		g_pPlayersApi->SwitchTeam(iTarget, iTeam);
	else
		g_pPlayersApi->ChangeTeam(iTarget, iTeam);
	char szBuffer[128];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, iTeam);
	g_pAdminApi->SendAction(iSlot, "changeteam", szBuffer);
	constexpr const char *teams[] = {"none", "spectators", "terrorists", "counter-terrorists"};
	if(g_pAdminApi->GetMessageType() == 2)
	{
		if(bSwitch)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminSwitchTeamMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), g_pAdminApi->GetTranslation(teams[iTeam]));
		else
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminChangeTeamMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), g_pAdminApi->GetTranslation(teams[iTeam]));
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		if(bSwitch)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("SwitchTeamMessage"), g_pPlayersApi->GetPlayerName(iTarget), g_pAdminApi->GetTranslation(teams[iTeam]));
		else
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("ChangeTeamMessage"), g_pPlayersApi->GetPlayerName(iTarget), g_pAdminApi->GetTranslation(teams[iTeam]));
	}
}

void SelectTeamMenu(int iSlot, int iTarget, bool bSwitch)
{
	Menu hMenu;
	g_pMenusApi->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("SelectTeam"));
	g_pMenusApi->AddItemMenu(hMenu, "1", "SPEC");
	g_pMenusApi->AddItemMenu(hMenu, "2", "T");
	g_pMenusApi->AddItemMenu(hMenu, "3", "CT");
	g_pMenusApi->SetBackMenu(hMenu, true);
	g_pMenusApi->SetExitMenu(hMenu, true);
	g_pMenusApi->SetCallback(hMenu, [iTarget, bSwitch](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iTeam = std::atoi(szBack);
			ChangePlayerTeam(iSlot, iTarget, iTeam, bSwitch);
		}
		else if(iItem == 7)
		{
			OnItemSelect(iSlot, "players", "changeteam", "changeteam");
		}
	});
	g_pMenusApi->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void SlapMenu(int iSlot, int iTarget)
{
	Menu hMenu;
	g_pMenusApi->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("Item_Slap_Damage"));
	g_pMenusApi->AddItemMenu(hMenu, "0", "0");
	g_pMenusApi->AddItemMenu(hMenu, "1", "1");
	g_pMenusApi->AddItemMenu(hMenu, "5", "5");
	g_pMenusApi->AddItemMenu(hMenu, "10", "10");
	g_pMenusApi->AddItemMenu(hMenu, "20", "20");
	g_pMenusApi->AddItemMenu(hMenu, "50", "50");
	g_pMenusApi->AddItemMenu(hMenu, "100", "100");
	g_pMenusApi->SetBackMenu(hMenu, true);
	g_pMenusApi->SetExitMenu(hMenu, true);
	g_pMenusApi->SetCallback(hMenu, [iTarget](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iDamage = std::atoi(szBack);
			SlapPlayer(iSlot, iTarget, iDamage);
		}
		else if(iItem == 7)
		{
			OnItemSelect(iSlot, "players", "slap", "slap");
		}
	});
	g_pMenusApi->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

std::string GetRandomName()
{
    std::string sRandomName;
    for(int i = 0; i < 10; i++)
    {
        char c;
        int randValue = rand() % 52;
        if (randValue < 26) {
            c = 'a' + randValue;
        } else {
            c = 'A' + (randValue - 26);
        }
        sRandomName += c;
    }
    return sRandomName;
}

void OnItemSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
	szLastItem[iSlot] = strdup(szIdentity);
	bool bIsAlive = !strcmp(szIdentity, "kill") || !strcmp(szIdentity, "slap") || !strcmp(szIdentity, "noclip");
	Menu hMenu;
	g_pMenusApi->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("SelectPlayer"));
	for (int i = 0; i < 64; i++)
	{
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if(!pPlayerController) continue;
		CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
		if(!pPlayerPawn) continue;
		if (bIsAlive && !pPlayerPawn->IsAlive()) continue;
		if(!strcmp(szIdentity, "noclip"))
		{
			char szBuffer[128];
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%s]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_nActualMoveType() == MOVETYPE_NOCLIP?"ON":"OFF");
			g_pMenusApi->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else
			g_pMenusApi->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pPlayersApi->GetPlayerName(i));
	}
	g_pMenusApi->SetBackMenu(hMenu, true);
	g_pMenusApi->SetExitMenu(hMenu, true);
	g_pMenusApi->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iTarget = std::atoi(szBack);
			CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
			if(!pPlayerController) return;
			CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
			if(!pPlayerPawn) return;
			if(!strcmp(szLastItem[iSlot], "kill"))
			{
				KillPlayer(iSlot, iTarget);
				OnItemSelect(iSlot, "players", "kill", "kill");
			}
			else if(!strcmp(szLastItem[iSlot], "kick"))
			{
				KickPlayer(iSlot, iTarget);
				OnItemSelect(iSlot, "players", "kick", "kick");
			}
			else if(!strcmp(szLastItem[iSlot], "slap"))
				SlapMenu(iSlot, iTarget);
			else if(!strcmp(szLastItem[iSlot], "rename"))
			{
				std::string sRandomName = GetRandomName();
				ChangePlayerName(iSlot, iTarget, strdup(sRandomName.c_str()));
			}
			else if(!strcmp(szLastItem[iSlot], "changeteam"))
				SelectTeamMenu(iSlot, iTarget, false);
			else if(!strcmp(szLastItem[iSlot], "switchteam"))
				SelectTeamMenu(iSlot, iTarget, true);
			else if(!strcmp(szLastItem[iSlot], "noclip"))
			{
				NoclipPlayer(iSlot, iTarget);
				OnItemSelect(iSlot, "players", "noclip", "noclip");
			}
			else if(!strcmp(szLastItem[iSlot], "who"))
			{
				WhoPlayer(iSlot, iTarget, false);
			}
		}
		else if(iItem == 7)
		{
			g_pAdminApi->ShowAdminLastCategoryMenu(iSlot);
		}
	});
	g_pMenusApi->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void Players::AllPluginsLoaded()
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
	g_pMenusApi = (IMenusApi *)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Menus system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pPlayersApi = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
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
		g_pAdminApi->RegisterCategory("players", "Category_Players", nullptr);
		g_pAdminApi->RegisterItem("who", "Item_Who", "players", "@admin/who", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("noclip", "Item_Noclip", "players", "@admin/noclip", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("switchteam", "Item_SwitchTeam", "players", "@admin/switchteam", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("changeteam", "Item_ChangeTeam", "players", "@admin/changeteam", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("rename", "Item_Rename", "players", "@admin/rename", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("slap", "Item_Slap", "players", "@admin/slap", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("kick", "Item_Kick", "players", "@admin/kick", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("kill", "Item_Kill", "players", "@admin/kill", nullptr, OnItemSelect);
	});

	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->RegCommand(g_PLID, {}, {"!who"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/who")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageDefault"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		WhoPlayer(iSlot, iTarget, false);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!noclip"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/noclip")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageDefault"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		NoclipPlayer(iSlot, iTarget);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!switchteam"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/switchteam")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 4)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSwitchTeam"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		std::string arg2 = args[2];
		if(!arg2.empty() && (arg2.length() != 1 || !isdigit(arg2.at(0)))) {
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSwitchTeam"), args[0]);
			return true;
		}
		int iTeam = std::atoi(args[2]);
		ChangePlayerTeam(iSlot, iTarget, iTeam, true);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!changeteam"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/changeteam")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 4)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSwitchTeam"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		std::string arg2 = args[2];
		if(!arg2.empty() && (arg2.length() != 1 || !isdigit(arg2.at(0)))) {
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSwitchTeam"), args[0]);
			return true;
		}
		int iTeam = std::atoi(args[2]);
		ChangePlayerTeam(iSlot, iTarget, iTeam, false);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!rename"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/rename")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 4)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageRename"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		std::string sName = args.ArgS() + 1;
		std::string arg1 = args[1];
		sName.erase(0, arg1.length());
		if (!sName.empty()) {
			sName.pop_back();
		}
		ChangePlayerName(iSlot, iTarget, strdup(sName.c_str()));
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!slap"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/slap")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSlap"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		std::string arg2 = args[2];
		if(!arg2.empty() && (arg2.length() > 3 || !std::all_of(arg2.begin(), arg2.end(), ::isdigit))) {
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageSlap"), args[0]);
			return true;
		}
		int iDamage = std::atoi(args[2]);
		SlapPlayer(iSlot, iTarget, iDamage);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!kick"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/kick")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageDefault"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		KickPlayer(iSlot, iTarget);
		return true;
	});
	g_pUtils->RegCommand(g_PLID, {}, {"!kill"}, [](int iSlot, const char* szContent) {
		if(!g_pAdminApi->HasPermission(iSlot, "@admin/kill")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("UsageDefault"), args[0]);
			return true;
		}
		int iTarget = FindUser(args[1]);
		if(iTarget == -1) return true;
		KillPlayer(iSlot, iTarget);
		return true;
	});
}

///////////////////////////////////////
const char* Players::GetLicense()
{
	return "GPL";
}

const char* Players::GetVersion()
{
	return "1.0.1";
}

const char* Players::GetDate()
{
	return __DATE__;
}

const char *Players::GetLogTag()
{
	return "Players";
}

const char* Players::GetAuthor()
{
	return "Pisex";
}

const char* Players::GetDescription()
{
	return "AS Players";
}

const char* Players::GetName()
{
	return "[AS] Players";
}

const char* Players::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
