#include <stdio.h>
#include "as_checkcheats.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

CheckCheats g_Sample;
PLUGIN_EXPOSE(CheckCheats, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenus;
IPlayersApi* g_pPlayers;
IAdminApi* g_pAdmin;

///////////////////////////////////////////
///////////////////CONFIG//////////////////
///////////////////////////////////////////
std::map<int, Reasons> g_mReasons;
std::map<std::string, Socials> g_mSocials;
Timer g_Timer;
Leave g_Leave;

const char* g_szSound;
bool g_bAutoMove;

int g_iStart[64];

int g_iImmunityType;
const char* g_szImmunityFlag;

bool g_bAdminMenuType;
const char* g_szAdminMenuCategory;
const char* g_szAdminMenuFlag;

const char* g_szContactCommand;

const char* g_szOverlay;

bool g_bDB;
int g_iServerID;
///////////////////////////////////////////
///////////////////////////////////////////
///////////////////////////////////////////


///////////////////////////////////////////
/////////////////USER DATA/////////////////
///////////////////////////////////////////

int g_iTimer[64];

const char* g_szSocial[64];
int g_iTarget[64] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int g_iStage[64] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

const char* g_szContact[64];

bool g_bBlockTeamChange[64];

bool g_bAdmin[64];

CHandle<CParticleSystem> g_hOverlays[64];

///////////////////////////////////////////
///////////////////////////////////////////
///////////////////////////////////////////

int g_iCheckTransmit;

SH_DECL_MANUALHOOK8_void(CheckTransmit, 13, 0, 0, ISource2GameEntities*, CCheckTransmitInfoHack**, uint32_t, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, uint32_t);


void CheckMenu(int iSlot);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void SetStage(int iSlot, int iStage)
{
	g_iStage[iSlot] = iStage;
	if(g_iTarget[iSlot] != -1) g_iStage[g_iTarget[iSlot]] = iStage;
	
	if(iStage == g_Timer.iMin)
	{
		g_iTimer[iSlot] = g_Timer.iTime;
		if(g_iTarget[iSlot] != -1) g_iTimer[g_iTarget[iSlot]] = g_Timer.iTime;
	}
	else if(iStage == g_Timer.iMax)
	{
		g_iTimer[iSlot] = 0;
		if(g_iTarget[iSlot] != -1) g_iTimer[g_iTarget[iSlot]] = 0;
	}
	if(iStage != -1 && g_iTarget[iSlot] != -1)
	{
		CheckMenu(g_bAdmin[iSlot]?iSlot:g_iTarget[iSlot]);
	}
	if(iStage == 1)
	{
		if(g_iTarget[iSlot] != -1) g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Input_Contact"), g_szContactCommand, g_mSocials[g_szSocial[iSlot]].sExample.c_str());
		g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Input_Contact"), g_szContactCommand, g_mSocials[g_szSocial[iSlot]].sExample.c_str());
	}
}

void SetSocial(int iSlot, const char* szSocial)
{
	g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Select_Social"), szSocial);
	g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Select_Social"), szSocial);
	g_szSocial[iSlot] = szSocial;
	g_szSocial[g_iTarget[iSlot]] = szSocial;
	g_szContact[iSlot] = "";
	g_szContact[g_iTarget[iSlot]] = "";
	SetStage(iSlot, 1);
}

void ResetUserData(int iSlot)
{
	g_iTimer[iSlot] = 0;
	g_iTarget[iSlot] = -1;
	g_szContact[iSlot] = "";
	g_szSocial[iSlot] = "";
	g_bBlockTeamChange[iSlot] = false;
	g_bAdmin[iSlot] = false;
	SetStage(iSlot, -1);
	g_pMenus->ClosePlayerMenu(iSlot);
}

void SetContact(int iSlot, const char* szContact)
{
	g_szContact[iSlot] = szContact;
	if(g_iTarget[iSlot] != -1) g_szContact[g_iTarget[iSlot]] = szContact;
	SetStage(iSlot, 2);
}

void SetTarget(int iSlot, int iTarget)
{
	g_iTarget[iSlot] = iTarget;
	g_iTarget[iTarget] = iSlot;
	SetStage(iSlot, 0);
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

bool CheckCheats::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);

	// SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &CheckCheats::OnCheckTransmit), true);
	g_iCheckTransmit = SH_ADD_MANUALDVPHOOK(CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &CheckCheats::OnCheckTransmit), true);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL);

	return true;
}

