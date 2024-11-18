#include <stdio.h>
#include "as_funcommands.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

FunCommands g_Sample;
PLUGIN_EXPOSE(FunCommands, g_Sample);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IMenusApi* g_pMenus;
IPlayersApi* g_pPlayersApi;
IAdminApi* g_pAdminApi;

const char* g_szLastItem[64];

void OnItemSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
void SetGravity(int iSlot, int iTarget, float fGravity);
void SetHP(int iSlot, int iTarget, int iHP);
void SetArmor(int iSlot, int iTarget, int iArmor);
void SetScale(int iSlot, int iTarget, float fScale);
void SetSpeed(int iSlot, int iTarget, float fSpeed);
void SetGod(int iSlot, int iTarget, bool bGod);
void SetFreezy(int iSlot, int iTarget, bool bFreezy);
void BlindPlayer(int iSlot, int iTarget, bool bBlind);
void BuryPlayer(int iSlot, int iTarget);
void UnburyPlayer(int iSlot, int iTarget);

CON_COMMAND_F(mm_gravity, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/gravity") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Gravity"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	float fGravity = std::atof(args.Arg(2));
	SetGravity(iSlot, iTarget, fGravity);
}

CON_COMMAND_F(mm_hp, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/hp") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_HP"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	int iHP = std::atoi(args.Arg(2));
	SetHP(iSlot, iTarget, iHP);
}

CON_COMMAND_F(mm_armor, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/armor") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Armor"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	int iArmor = std::atoi(args.Arg(2));
	SetArmor(iSlot, iTarget, iArmor);
}

CON_COMMAND_F(mm_scale, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/scale") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Scale"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	float fScale = std::atof(args.Arg(2));
	SetScale(iSlot, iTarget, fScale);
}

CON_COMMAND_F(mm_speed, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/speed") && !bConsole) return;
	if(args.ArgC() < 3)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Speed"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	float fSpeed = std::atof(args.Arg(2));
	SetSpeed(iSlot, iTarget, fSpeed);
}

CON_COMMAND_F(mm_god, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/god") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_God"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	bool bGod = args.ArgC() > 2 ? std::atoi(args.Arg(2)) : true;
	SetGod(iSlot, iTarget, bGod);
}

CON_COMMAND_F(mm_freezy, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/freezy") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Freeze"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	float fTime = args.ArgC() > 2 ? std::atof(args.Arg(2)) : 0;
	SetFreezy(iSlot, iTarget, true);
	if(fTime > 0)
	{
		g_pUtils->CreateTimer(fTime, [iSlot, iTarget]() {
			SetFreezy(iSlot, iTarget, false);
			return -1.0f;
		});
	}
}

CON_COMMAND_F(mm_unfreezy, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/freezy") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Unfreeze"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	SetFreezy(iSlot, iTarget, false);
}

CON_COMMAND_F(mm_blind, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/blind") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Blind"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	float fTime = args.ArgC() > 2 ? std::atof(args.Arg(2)) : 0;
	BlindPlayer(iSlot, iTarget, true);
	if(fTime > 0)
	{
		g_pUtils->CreateTimer(fTime, [iSlot, iTarget]() {
			BlindPlayer(iSlot, iTarget, false);
			return -1.0f;
		});
	}
}

CON_COMMAND_F(mm_unblind, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/blind") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Unblind"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	BlindPlayer(iSlot, iTarget, false);
}

CON_COMMAND_F(mm_bury, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/bury") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Bury"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	BuryPlayer(iSlot, iTarget);
}

CON_COMMAND_F(mm_unbury, "", FCVAR_NONE)
{
	int iSlot = context.GetPlayerSlot().Get();
	bool bConsole = iSlot == -1;
	if(!g_pAdminApi->HasPermission(iSlot, "@admin/bury") && !bConsole) return;
	if(args.ArgC() < 2)
	{
		char szBuffer[256];
		g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("Usage_Unbury"), args.Arg(0));
		if(bConsole)
			META_CONPRINTF("%s\n", szBuffer);
		else
			g_pUtils->PrintToConsole(iSlot, "%s\n", szBuffer);
		return;
	}
	int iTarget = std::atoi(args.Arg(1));
	UnburyPlayer(iSlot, iTarget);
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

