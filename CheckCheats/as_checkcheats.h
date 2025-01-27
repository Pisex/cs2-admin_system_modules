#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include <utlstring.h>
#include <KeyValues.h>
#include "CCSPlayerController.h"
#include "CParticleSystem.h"
#include "include/menus.h"
#include "include/admin.h"
#include "include/mysql_mm.h"

struct Reasons
{
	std::string sReason;
	std::string sReason2;
	int iTime;
	int iMin;
	int iMax;
	bool bShow;
};

struct Timer
{
	int iTime;
	int iMin;
	int iMax;
	bool bAutoBan;
	int iBanReason;
};

struct Socials
{
	std::string sName;
	int iMin;
	int iMax;
	std::string sExample;
};

struct Leave
{
	bool bAutoBan;
	int iBanReason;
	bool bLeaveMenu;
	int iMin;
	int iMax;
};


class CheckCheats final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
private:
	void OnCheckTransmit(CCheckTransmitInfo **ppInfoList, int infoCount, CBitVec<16384> &unionTransmitEdicts, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities, bool bEnablePVSBits);
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