void CheckCheats::OnCheckTransmit(class ISource2GameEntities* pThis, class CCheckTransmitInfoHack** ppInfoList, uint32_t infoCount, CBitVec<16384>& unionTransmitEdicts1, CBitVec<16384>& unionTransmitEdicts2, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, uint32_t nEntities)
{
	if (!g_pEntitySystem || !g_szOverlay[0])
		return;

	for (auto i = 0u; i < infoCount; i++)
	{
		const auto& pInfo = ppInfoList[i];
		int iPlayerSlot = pInfo->m_nPlayerSlot;
		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(iPlayerSlot);
		if (!pSelfController || !pSelfController->IsConnected())
			continue;
		
		for (int j = 0; j < 64; j++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(j);
			if (!pController || j == iPlayerSlot) continue;
			if(g_hOverlays[j])
			{
				CParticleSystem* pEnt = g_hOverlays[j].Get();
				pInfo->m_pTransmitEntity->Clear(pEnt->entindex());
			}
		}
	}
}

bool CheckCheats::Unload(char *error, size_t maxlen)
{
	// SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &CheckCheats::OnCheckTransmit), true);
	SH_REMOVE_HOOK_ID(g_iCheckTransmit);
	ConVar_Unregister();
	
	return true;
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

void EndCheckMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("CC_Title_EndCheck"));
	for(auto& it : g_mReasons)
	{
		if(it.second.bShow)
		{
			g_pMenus->AddItemMenu(hMenu, std::to_string(it.first).c_str(), it.second.sReason.c_str(), (it.second.iMin == -1 || it.second.iMin <= g_iStage[iSlot]) && (it.second.iMax == -1 || g_iStage[iSlot] < it.second.iMax)?ITEM_DEFAULT:ITEM_DISABLED);
		}
	}
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{ 
			int iTarget = g_iTarget[iSlot];
			if(g_hOverlays[iTarget])
				g_pUtils->RemoveEntity(g_hOverlays[iTarget]);
			g_hOverlays[iTarget] = nullptr;
			int iReason = std::atoi(szBack);
			if(g_mReasons.find(iReason) != g_mReasons.end())
			{
				Reasons sReason = g_mReasons[iReason];
				if((sReason.iMin <= g_iStage[iSlot] || sReason.iMin == -1) && (sReason.iMax > g_iStage[iSlot] || sReason.iMax == -1))
				{
					if(g_bDB) {
						char szBuffer[1024];
						g_SMAPI->Format(szBuffer, sizeof(szBuffer), "INSERT INTO `checkcheats_stats` (\
							`server_id`, `player_steamid`, `player_name`, `admin_steamid`, `admin_name`, `datestart`, `date_end`, `verdict`, `suspect_discord`) VALUES (\
							'%i', '%lld', '%s', '%lld', '%s', '%i', '%i', '%s', '%s')",
							g_iServerID, 
							g_pPlayers->GetSteamID64(iTarget),
							g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(iTarget)).c_str(),
							g_pPlayers->GetSteamID64(iSlot),
							g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(iSlot)).c_str(),
							g_iStart[iSlot],
							std::time(nullptr),
							sReason.sReason.c_str(),
							g_pAdmin->GetMySQLConnection()->Escape(g_szContact[iSlot]).c_str());
						g_pAdmin->GetMySQLConnection()->Query(szBuffer, [](ISQLQuery*){});
					}
					char szBuffer[128];
					g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i %s", iTarget, sReason.iTime, sReason.sReason2.c_str());
					g_pAdmin->SendAction(iSlot, "checkcheats", szBuffer);
					if(sReason.iTime == -1)
					{
						g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_End_Good_Admin"));
						g_pUtils->PrintToChat(iTarget, g_pAdmin->GetTranslation("CC_End_Good"));
					}
					else g_pAdmin->AddPlayerPunishment(iTarget, RT_BAN, sReason.iTime, sReason.sReason2.c_str(), iSlot);
					ResetUserData(iTarget);
					ResetUserData(iSlot);
				}
			}
		}
		else if(iItem == 7)
		{
			CheckMenu(iSlot);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void CheckMenu(int iSlot)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("CC_Title_Main"));
	char szBuffer[64];
	if(g_iStage[iSlot] < 1)
	{
		g_pMenus->AddItemMenu(hMenu, "0", g_pAdmin->GetTranslation("CC_Notify_Socials"));
	}
	if(g_iStage[iSlot] < 2 && g_iStage[iSlot] > 0)
	{
		g_pMenus->AddItemMenu(hMenu, "1", g_pAdmin->GetTranslation("CC_Notify_Contact"));
	}
	g_pMenus->AddItemMenu(hMenu, "3", g_pAdmin->GetTranslation("CC_Stage_Start"), (g_iStage[iSlot] < 3 && g_iStage[iSlot] >= 2)?ITEM_DEFAULT:ITEM_DISABLED);
	if(!g_bAutoMove)
	{
		g_pMenus->AddItemMenu(hMenu, "2", g_pAdmin->GetTranslation("CC_ChangeSpecItem"));
		g_pMenus->AddItemMenu(hMenu, "5", g_pAdmin->GetTranslation("CC_BlockChangeTeam_Item"));
	}
	g_pMenus->AddItemMenu(hMenu, "4", g_pAdmin->GetTranslation("CC_EndCheck_Item"));
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iItem2 = std::atoi(szBack);
			switch(iItem2)
			{
				case 0:
					g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_Socials_Admin"));
					g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Notify_Socials_User"));
					break;
				case 1:
					g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_Contact_Admin"));
					g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Input_Contact"), g_szContactCommand, g_mSocials[g_szSocial[iSlot]].sExample.c_str());
					break;
				case 2:
					g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_Spec_Admin"));
					g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Notify_Spec_User"));
					g_pPlayers->CommitSuicide(g_iTarget[iSlot], false, true);
					g_pPlayers->ChangeTeam(g_iTarget[iSlot], 1);
					break;
				case 3:
					SetStage(iSlot, 3);
					g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_Start_Admin"));
					g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Notify_Start_User"));
					break;
				case 4:
					EndCheckMenu(iSlot);
					break;
				case 5:
					g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_Block_Team_Admin"));
					g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_Notify_Block_Team_User"));
					g_bBlockTeamChange[iSlot] = true;
					break;
			}
			
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void StartCheck(int iSlot, int iTarget)
{
	g_iStart[iSlot] = std::time(0);
	g_iStart[iTarget] = std::time(0);
	// g_pUtils->PrintToChatAll(g_pAdmin->GetTranslation("CC_Notify_All_Start"), g_pPlayers->GetPlayerName(iSlot), g_pPlayers->GetPlayerName(iTarget));
	// g_pUtils->PrintToChat(iTarget, g_pAdmin->GetTranslation("CC_Start_Warn"));
	if(g_szSound[0]) engine->ClientCommand(iTarget, "play %s", g_szSound);
	if(g_szOverlay[0])
	{
		// CCSPlayerController* pController = CCSPlayerController::FromSlot(iTarget);
		// if(!pController) return;
		// CCSPlayerPawn* pPlayerPawn = pController->GetPlayerPawn();
		// if(!pPlayerPawn) return;
		// CParticleSystem* pEnt = (CParticleSystem*)g_pUtils->CreateEntityByName("info_particle_system", -1);
		// if(!pEnt) return;
		// pEnt->m_bStartActive(true);
		// pEnt->m_iszEffectName(g_szOverlay);
		// Vector vecOrigin = pPlayerPawn->GetAbsOrigin();
		// g_pUtils->TeleportEntity(pEnt, &vecOrigin, nullptr, nullptr);
		// g_pUtils->DispatchSpawn(pEnt, nullptr);
		// g_pUtils->AcceptEntityInput(pEnt, "FollowEntity", "!activator", pPlayerPawn, pEnt);
		// g_hOverlays[iTarget] = CHandle<CParticleSystem>(pEnt);
	}
	if(g_bAutoMove)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Notify_SpecAndBlock_Admin"));
		g_pUtils->PrintToChat(iTarget, g_pAdmin->GetTranslation("CC_Notify_SpecAndBlock_User"));
		g_pPlayers->CommitSuicide(iTarget, false, true);
		g_pPlayers->ChangeTeam(iTarget, 1);
		g_bBlockTeamChange[iSlot] = true;
	}
	if(g_mSocials.size() == 1)
	{
		SetSocial(iSlot, g_mSocials.begin()->first.c_str());
	}
	else
	{
		Menu hMenu;
		g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("CC_Title_Socials"));
		for(auto& it : g_mSocials)
		{
			g_pMenus->AddItemMenu(hMenu, it.first.c_str(), it.second.sName.c_str());
		}
		g_pMenus->SetBackMenu(hMenu, false);
		g_pMenus->SetExitMenu(hMenu, false);
		g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
			if(iItem < 7)
			{
				SetSocial(iSlot, strdup(szBack));
				g_pMenus->ClosePlayerMenu(iSlot);
			}
		});
		g_pMenus->DisplayPlayerMenu(hMenu, iTarget);
	}
	if(iSlot != iTarget) CheckMenu(iSlot);
}