bool FunCommands::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );
	
	ConVar_Register(FCVAR_GAMEDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_NOTIFY);

	return true;
}

void SetGravity(int iSlot, int iTarget, float fGravity)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_flGravityScale() = fGravity;
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminGravityMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), fGravity);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("GravityMessage"), g_pPlayersApi->GetPlayerName(iTarget), fGravity);
	
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %f", iTarget, fGravity);
	g_pAdminApi->SendAction(iSlot, "gravity", szBuffer);
}

void SetHP(int iSlot, int iTarget, int iHP)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_iHealth() = iHP;
	g_pUtils->SetStateChanged(pPlayerPawn, "CBaseEntity", "m_iHealth");
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminHPMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iHP);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("HPMessage"), g_pPlayersApi->GetPlayerName(iTarget), iHP);

	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, iHP);
	g_pAdminApi->SendAction(iSlot, "hp", szBuffer);
}

void SetArmor(int iSlot, int iTarget, int iArmor)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_ArmorValue() = iArmor;
	g_pUtils->SetStateChanged(pPlayerPawn, "CCSPlayerPawn", "m_ArmorValue");
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminArmorMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), iArmor);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("ArmorMessage"), g_pPlayersApi->GetPlayerName(iTarget), iArmor);
	
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, iArmor);
	g_pAdminApi->SendAction(iSlot, "armor", szBuffer);
}

void SetScale(int iSlot, int iTarget, float fScale)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_CBodyComponent()->m_pSceneNode()->m_flScale() = fScale;
	g_pUtils->SetStateChanged(pPlayerPawn, "CBaseEntity", "m_CBodyComponent");
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminScaleMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), fScale);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("ScaleMessage"), g_pPlayersApi->GetPlayerName(iTarget), fScale);
	
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %f", iTarget, fScale);
	g_pAdminApi->SendAction(iSlot, "scale", szBuffer);
}

void SetSpeed(int iSlot, int iTarget, float fSpeed)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_flVelocityModifier() = fSpeed;
	g_pUtils->SetStateChanged(pPlayerPawn, "CCSPlayerPawn", "m_flVelocityModifier");
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminSpeedMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget), fSpeed);
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("SpeedMessage"), g_pPlayersApi->GetPlayerName(iTarget), fSpeed);
	
	char szBuffer[128];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %f", iTarget, fSpeed);
	g_pAdminApi->SendAction(iSlot, "speed", szBuffer);
}

void SetGod(int iSlot, int iTarget, bool bGod)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	pPlayerPawn->m_bTakesDamage() = bGod;
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
	{
		if(bGod)
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("AdminGodMessageEnable"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("AdminGodMessageDisable"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		g_pUtils->PrintToChatAll(szBuffer);
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		if(bGod)
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("GodMessageEnable"), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("GodMessageDisable"), g_pPlayersApi->GetPlayerName(iTarget));
		g_pUtils->PrintToChatAll(szBuffer);
	}
	
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, bGod);
	g_pAdminApi->SendAction(iSlot, "god", szBuffer);
}

void SetFreezy(int iSlot, int iTarget, bool bFreeze)
{
	if(bFreeze) g_pPlayersApi->SetMoveType(iTarget, MOVETYPE_OBSOLETE);
	else g_pPlayersApi->SetMoveType(iTarget, MOVETYPE_WALK);
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
	{
		if(bFreeze)
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("AdminFreezeMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("AdminUnfreezeMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		g_pUtils->PrintToChatAll(szBuffer);
	}
	else if(g_pAdminApi->GetMessageType() == 1)
	{
		if(bFreeze)
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("FreezeMessage"), g_pPlayersApi->GetPlayerName(iTarget));
		else
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pAdminApi->GetTranslation("UnfreezeMessage"), g_pPlayersApi->GetPlayerName(iTarget));
		g_pUtils->PrintToChatAll(szBuffer);
	}
	
	g_pAdminApi->SendAction(iSlot, bFreeze?"freeze":"unfreeze", std::to_string(iTarget).c_str());
}

