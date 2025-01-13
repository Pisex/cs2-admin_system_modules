#include <stdio.h>
#include "as_blockdb.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include <steam/steam_gameserver.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <thread>
#include <condition_variable>

BlockDB g_BlockDB;
PLUGIN_EXPOSE(BlockDB, g_BlockDB);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

CSteamGameServerAPIContext g_steamAPI;
ISteamHTTP *g_http = nullptr;

const char* g_szApiKey;
const char* g_szWebHook;
int g_iAutoBlockBanCount;
const char* g_szServerName;

std::map<uint64, int> g_mLastConnectTime;

SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);

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

bool BlockDB::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, this, &BlockDB::OnGameServerSteamAPIActivated, false);
	
	ConVar_Register( FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY ); 

	return true;
}

void BlockDB::OnGameServerSteamAPIActivated()
{
	std::thread(&BlockDB::Authorization, this).detach();
	RETURN_META(MRES_IGNORED);
}

void BlockDB::Authorization()
{
	int uAttempts = 300;
	while (--uAttempts)
	{
		if (engine->GetGameServerSteamID().GetStaticAccountKey())
		{
			g_steamAPI.Init();
			ISteamGameServer* pServer = SteamGameServer();
			g_http = g_steamAPI.SteamHTTP();
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

bool BlockDB::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, this, &BlockDB::OnGameServerSteamAPIActivated, false);
	ConVar_Unregister();
	
	return true;
}

void PlayerPunishCallback(int iSlot, int iType, int iTime, const char* szReason, int iAdminID)
{
	if(iType == RT_BAN)
	{
		uint64 iSteamID64 = g_pPlayersApi->GetSteamID64(iSlot);
		json jRequestBody = {
			{"steamid64", iSteamID64},
			{"reason", szReason},
			{"duration", iTime}
		};
		std::string sRequestBody = jRequestBody.dump();
		std::thread([sRequestBody](){
			char szUrl[256];
			g_SMAPI->Format(szUrl, sizeof(szUrl), "https://api.blockdb.ru/v2/bans");
			auto hReq = g_http->CreateHTTPRequest(k_EHTTPMethodPOST, szUrl);

			g_http->SetHTTPRequestHeaderValue(hReq, "Content-Type", "application/json");
			g_http->SetHTTPRequestHeaderValue(hReq, "Authorization", g_szApiKey);
			g_http->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8*)sRequestBody.c_str(), sRequestBody.size());

			SteamAPICall_t hCall;
			g_http->SendHTTPRequest(hReq, &hCall);
		}).detach();
	}
}

void OfflinePlayerUnpunishCallback(const char* szSteamID64, int iType, int iAdminID)
{
	if(iType == RT_BAN)
	{	
		json jRequestBody = {
			{"steamid64", szSteamID64}
		};
		std::string sRequestBody = jRequestBody.dump();
		std::thread([sRequestBody](){
			char szUrl[256];
			g_SMAPI->Format(szUrl, sizeof(szUrl), "https://api.blockdb.ru/v2/bans");
			auto hReq = g_http->CreateHTTPRequest(k_EHTTPMethodDELETE, szUrl);

			g_http->SetHTTPRequestHeaderValue(hReq, "Content-Type", "application/json");
			g_http->SetHTTPRequestHeaderValue(hReq, "Authorization", g_szApiKey);
			g_http->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8*)sRequestBody.c_str(), sRequestBody.size());

			SteamAPICall_t hCall;
			g_http->SendHTTPRequest(hReq, &hCall);
		}).detach();
	}
}

class PlayerSearchBan
{
public:
    PlayerSearchBan(const std::string& steamID) : m_steamID(steamID), m_Ready(false) {}

    json GetPlayerBans()
    {
        m_Ready = false;
        char szURL[512];
        g_SMAPI->Format(szURL, sizeof(szURL), "https://api.blockdb.ru/v2/bans/%s", m_steamID.c_str());
        auto hReq = g_http->CreateHTTPRequest(k_EHTTPMethodGET, szURL);
		g_http->SetHTTPRequestHeaderValue(hReq, "Content-Type", "application/json");
		g_SMAPI->Format(szURL, sizeof(szURL), "Bearer %s", g_szApiKey);
		g_http->SetHTTPRequestHeaderValue(hReq, "Authorization", szURL);
		
        SteamAPICall_t hCall;
        g_http->SendHTTPRequest(hReq, &hCall);
        m_httpRequestCallback.SetGameserverFlag();
        m_httpRequestCallback.Set(hCall, this, &PlayerSearchBan::OnGetPlayerBans);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_Ready; });
        return m_bans;
    }

