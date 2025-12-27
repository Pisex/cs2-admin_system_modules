#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#include "as_warnsystem.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "filesystem.h"

warnsystem g_warnsystem;
PLUGIN_EXPOSE(warnsystem, g_warnsystem);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;
IMenusApi* g_pMenus = nullptr;
IPlayersApi* g_pPlayers = nullptr;
IAdminApi* g_pAdmin = nullptr;

enum NotifyChannel
{
	NOTIFY_CHAT = 1 << 0,
	NOTIFY_CENTER = 1 << 1
};
struct WarnConfig
{
	int iMaxWarnings = 3;
	int iBanDuration = 1440;
	std::string sBanReason = "Reached max warnings";
	int iExpireSeconds = 604800;
	std::vector<std::string> vReasons = {"Warning"};
	std::vector<int> vReasonExpire = {iExpireSeconds};
	bool bUseDB = true;
	bool bAllowDuplicateReason = true;
	int iServerID = 0;
	std::string sCommand = "!warn";
	std::string sConsoleCommand = "warn";
	std::string sPlayerCommand = "!warns";
	std::string sPlayerConsoleCommand;
	std::string sWarnSound;
	int iNotifyChannels = NOTIFY_CHAT;
	std::string sFlag = "@admin/warn";
	bool bCustomCategory = false;
	std::string sCategory = "players";
};

struct WarnEntry
{
	int iID = 0;
	std::string sReason;
	time_t tCreated = 0;
	time_t tExpire = 0;
	uint64 iAdminSteam = 0;
	std::string sAdminName;
};

struct RecentDisconnect
{
	uint64 iSteam = 0;
	std::string sName;
	time_t tDisconnected = 0;
};

WarnConfig g_Config;
std::map<std::string, std::vector<WarnEntry>> g_WarnStorage;
static std::deque<RecentDisconnect> g_RecentDisconnects;
static size_t g_MaxRecentDisconnects = 10;

static std::string SteamToString(uint64 id);
static bool HasActiveReason(uint64 steam, const std::string& reason);

template<typename... Args>
static void ChatMsg(int iSlot, const char* szKey, Args... args)
{
	const char* szPrefix = g_pAdmin->GetTranslation("Chat_Prefix");
	const char* szBody = g_pAdmin->GetTranslation(szKey);
	char szFmt[256];
	g_SMAPI->Format(szFmt, sizeof(szFmt), " %s%s", szPrefix, szBody);
	g_pUtils->PrintToChat(iSlot, szFmt, args...);
}

template<typename... Args>
static void ChatMsgAll(const char* szKey, Args... args)
{
	const char* szPrefix = g_pAdmin->GetTranslation("Chat_Prefix");
	const char* szBody = g_pAdmin->GetTranslation(szKey);
	char szFmt[256];
	g_SMAPI->Format(szFmt, sizeof(szFmt), " %s%s", szPrefix, szBody);
	g_pUtils->PrintToChatAll(szFmt, args...);
}

template<typename... Args>
static void NotifyMsg(int iSlot, const char* szKey, Args... args)
{
	const char* szBody = g_pAdmin->GetTranslation(szKey);
	if(g_Config.iNotifyChannels & NOTIFY_CHAT)
	{
		const char* szPrefix = g_pAdmin->GetTranslation("Chat_Prefix");
		char szFmt[256];
		g_SMAPI->Format(szFmt, sizeof(szFmt), " %s%s", szPrefix, szBody);
		g_pUtils->PrintToChat(iSlot, szFmt, args...);
	}
	if(g_Config.iNotifyChannels & NOTIFY_CENTER)
	{
		g_pUtils->PrintToCenter(iSlot, szBody, args...);
	}
}

template<typename... Args>
static void NotifyMsgAll(const char* szKey, Args... args)
{
	const char* szBody = g_pAdmin->GetTranslation(szKey);
	if(g_Config.iNotifyChannels & NOTIFY_CHAT)
	{
		const char* szPrefix = g_pAdmin->GetTranslation("Chat_Prefix");
		char szFmt[256];
		g_SMAPI->Format(szFmt, sizeof(szFmt), " %s%s", szPrefix, szBody);
		g_pUtils->PrintToChatAll(szFmt, args...);
	}
	if(g_Config.iNotifyChannels & NOTIFY_CENTER)
	{
		g_pUtils->PrintToCenterAll(szBody, args...);
	}
}

std::vector<std::string> Split(const std::string& input, char delim)
{
	std::vector<std::string> out;
	std::string token;
	for(char c : input)
	{
		if(c == delim)
		{
			if(!token.empty()) out.push_back(token);
			token.clear();
		}
		else token.push_back(c);
	}
	if(!token.empty()) out.push_back(token);
	return out;
}

static std::string ToLower(const std::string& s)
{
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return out;
}

static bool CanAdminPunish(int iAdminSlot, int iTargetSlot)
{
	if(iAdminSlot < 0 || iTargetSlot < 0) return true;
	const int immunityType = g_pAdmin->GetImmunityType();
	if(immunityType == 0) return true;
	if(!g_pAdmin->IsAdmin(iTargetSlot)) return true;
	const int adminImm = g_pAdmin->GetAdminImmunity(iAdminSlot);
	const int targetImm = g_pAdmin->GetAdminImmunity(iTargetSlot);

	switch(immunityType)
	{
		case 1: 
			return adminImm >= targetImm;
		case 2:
			return adminImm > targetImm;
		case 3:
			return false;
		default:
			return true;
	}
}

static bool CheckImmunityOrNotify(int iAdminSlot, int iTargetSlot)
{
	if(CanAdminPunish(iAdminSlot, iTargetSlot)) return true;
	ChatMsg(iAdminSlot, "Chat_ImmunityBlocked");
	return false;
}

static std::string FormatDurationShort(int seconds)
{
	if(seconds <= 0) return g_pAdmin->GetTranslation("Menu_WarnExpireNever");

	int s = seconds;
	int days = s / 86400; s %= 86400;
	int hours = s / 3600; s %= 3600;
	int mins = s / 60; s %= 60;
	int secs = s;

	bool useSecs = (days == 0 && hours == 0 && mins == 0);

	std::vector<std::string> parts;
	if(days)
	{
		char buf[32];
		g_SMAPI->Format(buf, sizeof(buf), "%i %s", days, g_pAdmin->GetTranslation("Unit_Day"));
		parts.push_back(buf);
	}
	if(hours)
	{
		char buf[32];
		g_SMAPI->Format(buf, sizeof(buf), "%i %s", hours, g_pAdmin->GetTranslation("Unit_Hour"));
		parts.push_back(buf);
	}
	if(mins)
	{
		char buf[32];
		g_SMAPI->Format(buf, sizeof(buf), "%i %s", mins, g_pAdmin->GetTranslation("Unit_Min"));
		parts.push_back(buf);
	}
	if(useSecs)
	{
		char buf[32];
		g_SMAPI->Format(buf, sizeof(buf), "%i %s", secs, g_pAdmin->GetTranslation("Unit_Sec"));
		parts.push_back(buf);
	}

	if(parts.size() > 2) parts.resize(2);

	std::string out;
	for(size_t i = 0; i < parts.size(); ++i)
	{
		if(i) out += ' ';
		out += parts[i];
	}
	return out;
}