void BlindPlayer(int iSlot, int iTarget, bool bBlind)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	char szBuffer[128];
	if(bBlind)
	{
		pPlayerPawn->m_flFlashDuration() = 9999;
		pPlayerPawn->m_flFlashMaxAlpha() = 255;
		pPlayerPawn->m_blindUntilTime.Set(gpGlobals->curtime + 9999);
		pPlayerPawn->m_blindStartTime.Set(gpGlobals->curtime);
		g_pUtils->SetStateChanged(pPlayerPawn, "CCSPlayerPawnBase", "m_flFlashDuration");
		g_pUtils->SetStateChanged(pPlayerPawn, "CCSPlayerPawnBase", "m_flFlashMaxAlpha");
		if(g_pAdminApi->GetMessageType() == 2)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminBlindMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		else if(g_pAdminApi->GetMessageType() == 1)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("BlindMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	}
	else
	{
		pPlayerPawn->m_blindUntilTime.Set(gpGlobals->curtime - 1);
		if(g_pAdminApi->GetMessageType() == 2)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminUnblindMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
		else if(g_pAdminApi->GetMessageType() == 1)
			g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("UnblindMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	}
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%i %i", iTarget, pPlayerPawn->m_flFlashDuration() > 0);
	g_pAdminApi->SendAction(iSlot, "blind", szBuffer);
}

void BuryPlayer(int iSlot, int iTarget)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	Vector vOrigin = pPlayerPawn->GetAbsOrigin();
	vOrigin.z -= 10.0f;
	QAngle pAngles = pPlayerPawn->GetAbsRotation();
	Vector vVelocity = pPlayerPawn->GetAbsVelocity();
	g_pUtils->TeleportEntity(pPlayerPawn, &vOrigin, &pAngles, &vVelocity);
	char szBuffer[128];
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminBuryMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("BuryMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	
	g_pAdminApi->SendAction(iSlot, "bury", std::to_string(iTarget).c_str());
}

void UnburyPlayer(int iSlot, int iTarget)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;
	Vector vOrigin = pPlayerPawn->GetAbsOrigin();
	vOrigin.z += 10.0f;
	QAngle pAngles = pPlayerPawn->GetAbsRotation();
	Vector vVelocity = pPlayerPawn->GetAbsVelocity();
	g_pUtils->TeleportEntity(pPlayerPawn, &vOrigin, &pAngles, &vVelocity);
	if(g_pAdminApi->GetMessageType() == 2)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("AdminUnburyMessage"), iSlot == -1?"Console":g_pPlayersApi->GetPlayerName(iSlot), g_pPlayersApi->GetPlayerName(iTarget));
	else if(g_pAdminApi->GetMessageType() == 1)
		g_pUtils->PrintToChatAll(g_pAdminApi->GetTranslation("UnburyMessage"), g_pPlayersApi->GetPlayerName(iTarget));
	
	g_pAdminApi->SendAction(iSlot, "unbury", std::to_string(iTarget).c_str());
}

void ShowBaseStatMenu(int iSlot, int iTarget, bool bHealth)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;

	Menu hMenu;
	char szBuffer[256];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%i]", g_pPlayersApi->GetPlayerName(iTarget), bHealth?pPlayerPawn->m_iHealth():pPlayerPawn->m_ArmorValue());
	g_pMenus->SetTitleMenu(hMenu, szBuffer);
	g_pMenus->AddItemMenu(hMenu, "1", "1");
	g_pMenus->AddItemMenu(hMenu, "5", "5");
	g_pMenus->AddItemMenu(hMenu, "10", "10");
	g_pMenus->AddItemMenu(hMenu, "20", "20");
	g_pMenus->AddItemMenu(hMenu, "50", "50");
	g_pMenus->AddItemMenu(hMenu, "100", "100");
	g_pMenus->AddItemMenu(hMenu, "200", "200");
	g_pMenus->AddItemMenu(hMenu, "500", "500");
	g_pMenus->AddItemMenu(hMenu, "1000", "1000");
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget, bHealth](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iValue = std::atoi(szBack);
			if(bHealth)
				SetHP(iSlot, iTarget, iValue);
			else
				SetArmor(iSlot, iTarget, iValue);
			ShowBaseStatMenu(iSlot, iTarget, bHealth);
		}
		else if(iItem == 7)
		{
			if(bHealth)
				OnItemSelect(iSlot, "players", "hp", "hp");
			else
				OnItemSelect(iSlot, "players", "armor", "armor");
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void ShowSpeedMenu(int iSlot, int iTarget, bool bSpeed)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;

	Menu hMenu;
	char szBuffer[256];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%.1fx]", g_pPlayersApi->GetPlayerName(iTarget), bSpeed?pPlayerPawn->m_flVelocityModifier():pPlayerPawn->m_CBodyComponent()->m_pSceneNode()->m_flScale());
	g_pMenus->SetTitleMenu(hMenu, szBuffer);
	g_pMenus->AddItemMenu(hMenu, "0.1", "0.1x");
	g_pMenus->AddItemMenu(hMenu, "0.2", "0.2x");
	g_pMenus->AddItemMenu(hMenu, "0.5", "0.5x");
	g_pMenus->AddItemMenu(hMenu, "1.0", "1x");
	g_pMenus->AddItemMenu(hMenu, "1.1", "1.1x");
	g_pMenus->AddItemMenu(hMenu, "1.2", "1.2x");
	g_pMenus->AddItemMenu(hMenu, "1.5", "1.5x");
	g_pMenus->AddItemMenu(hMenu, "2.0", "2x");
	g_pMenus->AddItemMenu(hMenu, "5.0", "5x");
	g_pMenus->AddItemMenu(hMenu, "10.0", "10x");
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget, bSpeed](const char* szBack, const char* szFront, int iItem, int iSlot){
		if(iItem < 7)
		{
			if(bSpeed) SetSpeed(iSlot, iTarget, std::atof(szBack));
			else SetScale(iSlot, iTarget, std::atof(szBack));
			ShowSpeedMenu(iSlot, iTarget, bSpeed);
		}
		else if(iItem == 7)
		{
			if(bSpeed) OnItemSelect(iSlot, "players", "speed", "speed");
			else OnItemSelect(iSlot, "players", "scale", "scale");
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void ShowGravityMenu(int iSlot, int iTarget)
{
	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
	if(!pPlayerController) return;
	CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
	if(!pPlayerPawn) return;

	Menu hMenu;
	char szBuffer[256];
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%.1fx]", g_pPlayersApi->GetPlayerName(iTarget), pPlayerPawn->m_flGravityScale());
	g_pMenus->SetTitleMenu(hMenu, szBuffer);
	g_pMenus->AddItemMenu(hMenu, "0.1", "0.1x");
	g_pMenus->AddItemMenu(hMenu, "0.2", "0.2x");
	g_pMenus->AddItemMenu(hMenu, "0.5", "0.5x");
	g_pMenus->AddItemMenu(hMenu, "0.7", "0.7x");
	g_pMenus->AddItemMenu(hMenu, "0.8", "0.8x");
	g_pMenus->AddItemMenu(hMenu, "1.0", "1x");
	g_pMenus->AddItemMenu(hMenu, "1.1", "1.1x");
	g_pMenus->AddItemMenu(hMenu, "1.2", "1.2x");
	g_pMenus->AddItemMenu(hMenu, "1.5", "1.5x");
	g_pMenus->AddItemMenu(hMenu, "2.0", "2x");
	g_pMenus->AddItemMenu(hMenu, "5.0", "5x");
	g_pMenus->AddItemMenu(hMenu, "10.0", "10x");
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [iTarget](const char* szBack, const char* szFront, int iItem, int iSlot){
		if(iItem < 7)
		{
			SetGravity(iSlot, iTarget, std::atof(szBack));
			ShowGravityMenu(iSlot, iTarget);
		}
		else if(iItem == 7)
		{
			OnItemSelect(iSlot, "players", "gravity", "gravity");
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

bool FunCommands::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnItemSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
	g_szLastItem[iSlot] = strdup(szIdentity);
	Menu hMenu;
	g_pMenus->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation("SelectPlayer"));
	char szBuffer[128];
	for (int i = 0; i < 64; i++)
	{
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if(!pPlayerController) continue;
		CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
		if(!pPlayerPawn) continue;
		if (!pPlayerPawn->IsAlive()) continue;
		if(!strcmp(szIdentity, "freeze"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%s]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_nActualMoveType() == MOVETYPE_OBSOLETE?"ON":"OFF");
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "gravity"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%.1fx]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_flGravityScale());
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "hp"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%i]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_iHealth());
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "armor"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%i]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_ArmorValue());
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "speed"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%.1fx]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_flVelocityModifier());
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "scale"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%.1fx]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_CBodyComponent()->m_pSceneNode()->m_flScale());
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "god"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%s]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_bTakesDamage()?"OFF":"ON");
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
		else if(!strcmp(szIdentity, "bury"))
		{
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pPlayersApi->GetPlayerName(i));
		}
		else if(!strcmp(szIdentity, "unbury"))
		{
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pPlayersApi->GetPlayerName(i));
		}
		else if(!strcmp(szIdentity, "blind"))
		{
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), "%s [%s]", g_pPlayersApi->GetPlayerName(i), pPlayerPawn->m_flFlashDuration() > 0?"ON":"OFF");
			g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), szBuffer);
		}
	}
	g_pMenus->SetBackMenu(hMenu, true);
	g_pMenus->SetExitMenu(hMenu, true);
	g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if(iItem < 7)
		{
			int iTarget = std::atoi(szBack);
			CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iTarget);
			if(!pPlayerController) return;
			CCSPlayerPawn* pPlayerPawn = pPlayerController->GetPlayerPawn();
			if(!pPlayerPawn) return;
			if(!strcmp(g_szLastItem[iSlot], "freeze"))
			{
				SetFreezy(iSlot, iTarget, pPlayerPawn->m_nActualMoveType() == MOVETYPE_OBSOLETE?false:true);
				OnItemSelect(iSlot, "players", "freeze", "@admin/freeze");
			}
			else if(!strcmp(g_szLastItem[iSlot], "gravity"))
			{
				ShowGravityMenu(iSlot, iTarget);
			}
			else if(!strcmp(g_szLastItem[iSlot], "hp"))
			{
				ShowBaseStatMenu(iSlot, iTarget, true);
			}
			else if(!strcmp(g_szLastItem[iSlot], "armor"))
			{
				ShowBaseStatMenu(iSlot, iTarget, false);
			}
			else if(!strcmp(g_szLastItem[iSlot], "speed"))
			{
				ShowSpeedMenu(iSlot, iTarget, true);
			}
			else if(!strcmp(g_szLastItem[iSlot], "scale"))
			{
				ShowSpeedMenu(iSlot, iTarget, false);
			}
			else if(!strcmp(g_szLastItem[iSlot], "god"))
			{
				SetGod(iSlot, iTarget, !pPlayerPawn->m_bTakesDamage());
				OnItemSelect(iSlot, "players", "god", "@admin/god");
			}
			else if(!strcmp(g_szLastItem[iSlot], "bury"))
			{
				BuryPlayer(iSlot, iTarget);
				OnItemSelect(iSlot, "players", "bury", "@admin/bury");
			}
			else if(!strcmp(g_szLastItem[iSlot], "unbury"))
			{
				UnburyPlayer(iSlot, iTarget);
				OnItemSelect(iSlot, "players", "unbury", "@admin/unbury");
			}
			else if(!strcmp(g_szLastItem[iSlot], "blind"))
			{
				BlindPlayer(iSlot, iTarget, pPlayerPawn->m_flFlashDuration() > 0?false:true);
				OnItemSelect(iSlot, "players", "blind", "@admin/blind");
			}
		}
		else
		{
			g_pAdminApi->ShowAdminLastCategoryMenu(iSlot);
		}
	});
	g_pMenus->DisplayPlayerMenu(hMenu, iSlot);
}