private:
    void OnGetPlayerBans(HTTPRequestCompleted_t* pResult, bool bFailed)
    {
        if (bFailed || pResult->m_eStatusCode < 200 || pResult->m_eStatusCode > 299)
        {
            Msg("HTTP request failed with status code %i\n", pResult->m_eStatusCode);
        }
        else
        {
            uint32 size;
            g_http->GetHTTPResponseBodySize(pResult->m_hRequest, &size);
            uint8* response = new uint8[size + 1];
            g_http->GetHTTPResponseBodyData(pResult->m_hRequest, response, size);
            response[size] = 0;

            json jsonResponse;
            if (size > 0)
            {
                jsonResponse = json::parse((char*)response, nullptr, false);
                if (jsonResponse.is_discarded())
                {
                    Msg("Failed parsing JSON from HTTP response: %s\n", (char*)response);
                }
                else
                {
					m_bans = jsonResponse;
                }
            }
            delete[] response;
        }
        if (g_http)
            g_http->ReleaseHTTPRequest(pResult->m_hRequest);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_Ready = true;
        }
        m_cv.notify_one();
    }

    std::string m_steamID;
    json m_bans;
    bool m_Ready;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    CCallResult<PlayerSearchBan, HTTPRequestCompleted_t> m_httpRequestCallback;
};

json GetPlayerBan(const std::string& szSteamID)
{
    PlayerSearchBan fetcher(szSteamID);
    return fetcher.GetPlayerBans();
}
#include <codecvt>
#include <locale>
#include <string>