void ShowMainMenu(int iSlot, bool bCommand = false)
{
	if(g_iTarget[iSlot] != -1)
	{
		CheckMenu(iSlot);
		return;
	}
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("CC_SelectPlayer"));
	bool bFound = false;
	for (int i = 0; i < 64; i++)
	{
		if(g_iTarget[i] != -1) continue;
		if(g_pPlayers->IsFakeClient(i)) continue;
		if(!g_pPlayers->IsConnected(i)) continue;
		if(!g_pPlayers->IsInGame(i)) continue;
		if(g_pAdmin->HasPermission(i, g_szImmunityFlag)) continue;
		if(i == iSlot) continue;
		g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pPlayers->GetPlayerName(i));
		bFound = true;
	}
	if(!bFound)
	{
		g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_NoPlayers"));
		return;
	}
	if(!bCommand) g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [bCommand](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iTarget = std::atoi(szBack);
			ResetUserData(iSlot);
			ResetUserData(iTarget);
			g_bAdmin[iSlot] = true;
			SetTarget(iSlot, iTarget);
			StartCheck(iSlot, iTarget);
		}
		else if(iItem == 7 && !bCommand)
		{
			g_pAdmin->ShowAdminLastCategoryMenu(iSlot);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

std::string StripQuotes(const std::string& str) {
	if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
		return str.substr(1, str.length() - 2);
	}
	return str;
}

