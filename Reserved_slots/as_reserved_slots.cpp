#include <stdio.h>
#include "as_reserved_slots.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "filesystem.h"

reserved_slots g_reserved_slots;
PLUGIN_EXPOSE(reserved_slots, g_reserved_slots);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;
IPlayersApi* g_pPlayersApi = nullptr;
IAdminApi* g_pAdminApi = nullptr;

struct ReservedSlotsConfig
{
	int publicSlots = 0;
	std::string reserveFlag = "@admin/reserve";
	int kickReasonId = 135; // NETWORK_DISCONNECT_REJECT_SERVERFULL
};

ReservedSlotsConfig g_ReservedCfg;
const char* g_szReservedConfigPath = "addons/configs/admin_system/reserved_slots.ini";

static void LoadReservedSlotsConfig();
static bool HasReserveAccess(int iSlot);
static int CountNonReservePlayers();
static int FindNonReservePlayerToKick(int avoidSlot);
static void KickClientForReserve(int iSlot, int iReasonId);
static void OnClientAuthorized(int iSlot, uint64 iSteamID64);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();

	LoadReservedSlotsConfig();
}

bool reserved_slots::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	return true;
}

bool reserved_slots::Unload(char *error, size_t maxlen)
{
	return true;
}

void reserved_slots::AllPluginsLoaded()
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

	g_pPlayersApi->HookOnClientAuthorized(g_PLID, OnClientAuthorized);
	g_pUtils->StartupServer(g_PLID, StartupServer);
}

static void LoadReservedSlotsConfig()
{
	if (!g_pFullFileSystem)
	{
		return;
	}

	g_ReservedCfg = {};

	if (gpGlobals)
	{
		g_ReservedCfg.publicSlots = gpGlobals->maxClients > 2 ? gpGlobals->maxClients - 2 : gpGlobals->maxClients;
	}

	KeyValues* pKv = new KeyValues("ReservedSlots");
	if (!pKv->LoadFromFile(g_pFullFileSystem, g_szReservedConfigPath))
	{
		if (g_pUtils)
		{
			g_pUtils->ErrorLog("[%s] Failed to load %s, using defaults", g_reserved_slots.GetLogTag(), g_szReservedConfigPath);
		}
		pKv->deleteThis();
		return;
	}

	g_ReservedCfg.reserveFlag = pKv->GetString("reserve_flag", g_ReservedCfg.reserveFlag.c_str());
	g_ReservedCfg.publicSlots = pKv->GetInt("public_slots", g_ReservedCfg.publicSlots);
	g_ReservedCfg.kickReasonId = pKv->GetInt("kick_reason_id", g_ReservedCfg.kickReasonId);

	pKv->deleteThis();

	if (gpGlobals)
	{
		if (g_ReservedCfg.publicSlots < 0)
			g_ReservedCfg.publicSlots = 0;
		if (g_ReservedCfg.publicSlots > gpGlobals->maxClients)
			g_ReservedCfg.publicSlots = gpGlobals->maxClients;
	}

	if (g_ReservedCfg.kickReasonId < 0)
		g_ReservedCfg.kickReasonId = 39;
}

static bool HasReserveAccess(int iSlot)
{
	if (!g_pAdminApi)
		return false;

	if (g_ReservedCfg.reserveFlag.empty())
		return false;

	if (g_pAdminApi->HasPermission(iSlot, g_ReservedCfg.reserveFlag.c_str()))
		return true;

	return g_pAdminApi->HasFlag(iSlot, g_ReservedCfg.reserveFlag.c_str());
}

static int CountNonReservePlayers()
{
	if (!g_pPlayersApi || !gpGlobals)
		return 0;

	int iCount = 0;
	for (int i = 0; i < gpGlobals->maxClients; ++i)
	{
		if (!g_pPlayersApi->IsConnected(i) || g_pPlayersApi->IsFakeClient(i))
			continue;
		if (HasReserveAccess(i))
			continue;
		iCount++;
	}
	return iCount;
}

static int FindNonReservePlayerToKick(int avoidSlot)
{
	if (!g_pPlayersApi || !gpGlobals)
		return -1;

	for (int i = 0; i < gpGlobals->maxClients; ++i)
	{
		if (i == avoidSlot)
			continue;
		if (!g_pPlayersApi->IsConnected(i) || g_pPlayersApi->IsFakeClient(i))
			continue;
		if (HasReserveAccess(i))
			continue;
		return i;
	}

	return -1;
}

static void KickClientForReserve(int iSlot, int iReasonId)
{
	if (!engine || !g_pPlayersApi)
		return;

	if (!g_pPlayersApi->IsConnected(iSlot))
		return;

	engine->DisconnectClient(CPlayerSlot(iSlot), static_cast<ENetworkDisconnectionReason>(iReasonId));
}

static void OnClientAuthorized(int iSlot, uint64 iSteamID64)
{
	if (!g_pPlayersApi || !g_pAdminApi || !gpGlobals)
		return;

	if (g_pPlayersApi->IsFakeClient(iSlot))
		return;

	const int iMaxClients = gpGlobals->maxClients;
	if (iMaxClients <= 0)
		return;

	int iPublicSlots = g_ReservedCfg.publicSlots;
	if (iPublicSlots < 0)
		iPublicSlots = 0;
	if (iPublicSlots > iMaxClients)
		iPublicSlots = iMaxClients;

	const bool bHasReserve = HasReserveAccess(iSlot);
	int iNonReserve = CountNonReservePlayers();

	if (!bHasReserve && iNonReserve > iPublicSlots)
	{
		KickClientForReserve(iSlot, g_ReservedCfg.kickReasonId);
		return;
	}

	if (bHasReserve && iNonReserve > iPublicSlots)
	{
		int iTarget = FindNonReservePlayerToKick(iSlot);
		if (iTarget == -1)
		{
			return;
		}
		KickClientForReserve(iTarget, g_ReservedCfg.kickReasonId);
	}
}

const char* reserved_slots::GetLicense()
{
	return "GPL";
}

const char* reserved_slots::GetVersion()
{
	return "1.0";
}

const char* reserved_slots::GetDate()
{
	return __DATE__;
}

const char *reserved_slots::GetLogTag()
{
	return "[as_reserved_slots]";
}

const char* reserved_slots::GetAuthor()
{
	return "ABKAM";
}

const char* reserved_slots::GetDescription()
{
	return "[AS] Reserved slots";
}

const char* reserved_slots::GetName()
{
	return "[AS] Reserved slots";
}

const char* reserved_slots::GetURL()
{
	return "https://discord.gg/ChYfTtrtmS";
}