static std::string FormatExpireDuration(int seconds)
{
	if(seconds <= 0) return g_pAdmin->GetTranslation("Menu_WarnExpireNever");
	return FormatDurationShort(seconds);
}

static bool GetExistingReasonInfo(uint64 steam, const std::string& reason, std::string& outExpire)
{
	const auto it = g_WarnStorage.find(SteamToString(steam));
	if(it == g_WarnStorage.end()) return false;
	const time_t now = std::time(nullptr);
	for(auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit)
	{
		const WarnEntry& w = *rit;
		if(w.sReason != reason) continue;
		if(w.tExpire != 0 && w.tExpire <= now) continue;
		int remaining = w.tExpire == 0 ? -1 : static_cast<int>(w.tExpire - now);
		if(remaining < 0 && w.tExpire != 0) remaining = 0;
		outExpire = (w.tExpire == 0) ? g_pAdmin->GetTranslation("Menu_WarnExpireNever") : FormatDurationShort(remaining);
		return true;
	}
	return false;
}

static bool HasActiveReason(uint64 steam, const std::string& reason)
{
	std::string dummy;
	return GetExistingReasonInfo(steam, reason, dummy);
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

static std::string SteamToString(uint64 id)
{
	return std::to_string(id);
}

static int GetWarningCount(uint64 steamid)
{
	const auto it = g_WarnStorage.find(SteamToString(steamid));
	if(it == g_WarnStorage.end()) return 0;
	return static_cast<int>(it->second.size());
}

static void SaveWarningToDB(const std::string& steam, const WarnEntry& entry, const char* szPlayerName)
{
	if(!g_Config.bUseDB || !g_pAdmin) return;

	std::string sEscapedReason = g_pAdmin->GetMySQLConnection()->Escape(entry.sReason.c_str());
	std::string sEscapedPlayer = g_pAdmin->GetMySQLConnection()->Escape(szPlayerName ? szPlayerName : "");
	std::string sEscapedAdmin = g_pAdmin->GetMySQLConnection()->Escape(entry.sAdminName.c_str());

	char szQuery[1024];
	g_SMAPI->Format(szQuery, sizeof(szQuery), "INSERT INTO `as_warnsystem` (\
		`server_id`,`player_steamid`,`player_name`,`admin_steamid`,`admin_name`,`reason`,`created_at`,`expires_at`,`removed`,`removed_at`) VALUES\
		('%i','%s','%s','%llu','%s','%s','%i','%i',0,0);",
		g_Config.iServerID,
		steam.c_str(),
		sEscapedPlayer.c_str(),
		entry.iAdminSteam,
		sEscapedAdmin.c_str(),
		sEscapedReason.c_str(),
		static_cast<int>(entry.tCreated),
		static_cast<int>(entry.tExpire));

	std::string sSteamCopy = steam;
	size_t idx = g_WarnStorage[steam].size() - 1;
	g_pAdmin->GetMySQLConnection()->Query(szQuery, [sSteamCopy, idx](ISQLQuery* pQuery){
		if(!pQuery) return;
		auto it = g_WarnStorage.find(sSteamCopy);
		if(it == g_WarnStorage.end()) return;
		if(idx >= it->second.size()) return;
		it->second[idx].iID = static_cast<int>(pQuery->GetInsertId());
	});
}

static void MarkWarningRemovedInDB(const std::string& steam, int warnId, uint64 adminSteam, const char* szAdminName)
{
	if(!g_Config.bUseDB || !g_pAdmin) return;
	const char* szName = szAdminName ? szAdminName : "";
	std::string sEscapedName = g_pAdmin->GetMySQLConnection()->Escape(szName);
	char szQuery[512];
	if(warnId > 0)
	{
		g_SMAPI->Format(szQuery, sizeof(szQuery), "UPDATE `as_warnsystem` SET `removed`=1,`removed_at`=UNIX_TIMESTAMP(),`removed_by_steamid`='%llu',`removed_by_name`='%s' WHERE `id`=%i;", adminSteam, sEscapedName.c_str(), warnId);
	}
	else
	{
		g_SMAPI->Format(szQuery, sizeof(szQuery), "UPDATE `as_warnsystem` SET `removed`=1,`removed_at`=UNIX_TIMESTAMP(),`removed_by_steamid`='%llu',`removed_by_name`='%s' WHERE `player_steamid`='%s' AND `removed`=0 ORDER BY `id` DESC LIMIT 1;", adminSteam, sEscapedName.c_str(), steam.c_str());
	}
	g_pAdmin->GetMySQLConnection()->Query(szQuery, [](ISQLQuery*){});
}

static void PruneExpiredWarnings()
{
	if(g_Config.iExpireSeconds <= 0) return;
	const time_t now = std::time(nullptr);
	for(auto& pair : g_WarnStorage)
	{
		auto& vec = pair.second;
		vec.erase(std::remove_if(vec.begin(), vec.end(), [now](const WarnEntry& w){
			return w.tExpire > 0 && w.tExpire <= now;
		}), vec.end());
	}
}

static int LoadCorePunishServerID()
{
	KeyValues kv("admin_system");
	const char* pszPath = "addons/configs/admin_system/core.ini";
	if(!kv.LoadFromFile(g_pFullFileSystem, pszPath))
	{
		return -1;
	}
	return kv.GetInt("punish_server_id", -1);
}

static void LoadConfig()
{
	KeyValues* pKv = new KeyValues("warnsystem");
	const char* pszPath = "addons/configs/admin_system/warnsystem.ini";

	if(!pKv->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtils->ErrorLog("[%s] Failed to load %s, using defaults", g_warnsystem.GetLogTag(), pszPath);
		return;
	}

	const int iCoreServerID = LoadCorePunishServerID();

	g_Config.iMaxWarnings = pKv->GetInt("max_warnings", g_Config.iMaxWarnings);
	g_Config.iBanDuration = pKv->GetInt("ban_duration", g_Config.iBanDuration);
	g_Config.sBanReason = pKv->GetString("ban_reason", g_Config.sBanReason.c_str());
	g_Config.iExpireSeconds = pKv->GetInt("expire_seconds", g_Config.iExpireSeconds);
	g_Config.sCommand = pKv->GetString("command", g_Config.sCommand.c_str());
	g_Config.sConsoleCommand = pKv->GetString("console_command", g_Config.sConsoleCommand.c_str());
	g_Config.sPlayerCommand = pKv->GetString("player_command", g_Config.sPlayerCommand.c_str());
	g_Config.sPlayerConsoleCommand = pKv->GetString("player_console_command", g_Config.sPlayerConsoleCommand.c_str());
	g_Config.sWarnSound = pKv->GetString("warn_sound", g_Config.sWarnSound.c_str());
	g_Config.sFlag = pKv->GetString("flag", g_Config.sFlag.c_str());
	g_Config.bCustomCategory = pKv->GetBool("custom_category", g_Config.bCustomCategory);
	g_Config.sCategory = pKv->GetString("category", g_Config.sCategory.c_str());

	int iRecentLimit = pKv->GetInt("offline_recent_limit", static_cast<int>(g_MaxRecentDisconnects));
	if(iRecentLimit < 1) iRecentLimit = 1;
	if(iRecentLimit > 50) iRecentLimit = 50;
	g_MaxRecentDisconnects = static_cast<size_t>(iRecentLimit);

	const char* pszNotify = pKv->GetString("notify_channels", "chat");
	std::vector<std::string> vNotify = Split(pszNotify, '|');
	int iNotify = 0;
	for(const auto& token : vNotify)
	{
		std::string sTok = ToLower(token);
		if(sTok == "chat") iNotify |= NOTIFY_CHAT;
		else if(sTok == "center") iNotify |= NOTIFY_CENTER;
	}
	if(iNotify == 0) iNotify = NOTIFY_CHAT;
	g_Config.iNotifyChannels = iNotify;

	g_Config.bAllowDuplicateReason = pKv->GetBool("allow_duplicate_reason", g_Config.bAllowDuplicateReason);

	if(!g_Config.bCustomCategory)
	{
		if(g_Config.sCategory != "players" && g_Config.sCategory != "punishments")
		{
			g_Config.bCustomCategory = true;
		}
	}

	int iCfgServer = pKv->GetInt("server_id", -1);
	if(iCfgServer == -1)
		g_Config.iServerID = iCoreServerID;
	else
		g_Config.iServerID = iCfgServer;

	KeyValues* pReasons = pKv->FindKey("reasons");
	if(pReasons)
	{
		g_Config.vReasons.clear();
		g_Config.vReasonExpire.clear();
		FOR_EACH_TRUE_SUBKEY(pReasons, pSub)
		{
			const char* pszName = pSub->GetString("text", pSub->GetName());
			int iExpire = pSub->GetInt("expire_seconds", g_Config.iExpireSeconds);
			if(pszName && pszName[0])
			{
				g_Config.vReasons.push_back(pszName);
				g_Config.vReasonExpire.push_back(iExpire);
			}
		}
	}

	if(g_Config.vReasons.empty())
	{
		g_Config.vReasons.push_back("Warning");
		g_Config.vReasonExpire.push_back(g_Config.iExpireSeconds);
	}
}

static WarnEntry BuildWarnEntry(int iAdmin, const std::string& sReason, int iReasonExpire)
{
	const time_t now = std::time(nullptr);
	const int iExpireSeconds = iReasonExpire > 0 ? iReasonExpire : g_Config.iExpireSeconds;
	const time_t expire = iExpireSeconds > 0 ? now + iExpireSeconds : 0;

	WarnEntry entry;
	entry.sReason = sReason;
	entry.tCreated = now;
	entry.tExpire = expire;
	entry.iAdminSteam = g_pPlayers->GetSteamID64(iAdmin);
	entry.sAdminName = g_pPlayers->GetPlayerName(iAdmin);
	return entry;
}

static void AddWarningInternal(const std::string& sSteam, const char* szPlayerName, const WarnEntry& entry, int iAdmin, int iTargetSlot)
{
	auto& vec = g_WarnStorage[sSteam];
	vec.push_back(entry);

	const char* szTargetName = szPlayerName ? szPlayerName : "";

	if(iTargetSlot >= 0)
	{
		NotifyMsg(iTargetSlot, "Chat_WarnReceived", static_cast<int>(vec.size()), g_Config.iMaxWarnings, entry.sReason.c_str());
		NotifyMsg(iAdmin, "Chat_WarnIssued", szTargetName, static_cast<int>(vec.size()), g_Config.iMaxWarnings);

		if(!g_Config.sWarnSound.empty())
		{
			engine->ClientCommand(iTargetSlot, "play %s", g_Config.sWarnSound.c_str());
		}
	}
	else
	{
		ChatMsg(iAdmin, "Chat_WarnIssuedOffline", szTargetName, static_cast<int>(vec.size()), g_Config.iMaxWarnings);
	}

	SaveWarningToDB(sSteam, entry, szTargetName);

	if(g_Config.iMaxWarnings > 0 && static_cast<int>(vec.size()) >= g_Config.iMaxWarnings)
	{
		if(iTargetSlot >= 0)
		{
			g_pAdmin->AddPlayerPunishment(iTargetSlot, RT_BAN, g_Config.iBanDuration, g_Config.sBanReason.c_str(), iAdmin);
		}
		else
		{
			g_pAdmin->AddOfflinePlayerPunishment(sSteam.c_str(), szTargetName, RT_BAN, g_Config.iBanDuration, g_Config.sBanReason.c_str(), iAdmin);
		}
		NotifyMsgAll("Chat_WarnBan", szTargetName);
	}
}

static void AddWarning(int iAdmin, int iTarget, const std::string& sReason, int iReasonExpire)
{
	if(!CheckImmunityOrNotify(iAdmin, iTarget)) return;
	const uint64 steamTarget = g_pPlayers->GetSteamID64(iTarget);
	if(!g_Config.bAllowDuplicateReason && HasActiveReason(steamTarget, sReason))
	{
		ChatMsg(iAdmin, "Chat_WarnDuplicateBlocked");
		return;
	}
	std::string sSteam = SteamToString(steamTarget);
	WarnEntry entry = BuildWarnEntry(iAdmin, sReason, iReasonExpire);
	AddWarningInternal(sSteam, g_pPlayers->GetPlayerName(iTarget), entry, iAdmin, iTarget);
}

static void AddOfflineWarning(int iAdmin, uint64 steamTarget, const char* szPlayerName, const std::string& sReason, int iReasonExpire)
{
	if(!g_Config.bAllowDuplicateReason && HasActiveReason(steamTarget, sReason))
	{
		ChatMsg(iAdmin, "Chat_WarnDuplicateBlocked");
		return;
	}
	std::string sSteam = SteamToString(steamTarget);
	WarnEntry entry = BuildWarnEntry(iAdmin, sReason, iReasonExpire);
	AddWarningInternal(sSteam, szPlayerName, entry, iAdmin, -1);
}

static void RemoveWarningByIndex(int iAdmin, int iTarget, int idx)
{
	const uint64 steamTarget = g_pPlayers->GetSteamID64(iTarget);
	std::string sSteam = SteamToString(steamTarget);
	auto it = g_WarnStorage.find(sSteam);
	if(it == g_WarnStorage.end() || it->second.empty())
	{
		ChatMsg(iAdmin, "Chat_NoWarnings");
		return;
	}
	if(idx < 0 || idx >= static_cast<int>(it->second.size()))
	{
		ChatMsg(iAdmin, "Chat_NoWarnings");
		return;
	}

	int warnId = it->second[idx].iID;
	it->second.erase(it->second.begin() + idx);
	MarkWarningRemovedInDB(sSteam, warnId, g_pPlayers->GetSteamID64(iAdmin), g_pPlayers->GetPlayerName(iAdmin));
	NotifyMsg(iTarget, "Chat_WarnRemovedTarget", static_cast<int>(it->second.size()));
	NotifyMsg(iAdmin, "Chat_WarnRemovedAdmin", g_pPlayers->GetPlayerName(iTarget));
}

static void LoadWarningsFromDB(uint64 steamid)
{
	if(!g_Config.bUseDB || !g_pAdmin) return;
	std::string sSteam = SteamToString(steamid);
	char szQuery[256];
	g_SMAPI->Format(szQuery, sizeof(szQuery), "SELECT `id`,`reason`,`created_at`,`expires_at`,`admin_steamid`,`admin_name` FROM `as_warnsystem` WHERE `player_steamid`='%s' AND `removed`=0 AND (`expires_at`=0 OR `expires_at`>UNIX_TIMESTAMP());", sSteam.c_str());

	g_pAdmin->GetMySQLConnection()->Query(szQuery, [sSteam](ISQLQuery* pQuery){
		if(!pQuery) return;
		ISQLResult* pRes = pQuery->GetResultSet();
		if(!pRes) return;
		std::vector<WarnEntry> vec;
		while(pRes->MoreRows())
		{
			ISQLRow* pRow = pRes->FetchRow();
			if(!pRow) break;
			WarnEntry e;
			unsigned int colId = 0;
			if(pRes->FieldNameToNum("id", &colId)) e.iID = pRes->GetInt(colId);
			if(pRes->FieldNameToNum("reason", &colId)) e.sReason = pRes->GetString(colId);
			if(pRes->FieldNameToNum("created_at", &colId)) e.tCreated = pRes->GetInt(colId);
			if(pRes->FieldNameToNum("expires_at", &colId)) e.tExpire = pRes->GetInt(colId);
			if(pRes->FieldNameToNum("admin_steamid", &colId)) e.iAdminSteam = static_cast<uint64>(pRes->GetInt(colId));
			if(pRes->FieldNameToNum("admin_name", &colId)) e.sAdminName = pRes->GetString(colId);
			vec.push_back(e);
		}
		g_WarnStorage[sSteam] = vec;
	});
}

static void AddRecentDisconnect(uint64 steam, const char* szName)
{
	if(steam == 0) return;
	const char* szSafeName = (szName && szName[0]) ? szName : "Unknown";

	int slot = g_pPlayers->FindPlayer(steam);
	if(slot != -1 && g_pPlayers->IsConnected(slot) && g_pPlayers->IsInGame(slot))
	{
		return;
	}

	g_RecentDisconnects.erase(std::remove_if(g_RecentDisconnects.begin(), g_RecentDisconnects.end(), [steam](const RecentDisconnect& rec){
		return rec.iSteam == steam;
	}), g_RecentDisconnects.end());

	RecentDisconnect rec;
	rec.iSteam = steam;
	rec.sName = szSafeName;
	rec.tDisconnected = std::time(nullptr);
	g_RecentDisconnects.push_front(rec);
	if(g_MaxRecentDisconnects > 0 && g_RecentDisconnects.size() > g_MaxRecentDisconnects)
	{
		g_RecentDisconnects.pop_back();
	}

	LoadWarningsFromDB(steam);
}

static void HandlePlayerDisconnect(IGameEvent* pEvent)
{
	if(!pEvent) return;
	if(pEvent->GetBool("bot", false)) return;

	uint64 steam = pEvent->GetUint64("xuid");
	if(steam == 0) steam = pEvent->GetUint64("steamid");
	if(steam == 0)
	{
		const char* szSteamStr = pEvent->GetString("steamid");
		if((!szSteamStr || !szSteamStr[0])) szSteamStr = pEvent->GetString("networkid");
		if(szSteamStr && szSteamStr[0])
		{
			steam = std::strtoull(szSteamStr, nullptr, 10);
		}
	}
	if(steam == 0) return;

	const char* szName = pEvent->GetString("name");
	std::string sNameCopy = (szName && szName[0]) ? szName : "Unknown";

	g_pUtils->NextFrame([steam, sNameCopy](){
		int slot = g_pPlayers->FindPlayer(steam);
		if(slot != -1 && g_pPlayers->IsConnected(slot) && g_pPlayers->IsInGame(slot))
		{
			return;
		}
		AddRecentDisconnect(steam, sNameCopy.c_str());
	});
}

static int FindRecentIndex(uint64 steam)
{
	for(size_t i = 0; i < g_RecentDisconnects.size(); ++i)
	{
		if(g_RecentDisconnects[i].iSteam == steam) return static_cast<int>(i);
	}
	return -1;
}

static void UpdateRecentDisconnectName(uint64 steam, const char* szName)
{
	if(!szName || !szName[0]) return;
	for(auto& rec : g_RecentDisconnects)
	{
		if(rec.iSteam == steam)
		{
			rec.sName = szName;
			break;
		}
	}
}

static void ShowMyWarningsMenu(int iSlot);
static void ShowRemoveWarningMenu(int iSlot, int iTarget);
static void ShowOfflinePlayerMenu(int iSlot, bool bCommand);
static void ShowOfflineReasonMenu(int iSlot, int iRecentIdx);
static void ShowOfflineReasonConfirmMenu(int iSlot, int iRecentIdx, int idx);

static void ShowMyWarningDetail(int iSlot, int idx)
{
	uint64 steam = g_pPlayers->GetSteamID64(iSlot);
	auto it = g_WarnStorage.find(SteamToString(steam));
	if(it == g_WarnStorage.end() || idx < 0 || idx >= static_cast<int>(it->second.size())) return;
	const WarnEntry& w = it->second[idx];

	Menu hMenu;
	char szTitle[96];
	g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_WarnDetail"), idx + 1);
	g_pMenus->SetTitleMenu(hMenu, szTitle);

	g_pMenus->AddItemMenu(hMenu, "reason", w.sReason.c_str(), ITEM_DISABLED);

	std::string sExpire;
	if(w.tExpire == 0)
	{
		sExpire = g_pAdmin->GetTranslation("Menu_WarnExpireNever");
	}
	else
	{
		const time_t now = std::time(nullptr);
		int remaining = static_cast<int>(w.tExpire - now);
		if(remaining < 0) remaining = 0;
			sExpire = FormatDurationShort(remaining);
	}
	char szExpire[160];
	g_SMAPI->Format(szExpire, sizeof(szExpire), g_pAdmin->GetTranslation("Menu_WarnExpires"), sExpire.c_str());
	g_pMenus->AddItemMenu(hMenu, "exp", szExpire, ITEM_DISABLED);

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [idx](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem == 7)
		{
			ShowMyWarningsMenu(iSlot);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowMyWarningsMenu(int iSlot)
{
	uint64 steam = g_pPlayers->GetSteamID64(iSlot);
	auto it = g_WarnStorage.find(SteamToString(steam));
	if(it == g_WarnStorage.end() || it->second.empty())
	{
		Menu hMenu;
		char szTitle[96];
		g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_MyWarnings"), 0);
		g_pMenus->SetTitleMenu(hMenu, szTitle);
		g_pMenus->AddItemMenu(hMenu, "none", g_pAdmin->GetTranslation("Menu_NoWarningsSelf"), ITEM_DISABLED);
		g_pMenus->SetExitMenu(hMenu, true);
		g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
		return;
	}

	const auto& vec = it->second;
	Menu hMenu;
	char szTitle[96];
	g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_MyWarnings"), static_cast<int>(vec.size()));
	g_pMenus->SetTitleMenu(hMenu, szTitle);

	for(size_t i = 0; i < vec.size(); ++i)
	{
		char szLine[160];
		g_SMAPI->Format(szLine, sizeof(szLine), "%zu) %s", i + 1, vec[i].sReason.c_str());
		g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szLine);
	}

	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			int idx = std::atoi(szBack);
			ShowMyWarningDetail(iSlot, idx);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static std::string BuildWarningLabel(const WarnEntry& w);
static void ShowActionsMenu(int iSlot, int iTarget);
static void ShowReasonConfirmMenu(int iSlot, int iTarget, int idx);

static void ShowRemoveWarningMenu(int iSlot, int iTarget)
{
	const uint64 steamTarget = g_pPlayers->GetSteamID64(iTarget);
	std::string sSteam = SteamToString(steamTarget);
	auto it = g_WarnStorage.find(sSteam);
	Menu hMenu;

	int iCount = it == g_WarnStorage.end() ? 0 : static_cast<int>(it->second.size());
	char szTitle[128];
	g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_RemoveSelect"));
	g_pMenus->SetTitleMenu(hMenu, szTitle);

	if(iCount == 0 || it == g_WarnStorage.end())
	{
		g_pMenus->AddItemMenu(hMenu, "none", g_pAdmin->GetTranslation("Chat_NoWarnings"), ITEM_DISABLED);
	}
	else
	{
		for(size_t i = 0; i < it->second.size(); ++i)
		{
			std::string sLabel = BuildWarningLabel(it->second[i]);
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), sLabel.c_str());
		}
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			int idx = std::atoi(szBack);
			RemoveWarningByIndex(iSlot, iTarget, idx);
			ShowRemoveWarningMenu(iSlot, iTarget);
		}
		else if(iItem == 7)
		{
			ShowActionsMenu(iSlot, iTarget);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}
static std::string BuildWarningLabel(const WarnEntry& w)
{
	std::string sExpire;
	if(w.tExpire == 0)
	{
		sExpire = g_pAdmin->GetTranslation("Menu_WarnExpireNever");
	}
	else
	{
		const time_t now = std::time(nullptr);
		int remaining = static_cast<int>(w.tExpire - now);
		if(remaining < 0) remaining = 0;
		sExpire = FormatDurationShort(remaining);
	}
	char sz[192];
	g_SMAPI->Format(sz, sizeof(sz), "%s (%s)", w.sReason.c_str(), sExpire.c_str());
	return sz;
}

static void ShowReasonMenu(int iSlot, int iTarget)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("Menu_SelectReason"));
	for(size_t i = 0; i < g_Config.vReasons.size(); ++i)
	{
		g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_Config.vReasons[i].c_str());
	}
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			int idx = std::atoi(szBack);
			if(idx >= 0 && idx < static_cast<int>(g_Config.vReasons.size()))
			{
				ShowReasonConfirmMenu(iSlot, iTarget, idx);
			}
		}
		else if(iItem == 7)
		{
			ShowActionsMenu(iSlot, iTarget);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowReasonConfirmMenu(int iSlot, int iTarget, int idx)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("Menu_ConfirmReason"));

	int iExpire = idx < static_cast<int>(g_Config.vReasonExpire.size()) ? g_Config.vReasonExpire[idx] : g_Config.iExpireSeconds;
	std::string sExpire = FormatExpireDuration(iExpire);
	char szExpire[160];
	g_SMAPI->Format(szExpire, sizeof(szExpire), g_pAdmin->GetTranslation("Menu_WarnExpires"), sExpire.c_str());

	g_pMenus->AddItemMenu(hMenu, "exp", szExpire, ITEM_DISABLED);

	bool bDuplicate = false;
	std::string sDupExpire;
	if(GetExistingReasonInfo(g_pPlayers->GetSteamID64(iTarget), g_Config.vReasons[idx], sDupExpire))
	{
		bDuplicate = true;
		char sDup[192];
		g_SMAPI->Format(sDup, sizeof(sDup), g_pAdmin->GetTranslation("Menu_WarnDuplicate"), sDupExpire.c_str());
		g_pMenus->AddItemMenu(hMenu, "dup", sDup, ITEM_DISABLED);
		if(!g_Config.bAllowDuplicateReason)
		{
			g_pMenus->AddItemMenu(hMenu, "block", g_pAdmin->GetTranslation("Menu_WarnDuplicateBlocked"), ITEM_DISABLED);
		}
	}

	if(!bDuplicate || g_Config.bAllowDuplicateReason)
	{
		g_pMenus->AddItemMenu(hMenu, "confirm", g_pAdmin->GetTranslation("Menu_IssueWarning"));
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget, idx, iExpire](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7 && !strcmp(szBack, "confirm"))
		{
			AddWarning(iSlot, iTarget, g_Config.vReasons[idx], iExpire);
			g_pMenus->ClosePlayerMenu(iSlot);
		}
		else if(iItem == 7)
		{
			ShowReasonMenu(iSlot, iTarget);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowOfflineReasonConfirmMenu(int iSlot, int iRecentIdx, int idx)
{
	if(idx < 0 || idx >= static_cast<int>(g_Config.vReasons.size()))
	{
		ShowOfflineReasonMenu(iSlot, iRecentIdx);
		return;
	}
	if(iRecentIdx < 0 || iRecentIdx >= static_cast<int>(g_RecentDisconnects.size()))
	{
		ShowOfflinePlayerMenu(iSlot, false);
		return;
	}

	const auto& info = g_RecentDisconnects[iRecentIdx];

	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("Menu_ConfirmReason"));

	int iExpire = idx < static_cast<int>(g_Config.vReasonExpire.size()) ? g_Config.vReasonExpire[idx] : g_Config.iExpireSeconds;
	std::string sExpire = FormatExpireDuration(iExpire);
	char szExpire[160];
	g_SMAPI->Format(szExpire, sizeof(szExpire), g_pAdmin->GetTranslation("Menu_WarnExpires"), sExpire.c_str());

	g_pMenus->AddItemMenu(hMenu, "exp", szExpire, ITEM_DISABLED);

	bool bDuplicate = false;
	std::string sDupExpire;
	if(GetExistingReasonInfo(info.iSteam, g_Config.vReasons[idx], sDupExpire))
	{
		bDuplicate = true;
		char sDup[192];
		g_SMAPI->Format(sDup, sizeof(sDup), g_pAdmin->GetTranslation("Menu_WarnDuplicate"), sDupExpire.c_str());
		g_pMenus->AddItemMenu(hMenu, "dup", sDup, ITEM_DISABLED);
		if(!g_Config.bAllowDuplicateReason)
		{
			g_pMenus->AddItemMenu(hMenu, "block", g_pAdmin->GetTranslation("Menu_WarnDuplicateBlocked"), ITEM_DISABLED);
		}
	}

	if(!bDuplicate || g_Config.bAllowDuplicateReason)
	{
		g_pMenus->AddItemMenu(hMenu, "confirm", g_pAdmin->GetTranslation("Menu_IssueWarning"));
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iRecentIdx, idx, iExpire](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7 && !strcmp(szBack, "confirm"))
		{
			if(iRecentIdx < 0 || iRecentIdx >= static_cast<int>(g_RecentDisconnects.size())) return;
			const auto& info = g_RecentDisconnects[iRecentIdx];
			AddOfflineWarning(iSlot, info.iSteam, info.sName.c_str(), g_Config.vReasons[idx], iExpire);
			g_pMenus->ClosePlayerMenu(iSlot);
		}
		else if(iItem == 7)
		{
			ShowOfflineReasonMenu(iSlot, iRecentIdx);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowOfflineReasonMenu(int iSlot, int iRecentIdx)
{
	if(iRecentIdx < 0 || iRecentIdx >= static_cast<int>(g_RecentDisconnects.size()))
	{
		ShowOfflinePlayerMenu(iSlot, false);
		return;
	}

	const auto& info = g_RecentDisconnects[iRecentIdx];

	Menu hMenu;
	char szTitle[128];
	g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_WarningsFor"), info.sName.c_str(), GetWarningCount(info.iSteam), g_Config.iMaxWarnings);
	g_pMenus->SetTitleMenu(hMenu, szTitle);

	for(size_t i = 0; i < g_Config.vReasons.size(); ++i)
	{
		g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_Config.vReasons[i].c_str());
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iRecentIdx](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			int idx = std::atoi(szBack);
			if(idx >= 0 && idx < static_cast<int>(g_Config.vReasons.size()))
			{
				ShowOfflineReasonConfirmMenu(iSlot, iRecentIdx, idx);
			}
		}
		else if(iItem == 7)
		{
			ShowOfflinePlayerMenu(iSlot, false);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowPlayerMenu(int iSlot, bool bCommand);

static void ShowOfflinePlayerMenu(int iSlot, bool bCommand)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("Menu_SelectOfflinePlayer"));

	std::vector<size_t> vAvailable;
	for(size_t i = 0; i < g_RecentDisconnects.size(); ++i)
	{
		int slot = g_pPlayers->FindPlayer(g_RecentDisconnects[i].iSteam);
		if(slot != -1 && g_pPlayers->IsConnected(slot) && g_pPlayers->IsInGame(slot))
		{
			continue;
		}
		vAvailable.push_back(i);
	}

	if(vAvailable.empty())
	{
		g_pMenus->AddItemMenu(hMenu, "none", g_pAdmin->GetTranslation("Menu_NoOfflinePlayers"), ITEM_DISABLED);
	}
	else
	{
		for(size_t idx : vAvailable)
		{
			const auto& info = g_RecentDisconnects[idx];
			int iCount = GetWarningCount(info.iSteam);
			char szText[192];
			g_SMAPI->Format(szText, sizeof(szText), "%s (%i)", info.sName.c_str(), iCount);
			g_pMenus->AddItemMenu(hMenu, std::to_string(static_cast<int>(idx)).c_str(), szText);
		}
	}

	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [bCommand](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			int idx = std::atoi(szBack);
			if(idx >= 0 && idx < static_cast<int>(g_RecentDisconnects.size()))
			{
				ShowOfflineReasonMenu(iSlot, idx);
			}
		}
		else if(iItem == 7)
		{
			ShowPlayerMenu(iSlot, bCommand);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowActionsMenu(int iSlot, int iTarget)
{
	Menu hMenu;
	int iCount = GetWarningCount(g_pPlayers->GetSteamID64(iTarget));
	char szTitle[128];
	g_SMAPI->Format(szTitle, sizeof(szTitle), g_pAdmin->GetTranslation("Menu_WarningsFor"), g_pPlayers->GetPlayerName(iTarget), iCount, g_Config.iMaxWarnings);
	g_pMenus->SetTitleMenu(hMenu, szTitle);

	g_pMenus->AddItemMenu(hMenu, "add", g_pAdmin->GetTranslation("Menu_IssueWarning"));
	g_pMenus->AddItemMenu(hMenu, "del", g_pAdmin->GetTranslation("Menu_RemoveWarning"), iCount > 0 ? ITEM_DEFAULT : ITEM_DISABLED);
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			if(!strcmp(szBack, "add"))
			{
				ShowReasonMenu(iSlot, iTarget);
			}
			else if(!strcmp(szBack, "del"))
			{
				ShowRemoveWarningMenu(iSlot, iTarget);
			}
		}
		else if(iItem == 7)
		{
			ShowPlayerMenu(iSlot, false);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void ShowPlayerMenu(int iSlot, bool bCommand)
{
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdmin->GetTranslation("Menu_SelectPlayer"));
	bool bAny = false;
	bool bHasOffline = !g_RecentDisconnects.empty();
	for(int i = 0; i < 64; ++i)
	{
		if(!g_pPlayers->IsConnected(i) || g_pPlayers->IsFakeClient(i) || !g_pPlayers->IsInGame(i)) continue;
		if(i == iSlot) continue;
		bAny = true;
		char szText[128];
		int iCount = GetWarningCount(g_pPlayers->GetSteamID64(i));
		g_SMAPI->Format(szText, sizeof(szText), "%s (%i)", g_pPlayers->GetPlayerName(i), iCount);
		g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szText);
	}

	if(!bAny && !bHasOffline)
	{
		g_pMenus->AddItemMenu(hMenu, "noplayers", g_pAdmin->GetTranslation("Menu_NoPlayers"), ITEM_DISABLED);
	}

	if(bHasOffline)
	{
		g_pMenus->AddItemMenu(hMenu, "offline", g_pAdmin->GetTranslation("Menu_OfflineWarnings"));
	}

	if(!bCommand) g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [bCommand](const char* szBack, const char*, int iItem, int iSlot){
		if(iItem < 7)
		{
			if(!strcmp(szBack, "offline"))
			{
				ShowOfflinePlayerMenu(iSlot, bCommand);
			}
			else
			{
				int iTarget = std::atoi(szBack);
				ShowActionsMenu(iSlot, iTarget);
			}
		}
		else if(iItem == 7 && !bCommand)
		{
			g_pAdmin->ShowAdminLastCategoryMenu(iSlot);
		}
	});

	g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

static void RegisterAdminMenu()
{
	if(g_Config.bCustomCategory)
	{
		g_pAdmin->RegisterCategory(g_Config.sCategory.c_str(), "Warns", nullptr);
	}
	else if(g_Config.sCategory.empty())
	{
		g_Config.sCategory = "players";
	}

	g_pAdmin->RegisterItem("warnsystem", g_pAdmin->GetTranslation("Menu_ItemWarnSystem"), g_Config.sCategory.c_str(), g_Config.sFlag.c_str(), nullptr, [](int iSlot, const char* szCategory, const char* szIdentity, const char* szItem){
		ShowPlayerMenu(iSlot, false);
	});
}

static void AddColumnIfMissing(const char* szColumn, const char* szDefinition, const char* szAfterColumn)
{
	if(!g_Config.bUseDB || !g_pAdmin) return;

	char szCheck[256];
	g_SMAPI->Format(szCheck, sizeof(szCheck), "SELECT 1 FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = 'as_warnsystem' AND column_name = '%s' LIMIT 1;", szColumn);

	std::string sColumn = szColumn ? szColumn : "";
	std::string sDefinition = szDefinition ? szDefinition : "";
	std::string sAfter = szAfterColumn ? szAfterColumn : "";

	g_pAdmin->GetMySQLConnection()->Query(szCheck, [sColumn, sDefinition, sAfter](ISQLQuery* pQuery){
		bool bExists = false;
		if(pQuery)
		{
			if(ISQLResult* pRes = pQuery->GetResultSet())
			{
				bExists = pRes->MoreRows();
			}
		}

		if(bExists || sColumn.empty() || sDefinition.empty()) return;

		char szAlter[512];
		if(!sAfter.empty())
		{
			g_SMAPI->Format(szAlter, sizeof(szAlter), "ALTER TABLE `as_warnsystem` ADD COLUMN `%s` %s AFTER `%s`;", sColumn.c_str(), sDefinition.c_str(), sAfter.c_str());
		}
		else
		{
			g_SMAPI->Format(szAlter, sizeof(szAlter), "ALTER TABLE `as_warnsystem` ADD COLUMN `%s` %s;", sColumn.c_str(), sDefinition.c_str());
		}

		g_pAdmin->GetMySQLConnection()->Query(szAlter, [sColumn](ISQLQuery* pAlter){
			if(!pAlter)
			{
				g_pUtils->ErrorLog("[%s] Failed to add column %s", g_warnsystem.GetLogTag(), sColumn.c_str());
			}
		});
	});
}

static void CreateTables()
{
	if(!g_Config.bUseDB) return;
	g_pAdmin->GetMySQLConnection()->Query("CREATE TABLE IF NOT EXISTS `as_warnsystem` (\
		`id` INT PRIMARY KEY AUTO_INCREMENT,\
		`server_id` INT DEFAULT 0,\
		`player_steamid` VARCHAR(32),\
		`player_name` VARCHAR(64),\
		`admin_steamid` VARCHAR(32),\
		`admin_name` VARCHAR(64),\
		`reason` VARCHAR(128),\
		`created_at` INT UNSIGNED,\
		`expires_at` INT UNSIGNED,\
		`removed` TINYINT(1) DEFAULT 0,\
		`removed_at` INT UNSIGNED DEFAULT 0,\
		`removed_by_steamid` VARCHAR(32) DEFAULT '',\
		`removed_by_name` VARCHAR(64) DEFAULT ''\
		) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;", [](ISQLQuery*){});
	
	AddColumnIfMissing("removed_by_steamid", "VARCHAR(32) DEFAULT ''", "removed_at");
	AddColumnIfMissing("removed_by_name", "VARCHAR(64) DEFAULT ''", "removed_by_steamid");
}

bool warnsystem::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener(this, this);

	return true;
}

bool warnsystem::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();

	return true;
}