std::string TrimTrailingQuote(const std::string& str) {
	if (!str.empty() && str.back() == '"') {
		return str.substr(0, str.length() - 1);
	}
	return str;
}

std::vector<std::string> SplitStringBySpace(const std::string& input) {
	std::vector<std::string> tokens;
	std::string current;
	bool inQuotes = false;

	for (size_t i = 0; i < input.size(); ++i) {
		char ch = input[i];

		if (ch == '"') {
			inQuotes = !inQuotes;
			continue;
		}

		if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
		} else {
			current += ch;
		}
	}

	if (!current.empty()) {
		tokens.push_back(current);
	}

	return tokens;
}

void LoadConfig()
{
	KeyValues* hKv = new KeyValues("Config");
	const char *pszPath = "addons/configs/admin_system/checkcheats.ini";

	if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
		return;
	}

	KeyValues* hReasons = hKv->FindKey("end");
	if (hReasons)
	{
		FOR_EACH_TRUE_SUBKEY(hReasons, pValue)
		{
			int iIndex = std::atoi(pValue->GetName());
			Reasons sReason;
			sReason.sReason = pValue->GetString("reason");
			sReason.sReason2 = pValue->GetString("reason2");
			sReason.iTime = pValue->GetInt("time");
			sReason.iMin = pValue->GetInt("min");
			sReason.iMax = pValue->GetInt("max");
			sReason.bShow = pValue->GetBool("show");
			g_mReasons[iIndex] = sReason;
		}
	}

	KeyValues* hSocials = hKv->FindKey("socials");
	if (hSocials)
	{
		FOR_EACH_TRUE_SUBKEY(hSocials, pValue)
		{
			std::string sName = pValue->GetName();
			Socials sSocial;
			sSocial.sName = sName;
			sSocial.iMin = pValue->GetInt("min");
			sSocial.iMax = pValue->GetInt("max");
			sSocial.sExample = pValue->GetString("example");
			g_mSocials[sName] = sSocial;
		}
	}

	KeyValues* hTimer = hKv->FindKey("timer");
	if (hTimer)
	{
		g_Timer.iTime = hTimer->GetInt("time");
		g_Timer.iMin = hTimer->GetInt("min");
		g_Timer.iMax = hTimer->GetInt("max");
		g_Timer.bAutoBan = hTimer->GetBool("auto_ban_status");
		g_Timer.iBanReason = hTimer->GetInt("auto_ban_reason");
	}

	KeyValues* hAdmin = hKv->FindKey("admin");
	if (hAdmin)
	{
		g_iImmunityType = hAdmin->GetInt("immunity_type");
		g_szImmunityFlag = hAdmin->GetString("immunity_flag");
		g_szAdminMenuFlag = hAdmin->GetString("check_flag");
	}

	KeyValues* hMenu = hKv->FindKey("admin_menu");
	if (hMenu)
	{
		g_bAdminMenuType = hMenu->GetBool("type");
		g_szAdminMenuCategory = hMenu->GetString("category");
	}
	
	KeyValues* hLeave = hKv->FindKey("leave");
	if (hLeave)
	{
		g_Leave.bAutoBan = hLeave->GetBool("player_ban_status");
		g_Leave.iBanReason = hLeave->GetInt("player_ban_reason");
		g_Leave.bLeaveMenu = hLeave->GetBool("player_menu");
		g_Leave.iMin = hLeave->GetInt("player_ban_min");
		g_Leave.iMax = hLeave->GetInt("player_ban_max");
	}
	g_szOverlay = hKv->GetString("overlay", "");
	g_szSound = hKv->GetString("sound", "");
	g_bAutoMove = hKv->GetBool("auto_move");
	g_bDB = hKv->GetBool("db_use", false);
	g_iServerID = hKv->GetInt("server_id", 0);
	const char* szCommands = hKv->GetString("commands");
	std::vector<std::string> vecCommands = split(std::string(szCommands), "|");
	g_pUtils->RegCommand(g_PLID, {}, vecCommands, [](int iSlot, const char* szContent) {
		if(g_pAdmin->HasPermission(iSlot, g_szAdminMenuFlag))
		{
			ShowMainMenu(iSlot, true);
		}
		return true;
	});
	g_szContactCommand = hKv->GetString("contact");
	g_pUtils->RegCommand(g_PLID, {}, {g_szContactCommand}, [](int iSlot, const char* szContent) {
		if(g_iTarget[iSlot] != -1 && g_szSocial[iSlot][0] && !g_szContact[iSlot][0] && !g_bAdmin[iSlot])
		{
			// CCommand arg;
			// arg.Tokenize(szContent);
			std::vector<std::string> vecArgs = SplitStringBySpace(szContent);
			// if(arg.ArgC() < 3)
			if(vecArgs.size() < 2)
			{
				g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("Usage_Contact"), vecArgs[0].c_str(), g_mSocials[g_szSocial[iSlot]].sExample.c_str());
				return true;
			}
			std::string sContact = szContent;
			sContact.erase(0, vecArgs[0].length() + 1);
			if(sContact.size())
				sContact.pop_back();
			
			if(sContact[0] == ' ') while(sContact[0] == ' ') sContact.erase(0, 1);
			if(sContact.size() < g_mSocials[g_szSocial[iSlot]].iMin || sContact.size() > g_mSocials[g_szSocial[iSlot]].iMax)
			{
				g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_Contact_Size"), g_mSocials[g_szSocial[iSlot]].iMin, g_mSocials[g_szSocial[iSlot]].iMax);
				return true;
			}
			SetContact(iSlot, strdup(sContact.c_str()));
			g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_SetContact_User"), sContact.c_str());
			g_pUtils->PrintToChat(g_iTarget[iSlot], g_pAdmin->GetTranslation("CC_SetContact_Admin"), sContact.c_str());
		}
		return true;
	});

	if(g_bAdminMenuType) g_pAdmin->RegisterCategory(g_szAdminMenuCategory, "Category_CheckCheats", nullptr);
	else g_pAdmin->RegisterCategory("players", "Category_Players", nullptr);
	g_pAdmin->RegisterItem("checkcheats", "Item_CheckCheats", g_szAdminMenuCategory, g_szAdminMenuFlag, nullptr, [](int iSlot, const char* szCategory, const char* szIdentity, const char* szItem) {
		ShowMainMenu(iSlot);
	});

}