void FunCommands::AllPluginsLoaded()
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
	g_pMenus = (IMenusApi *)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
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
	g_pAdminApi->OnCoreLoaded(g_PLID, [](){
		g_pAdminApi->RegisterItem("freeze", g_pAdminApi->GetTranslation("Item_Freeze"), "players", "@admin/freeze", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("gravity", g_pAdminApi->GetTranslation("Item_Gravity"), "players", "@admin/gravity", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("scale", g_pAdminApi->GetTranslation("Item_Scale"), "players", "@admin/scale", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("hp", g_pAdminApi->GetTranslation("Item_HP"), "players", "@admin/hp", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("armor", g_pAdminApi->GetTranslation("Item_Armor"), "players", "@admin/armor", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("speed", g_pAdminApi->GetTranslation("Item_Speed"), "players", "@admin/speed", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("god", g_pAdminApi->GetTranslation("Item_God"), "players", "@admin/god", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("bury", g_pAdminApi->GetTranslation("Item_Bury"), "players", "@admin/bury", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("unbury", g_pAdminApi->GetTranslation("Item_Unbury"), "players", "@admin/unbury", nullptr, OnItemSelect);
		g_pAdminApi->RegisterItem("blind", g_pAdminApi->GetTranslation("Item_Blind"), "players", "@admin/blind", nullptr, OnItemSelect);
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!freeze"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/freeze")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Freeze"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		float fTime = args.ArgC() > 2?std::atof(args[2]):0;
		SetFreezy(iSlot, iTarget, true);
		if(fTime > 0)
		{
			g_pUtils->CreateTimer(fTime, [iSlot, iTarget](){
				SetFreezy(iSlot, iTarget, false);
				return -1.0f;
			});
		}
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!unfreeze"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/freeze")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Unfreeze"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		SetFreezy(iSlot, iTarget, false);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!gravity"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/gravity")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Gravity"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		float fGravity = std::atof(args[2]);
		SetGravity(iSlot, iTarget, fGravity);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!scale"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/scale")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Scale"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		float fScale = std::atof(args[2]);
		SetScale(iSlot, iTarget, fScale);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!hp"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/hp")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_HP"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		int iHP = std::atoi(args[2]);
		SetHP(iSlot, iTarget, iHP);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!armor"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/armor")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Armor"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		int iArmor = std::atoi(args[2]);
		SetArmor(iSlot, iTarget, iArmor);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!speed"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/speed")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Speed"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		float fSpeed = std::atof(args[2]);
		SetSpeed(iSlot, iTarget, fSpeed);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!god"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/god")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 3)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_God"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		bool bGod = std::atoi(args[2]);
		SetGod(iSlot, iTarget, bGod);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!bury"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/bury")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Bury"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		BuryPlayer(iSlot, iTarget);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!unbury"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/unbury")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Unbury"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		UnburyPlayer(iSlot, iTarget);
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!blind"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/blind")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Blind"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		float fTime = args.ArgC() > 2?std::atof(args[2]):0;
		BlindPlayer(iSlot, iTarget, true);
		if(fTime > 0)
		{
			g_pUtils->CreateTimer(fTime, [iSlot, iTarget](){
				BlindPlayer(iSlot, iTarget, false);
				return -1.0f;
			});
		}
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!unblind"}, [](int iSlot, const char* szContent){
		if(!g_pAdminApi->HasPermission(g_PLID, "@admin/blind")) return true;
		CCommand args;
		args.Tokenize(szContent);
		if(args.ArgC() < 2)
		{
			g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("Usage_Unblind"), args[0]);
			return true;
		}
		int iTarget = std::atoi(args[1]);
		BlindPlayer(iSlot, iTarget, false);
		return true;
	});
}

///////////////////////////////////////
const char* FunCommands::GetLicense()
{
	return "GPL";
}

const char* FunCommands::GetVersion()
{
	return "1.0";
}

const char* FunCommands::GetDate()
{
	return __DATE__;
}

const char *FunCommands::GetLogTag()
{
	return "FunCommands";
}

const char* FunCommands::GetAuthor()
{
	return "Pisex";
}

const char* FunCommands::GetDescription()
{
	return "AS FunCommands";
}

const char* FunCommands::GetName()
{
	return "[AS] FunCommands";
}

const char* FunCommands::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