void warnsystem::AllPluginsLoaded()
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

	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
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

	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pAdmin->OnCoreLoaded(g_PLID, CreateTables);

	g_pUtils->LoadTranslations("warnsystem");
	g_pUtils->HookEvent(g_PLID, "player_disconnect", [](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
		HandlePlayerDisconnect(pEvent);
	});

	LoadConfig();
	RegisterAdminMenu();

	std::vector<std::string> commands = Split(g_Config.sCommand, '|');
	if(commands.empty()) commands.push_back(g_Config.sCommand);

	std::vector<std::string> consoleCommands;
	if(!g_Config.sConsoleCommand.empty())
	{
		consoleCommands = Split(g_Config.sConsoleCommand, '|');
		if(consoleCommands.empty()) consoleCommands.push_back(g_Config.sConsoleCommand);
	}

	g_pUtils->RegCommand(g_PLID, consoleCommands, commands, [](int iSlot, const char* szContent){
		if(!g_pAdmin->HasPermission(iSlot, g_Config.sFlag.c_str())) return true;

		std::string sInput = szContent ? szContent : "";
		size_t pos = sInput.find_first_not_of(' ');
		if(pos != std::string::npos) sInput.erase(0, pos); else sInput.clear();

		std::vector<std::string> vCmds = Split(g_Config.sCommand, '|');
		{
			std::vector<std::string> vMore = Split(g_Config.sConsoleCommand, '|');
			vCmds.insert(vCmds.end(), vMore.begin(), vMore.end());
		}
		std::vector<std::string> vCmdsNorm;
		for(auto c : vCmds)
		{
			if(!c.empty() && (c[0] == '!' || c[0] == '/')) c.erase(0, 1);
			vCmdsNorm.push_back(ToLower(c));
		}
		auto stripCommandToken = [&](std::vector<std::string>& words){
			while(!words.empty())
			{
				std::string first = words.front();
				if(!first.empty() && (first[0] == '!' || first[0] == '/')) first.erase(0, 1);
				first = ToLower(first);
				bool isCmd = false;
				for(const auto& c : vCmdsNorm)
				{
					if(c == first)
					{
						isCmd = true;
						break;
					}
				}
				if(isCmd)
				{
					words.erase(words.begin());
					continue;
				}
				break;
			}
		};

		std::vector<std::string> args = Split(sInput, ' ');

		size_t startIdx = 0;
		auto normalizeToken = [](std::string s){ if(!s.empty() && (s[0] == '!' || s[0] == '/')) s.erase(0, 1); return s; };
		while(startIdx < args.size())
		{
			std::string candidate = ToLower(normalizeToken(args[startIdx]));
			bool isCmd = false;
			for(const auto& c : vCmdsNorm)
			{
				if(c == candidate)
				{
					isCmd = true;
					break;
				}
			}
			if(isCmd)
			{
				++startIdx;
				continue;
			}
			break;
		}

		if(startIdx >= args.size())
		{
			ShowPlayerMenu(iSlot, true);
			return true;
		}

		std::string sSteamTok = normalizeToken(args[startIdx]);
		uint64 targetSteam = std::strtoull(sSteamTok.c_str(), nullptr, 10);
		if(targetSteam == 0)
		{
			ShowPlayerMenu(iSlot, true);
			return true;
		}

		int targetSlot = g_pPlayers->FindPlayer(targetSteam);
		bool targetOnline = (targetSlot != -1 && g_pPlayers->IsConnected(targetSlot) && g_pPlayers->IsInGame(targetSlot));

		if(targetSteam == g_pPlayers->GetSteamID64(iSlot))
		{
			ChatMsg(iSlot, "Chat_SelfWarnNotAllowed");
			return true;
		}

		if(args.size() >= startIdx + 3)
		{
			const std::string& sReasonArg = args[startIdx + 1];
			int iCustomExpire = std::atoi(args[startIdx + 2].c_str());
			if(iCustomExpire <= 0) iCustomExpire = g_Config.iExpireSeconds;

			if(targetOnline)
			{
				if(!CheckImmunityOrNotify(iSlot, targetSlot)) return true;
				AddWarning(iSlot, targetSlot, sReasonArg, iCustomExpire);
			}
			else
			{
				std::string sName = SteamToString(targetSteam);
				int idx = FindRecentIndex(targetSteam);
				if(idx == -1)
				{
					AddRecentDisconnect(targetSteam, sName.c_str());
					idx = FindRecentIndex(targetSteam);
				}
				if(idx != -1) sName = g_RecentDisconnects[idx].sName;
				AddOfflineWarning(iSlot, targetSteam, sName.c_str(), sReasonArg, iCustomExpire);
			}
			return true;
		}

		// Only steamid provided: open reason menu for that player (online) or offline entry
		if(targetOnline)
		{
			if(!CheckImmunityOrNotify(iSlot, targetSlot)) return true;
			ShowReasonMenu(iSlot, targetSlot);
			return true;
		}

		int idx = FindRecentIndex(targetSteam);
		if(idx == -1)
		{
			std::string sName = SteamToString(targetSteam);
			AddRecentDisconnect(targetSteam, sName.c_str());
			idx = FindRecentIndex(targetSteam);
		}
		if(idx != -1)
		{
			ShowOfflineReasonMenu(iSlot, idx);
		}
		else
		{
			ShowPlayerMenu(iSlot, true);
		}
		return true;
	});

	std::vector<std::string> playerCommands = Split(g_Config.sPlayerCommand, '|');
	if(playerCommands.empty()) playerCommands.push_back(g_Config.sPlayerCommand);

	std::vector<std::string> playerConsoleCommands;
	if(!g_Config.sPlayerConsoleCommand.empty())
	{
		playerConsoleCommands = Split(g_Config.sPlayerConsoleCommand, '|');
		if(playerConsoleCommands.empty()) playerConsoleCommands.push_back(g_Config.sPlayerConsoleCommand);
	}

	g_pUtils->RegCommand(g_PLID, playerConsoleCommands, playerCommands, [](int iSlot, const char* szContent){
		if(iSlot < 0 || !g_pPlayers->IsConnected(iSlot)) return true;
		ShowMyWarningsMenu(iSlot);
		return true;
	});

	g_pPlayers->HookOnClientAuthorized(g_PLID, [](int iSlot, uint64 iSteam){
		LoadWarningsFromDB(iSteam);
		const char* szName = g_pPlayers->GetPlayerName(iSlot);
		UpdateRecentDisconnectName(iSteam, szName);
	});

	g_pUtils->CreateTimer(5.0f, [](){
		PruneExpiredWarnings();
		return 5.0f;
	});
}

const char* warnsystem::GetLicense()
{
	return "GPL";
}

const char* warnsystem::GetVersion()
{
	return "1.1";
}

const char* warnsystem::GetDate()
{
	return __DATE__;
}

const char *warnsystem::GetLogTag()
{
	return "[as_warnsystem]";
}

const char* warnsystem::GetAuthor()
{
	return "ABKAM";
}

const char* warnsystem::GetDescription()
{
	return "[AS] WarnSystem";
}

const char* warnsystem::GetName()
{
	return "[AS] WarnSystem";
}

const char* warnsystem::GetURL()
{
	return "https://discord.gg/ChYfTtrtmS";
}