void OnPlayerDisconnect(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	int iSlot = pEvent->GetInt("userid");
	int iReason = pEvent->GetInt("reason");
	int iTarget = g_iTarget[iSlot];
	if(iTarget == -1) return;
	bool bAdmin = g_bAdmin[iSlot];
	if(bAdmin)
	{
		if(g_hOverlays[iTarget])
			g_pUtils->RemoveEntity(g_hOverlays[iTarget]);
		g_hOverlays[iTarget] = nullptr;
	} else {
		if(g_hOverlays[iSlot])
			g_pUtils->RemoveEntity(g_hOverlays[iSlot]);
		g_hOverlays[iSlot] = nullptr;
	}

	if(iReason == 0 || iReason == 1 || iReason == 41 || iReason == 54)
	{
		ResetUserData(iSlot);
		ResetUserData(iTarget);
		return;
	}

	const char* sName = g_pPlayers->GetPlayerName(bAdmin?iTarget:iSlot);
	uint64 iSteamID = g_pPlayers->GetSteamID64(bAdmin?iTarget:iSlot);
	int iStage = g_iStage[iSlot];
	const char* szContact = strdup(g_szContact[iSlot]);
	ResetUserData(iSlot);
	ResetUserData(iTarget);

	if(g_Leave.bAutoBan && (g_Leave.iMin <= iStage || g_Leave.iMin == -1) && (g_Leave.iMax > iStage || g_Leave.iMax == -1))
	{
		if(bAdmin)
			g_pAdmin->AddPlayerPunishment(iTarget, RT_BAN, g_mReasons[g_Leave.iBanReason].iTime, g_mReasons[g_Leave.iBanReason].sReason2.c_str(), iSlot);
		else
			g_pAdmin->AddPlayerPunishment(iSlot, RT_BAN, g_mReasons[g_Leave.iBanReason].iTime, g_mReasons[g_Leave.iBanReason].sReason2.c_str(), iTarget);

		if(g_bDB) {
			char szBuffer[1024];
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "INSERT INTO `checkcheats_stats` (\
				`server_id`, `player_steamid`, `player_name`, `admin_steamid`, `admin_name`, `datestart`, `date_end`, `verdict`, `suspect_discord`) VALUES (\
				'%i', '%lld', '%s', '%lld', '%s', '%i', '%i', '%s', '%s')",
				g_iServerID,
				iSteamID, 
				g_pAdmin->GetMySQLConnection()->Escape(sName).c_str(),
				g_pPlayers->GetSteamID64(bAdmin?iSlot:iTarget),
				g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(bAdmin?iSlot:iTarget)).c_str(),
				g_iStart[iSlot],
				std::time(0),
				g_mReasons[g_Leave.iBanReason].sReason.c_str(),
				g_pAdmin->GetMySQLConnection()->Escape(szContact).c_str());

			g_pAdmin->GetMySQLConnection()->Query(szBuffer, [](ISQLQuery*){});
		}
		
		char szBuffer[128];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i %s", bAdmin?iTarget:iSlot, g_mReasons[g_Leave.iBanReason].iTime, g_mReasons[g_Leave.iBanReason].sReason2.c_str());
		g_pAdmin->SendAction(bAdmin?iSlot:iTarget, "checkcheats", szBuffer);
	}
	else if(g_Leave.bLeaveMenu && !bAdmin)
	{
		Menu hMenu;
		g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("CC_Title_Leave"));
		for(auto& it : g_mReasons)
		{
			if(it.second.bShow)
			{
				g_pMenus->AddItemMenu(hMenu, std::to_string(it.first).c_str(), it.second.sReason.c_str(), (it.second.iMin == -1 || it.second.iMin <= iStage) && (it.second.iMax == -1 || iStage < it.second.iMax)?ITEM_DEFAULT:ITEM_DISABLED);
			}
		}
		g_pMenus->SetBackMenu(hMenu, false);
		g_pMenus->SetExitMenu(hMenu, true);
		g_pMenus->SetCallback(hMenu, [iSteamID, szName = strdup(sName), szContact](const char* szBack, const char* szFront, int iItem, int iSlot) {
			if(iItem < 7)
			{
				int iReason = std::atoi(szBack);
				if(g_mReasons.find(iReason) != g_mReasons.end())
				{
					Reasons sReason = g_mReasons[iReason];
					char szSteamID[64];
					g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", iSteamID);
					char szBuffer[128];
					g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s %s %i %s", szSteamID, szName, sReason.iTime, sReason.sReason.c_str());
					g_pAdmin->SendAction(iSlot, "checkcheats_offline", szBuffer);
					
					if(g_bDB) {
						char szBuffer[1024];
						g_SMAPI->Format(szBuffer, sizeof(szBuffer), "INSERT INTO `checkcheats_stats` (\
							`server_id`, `player_steamid`, `player_name`, `admin_steamid`, `admin_name`, `datestart`, `date_end`, `verdict`, `suspect_discord`) VALUES (\
							'%i', '%lld', '%s', '%lld', '%s', '%i', '%i', '%s', '%s')",
							g_iServerID,
							iSteamID, 
							g_pAdmin->GetMySQLConnection()->Escape(szName).c_str(),
							g_pPlayers->GetSteamID64(iSlot),
							g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(iSlot)).c_str(),
							g_iStart[iSlot],
							std::time(0),
							sReason.sReason.c_str(),
							g_pAdmin->GetMySQLConnection()->Escape(szContact).c_str());

						g_pAdmin->GetMySQLConnection()->Query(szBuffer, [](ISQLQuery*){});
					}

					if(sReason.iTime != -1) g_pAdmin->AddOfflinePlayerPunishment(szSteamID, szName, RT_BAN, sReason.iTime, sReason.sReason2.c_str(), iSlot);
					g_pMenus->ClosePlayerMenu(iSlot);
				}
			}
		});
		g_pMenus->DisplayPlayerMenu(hMenu, bAdmin?iSlot:iTarget);
	}
}