bool IsValidUTF8(const std::string& str)
{
    try
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wstr = converter.from_bytes(str);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::string CleanInvalidUTF8(const std::string& str)
{
    std::string result;
    for (char c : str)
    {
        if ((c & 0x80) == 0 || (c & 0xC0) == 0xC0)
        {
            result += c;
        }
    }
    return result;
}

std::pair<int, int> GetCounts(const json& bans)
{
	int bansCount = bans.size();
	std::set<uint> uniqueDiscordIds;
	for (const auto& ban : bans)
	{
		if (ban.contains("discord_id") && !ban["discord_id"].is_null())
		{
			try
			{
				uniqueDiscordIds.insert(ban["discord_id"].get<uint>());
			}
			catch (const json::exception& e)
			{
				Msg("Error processing discord_id: %s\n", e.what());
			}
		}
	}
	int bansProjectCount = uniqueDiscordIds.size();
	return std::make_pair(bansCount, bansProjectCount);
}

std::string FormatTime(int iTime)
{
	time_t t = iTime;
	struct tm* tm = localtime(&t);
	char szTime[64];
	strftime(szTime, sizeof(szTime), "%d.%m.%Y %H:%M:%S", tm);
	return szTime;
}

void OnClientAuthorized(int iSlot, uint64 iSteamID64)
{
	if(g_mLastConnectTime.find(iSteamID64) != g_mLastConnectTime.end() && time(nullptr) - g_mLastConnectTime[iSteamID64] < 60)
		return;
	std::thread([iSlot, iSteamID64](){
		try {
			g_mLastConnectTime[iSteamID64] = time(nullptr);
			char szSteamID64[64];
			g_SMAPI->Format(szSteamID64, sizeof(szSteamID64), "%llu", iSteamID64);
			json jBan = GetPlayerBan(szSteamID64);
			if(jBan.is_null() || jBan["bans"].is_null() || jBan["bans"].empty())
				return;
			std::pair<int, int> counts = GetCounts(jBan["bans"]);
			if(counts.second >= g_iAutoBlockBanCount)
			{
				engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
				return;
			}

			if(counts.first == 0)
				return;
			g_SMAPI->Format(szSteamID64, sizeof(szSteamID64), "```%llu```", iSteamID64);
			char szBuffer[256];
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "Список предыдущих нарушений (%i)", counts.first);
			char szName[128];
			g_SMAPI->Format(szName, sizeof(szName), "[%s](https://steamcommunity.com/profiles/%lld)", g_pPlayersApi->GetPlayerName(iSlot), iSteamID64);
			char szBans[1024] = "";
			for (const auto& ban : jBan["bans"])
			{
				char szBan[256];
				std::string reason = "undefined";
				std::string project_name = "undefined";

				try {
					if (ban.contains("reason") && ban["reason"].is_string()) {
						std::string raw_reason = ban["reason"].get<std::string>();
						reason = IsValidUTF8(raw_reason) ? raw_reason : CleanInvalidUTF8(raw_reason);
					}
				} catch (const nlohmann::json::type_error& e) {
					Msg("Error processing reason: %s\n", e.what());
				}

				try {
					if (ban.contains("project_name") && ban["project_name"].is_string()) {
						std::string raw_project_name = ban["project_name"].get<std::string>();
						project_name = IsValidUTF8(raw_project_name) ? raw_project_name : CleanInvalidUTF8(raw_project_name);
					}
				} catch (const nlohmann::json::type_error& e) {
					Msg("Error processing project_name: %s\n", e.what());
				}

				g_SMAPI->Format(szBan, sizeof(szBan), "```- %s | %s | %s```\n", reason.c_str(), project_name.c_str(), FormatTime(ban["created_at"].get<int>()).c_str());
				strcat(szBans, szBan);
			}
			char szFooter[256];
			g_SMAPI->Format(szFooter, sizeof(szFooter), "%s | %s", g_szServerName, FormatTime(time(nullptr)).c_str());
			json j = {
				{"content", nullptr},
				{"embeds", {
					{
						{"title", "Новое подключение нарушителя"},
						{
							"fields", 
							{
								{
									{"name", "Steam Профиль"},
									{"value", szName},
									{"inline", true}
								},
								{
									{"name", "Игрок (SteamID64)"},
									{"value", szSteamID64},
									{"inline", true}
								},
								{
									{"name", szBuffer},
									{"value", szBans}
								}
							}
						},
						{"footer", {
							{"text", szFooter}
						}}
					}
				}},
				{"attachments", nlohmann::json::array()}
			};
			std::string sJson = j.dump(4);
			const char* szJson = sJson.c_str();
			auto hReq = g_http->CreateHTTPRequest(k_EHTTPMethodPOST, g_szWebHook);
			g_http->SetHTTPRequestHeaderValue(hReq, "Content-Type", "application/json");
			g_http->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8*)szJson, strlen(szJson));
			SteamAPICall_t hCall;
			g_http->SendHTTPRequest(hReq, &hCall);
		} catch (const nlohmann::json::type_error& e) {
			std::cerr << "Type error: " << e.what() << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Exception: " << e.what() << std::endl;
		}
	}).detach();
}

void BlockDB::AllPluginsLoaded()
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
	g_pPlayersApi->HookOnClientAuthorized(g_PLID, OnClientAuthorized);
	g_pAdminApi->OnPlayerPunish(g_PLID, PlayerPunishCallback);
	g_pAdminApi->OnOfflinePlayerUnpunish(g_PLID, OfflinePlayerUnpunishCallback);
	{
		KeyValues* hKv = new KeyValues("BlockDB");
		const char *pszPath = "addons/configs/admin_system/BlockDB.ini";

		if (!hKv->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			g_pUtils->ErrorLog("[%s] Failed to load %s", GetLogTag(), pszPath);
			return;
		}
		g_szServerName = hKv->GetString("server_name");
		g_szApiKey = hKv->GetString("ApiKey");
		g_iAutoBlockBanCount = hKv->GetInt("AutoBlockBanCount");
		g_szWebHook = hKv->GetString("webhook");
	}
}

///////////////////////////////////////
const char* BlockDB::GetLicense()
{
	return "GPL";
}

const char* BlockDB::GetVersion()
{
	return "1.0";
}

const char* BlockDB::GetDate()
{
	return __DATE__;
}

const char *BlockDB::GetLogTag()
{
	return "BlockDB";
}

const char* BlockDB::GetAuthor()
{
	return "Pisex";
}

const char* BlockDB::GetDescription()
{
	return "AS BlockDB";
}

const char* BlockDB::GetName()
{
	return "[AS] BlockDB";
}

const char* BlockDB::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