void OnAdminCoreLoaded()
{
	if(g_bDB) {
		g_pAdmin->GetMySQLConnection()->Query("CREATE TABLE IF NOT EXISTS `checkcheats_stats` (\
			`id` INT PRIMARY KEY AUTO_INCREMENT,\
			`server_id` INT DEFAULT NULL,\
			`player_steamid` VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,\
			`player_name` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,\
			`admin_steamid` VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,\
			`admin_name` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,\
			`datestart` INT UNSIGNED NULL DEFAULT NULL,\
			`date_end` INT UNSIGNED NULL DEFAULT NULL,\
			`verdict` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,\
			`suspect_discord` VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL\
			) ENGINE = InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;", [](ISQLQuery*){});
	}
}

void CheckCheats::AllPluginsLoaded()
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
	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(Players_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Players system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pAdmin = (IAdminApi *)g_SMAPI->MetaFactory(Admin_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Admin system plugin", GetLogTag());

		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pUtils->HookEvent(g_PLID, "player_disconnect", OnPlayerDisconnect);
	g_pUtils->RegCommand(g_PLID, { "jointeam" }, {}, [](int iSlot, const char* szContent) {
		if(g_iTarget[iSlot] != -1 && (g_bBlockTeamChange[g_iTarget[iSlot]] || g_bAutoMove && g_bAdmin[g_iTarget[iSlot]]))
		{
			g_pUtils->PrintToChat(iSlot, g_pAdmin->GetTranslation("CC_BlockChangeTeam"));
			return true;
		}
		return false;
	});
	g_pAdmin->OnCoreLoaded(g_PLID, OnAdminCoreLoaded);
	g_pUtils->StartupServer(g_PLID, StartupServer);
	LoadConfig();
	g_pUtils->CreateTimer(1.0f, [](){
		char szBuffer[64];
		for (int i = 0; i < 64; i++)
		{
			if(g_iTimer[i] > 0)
			{
				g_iTimer[i]--;
				if(g_iTimer[i] == 0)
				{
					g_pUtils->PrintToChat(i, g_pAdmin->GetTranslation("CC_Timer_Expired"));
					g_pUtils->PrintToChat(g_iTarget[i], g_pAdmin->GetTranslation("CC_Timer_Expired"));
					g_iTimer[g_iTarget[i]] = 0;
					if(g_Timer.bAutoBan)
					{
						if(g_bAdmin[i]) {
							if(g_bDB) {
								char szBuffer[1024];
								g_SMAPI->Format(szBuffer, sizeof(szBuffer), "INSERT INTO `checkcheats_stats` (\
									`server_id`, `player_steamid`, `player_name`, `admin_steamid`, `admin_name`, `datestart`, `date_end`, `verdict`, `suspect_discord`) VALUES (\
									'%i', '%lld', '%s', '%lld', '%s', '%i', '%i', '%s', '%s')",
									g_iServerID,
									g_pPlayers->GetSteamID64(g_iTarget[i]),
									g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(g_iTarget[i])).c_str(),
									g_pPlayers->GetSteamID64(i),
									g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(i)).c_str(),
									g_iStart[i],
									std::time(0),
									g_mReasons[g_Timer.iBanReason].sReason.c_str(),
									g_pAdmin->GetMySQLConnection()->Escape(g_szContact[i]).c_str());
								g_pAdmin->GetMySQLConnection()->Query(szBuffer, [](ISQLQuery*){});
							}
							g_pAdmin->AddPlayerPunishment(g_iTarget[i], RT_BAN, g_mReasons[g_Timer.iBanReason].iTime, g_mReasons[g_Timer.iBanReason].sReason2.c_str(), i);
						}
						else {
							if(g_bDB) {
								char szBuffer[1024];
								g_SMAPI->Format(szBuffer, sizeof(szBuffer), "INSERT INTO `checkcheats_stats` (\
									`server_id`, `player_steamid`, `player_name`, `admin_steamid`, `admin_name`, `datestart`, `date_end`, `verdict`, `suspect_discord`) VALUES (\
									'%i', '%lld', '%s', '%lld', '%s', '%i', '%i', '%s', '%s')",
									g_iServerID,
									g_pPlayers->GetSteamID64(i),
									g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(i)).c_str(),
									g_pPlayers->GetSteamID64(g_iTarget[i]),
									g_pAdmin->GetMySQLConnection()->Escape(g_pPlayers->GetPlayerName(g_iTarget[i])).c_str(),
									g_iStart[i],
									std::time(0),
									g_mReasons[g_Timer.iBanReason].sReason.c_str(),
									g_pAdmin->GetMySQLConnection()->Escape(g_szContact[i]).c_str());
								g_pAdmin->GetMySQLConnection()->Query(szBuffer, [](ISQLQuery*){});
							}

							g_pAdmin->AddPlayerPunishment(i, RT_BAN, g_mReasons[g_Timer.iBanReason].iTime, g_mReasons[g_Timer.iBanReason].sReason2.c_str(), g_iTarget[i]);
						}
						
						char szBuffer[128];
						g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i %s", g_bAdmin[i]?g_iTarget[i]:i, g_mReasons[g_Timer.iBanReason].iTime, g_mReasons[g_Timer.iBanReason].sReason2.c_str());
						g_pAdmin->SendAction(g_bAdmin[i]?i:g_iTarget[i], "checkcheats", szBuffer);
					}
				}
				else
				{
					if(g_Timer.iMin <= g_iStage[i] && g_iStage[i] < g_Timer.iMax)
					{
						g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdmin->GetTranslation("CC_Timer_Seconds"), g_iTimer[i]);
						g_pUtils->PrintToCenter(i, szBuffer);
					}
				}
			}			
		}
		return 1.0f;
	});
}

///////////////////////////////////////
const char* CheckCheats::GetLicense()
{
	return "GPL";
}

const char* CheckCheats::GetVersion()
{
	return "1.0.6.2";
}

const char* CheckCheats::GetDate()
{
	return __DATE__;
}

const char *CheckCheats::GetLogTag()
{
	return "CheckCheats";
}

const char* CheckCheats::GetAuthor()
{
	return "Pisex";
}

const char* CheckCheats::GetDescription()
{
	return "AS CheckCheats";
}

const char* CheckCheats::GetName()
{
	return "[AS] CheckCheats";
}

const char* CheckCheats::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
