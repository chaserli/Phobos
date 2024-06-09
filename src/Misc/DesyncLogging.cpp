#ifndef IS_RELEASE_VER
#include <Utilities/Macro.h>
#include <Utilities/Debug.h>
#include <Ext/RadSite/Body.h>
#include <Ext/Bullet/Body.h>
#include <Ext/Anim/Body.h>
#include <Ext/Techno/Body.h>
#include <Utilities/AresHelper.h>
#include <AircraftClass.h>
#include <EventClass.h>
#include <FPSCounter.h>
#include <GameOptionsClass.h>
#include <CCINIClass.h>
#include <CRC.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <chrono>


class DesyncLogger
{
	using json = nlohmann::json;

	int FrameIdx;
	EventClass* OffendingEvent;
public:

	json MainFile;

	explicit DesyncLogger(int slot, EventClass* offendingEvent) :
		FrameIdx { slot }, OffendingEvent { offendingEvent },
		MainFile {}
	{	}

	explicit DesyncLogger(EventClass* offendingEvent) :
		FrameIdx { 0 }, OffendingEvent { offendingEvent },
		MainFile {}
	{ }

private:

	void WriteMetaInfo()
	{
		json network;

		network["CurrentFrame"] = Unsorted::CurrentFrame();
		network["AverageFPS"] = FPSCounter::GetAverageFrameRate();
		network["MaxAhead"] = Game::Network.MaxAhead();
		network["MaxMaxAhead"] = Game::Network.MaxMaxAhead();
		network["LatencyFudge"] = Game::Network.LatencyFudge();
		network["GameSpeed"] = GameOptionsClass::Instance->GameSpeed;
		network["FrameSendRate"] = Game::Network.FrameSendRate();

		json crcs = json::array();
		for (auto i = 0; i < 0x100; ++i)
			crcs.push_back(EventClass::LatestFramesCRC[i]);
		network["LatestFramesCRC"] = std::move(crcs);

		json ini_hash;

		DWORD* base = reinterpret_cast<DWORD*>(SessionClass::Instance->GameMode == GameMode::LAN ? 0xAC026C : 0xB77E00);
		ini_hash["Rules"] = base[0] ? base[0] : reinterpret_cast<DWORD(*)()>(0x679D90)();
		ini_hash["Art"] = base[1] ? base[1] : reinterpret_cast<DWORD(*)()>(0x679EC0)();
		ini_hash["AI"] = base[2] ? base[2] : reinterpret_cast<DWORD(*)()>(0x679ED0)();

		MainFile["Network"] = std::move(network);
		MainFile["INIHash"] = std::move(ini_hash);

		switch (AresHelper::AresVersion)
		{
		case AresHelper::Version::Ares30:
			MainFile["AresVersion"] = "3.0";
			break;
		case AresHelper::Version::Ares30p:
			MainFile["AresVersion"] = "3.0p1";
			break;
		default:
			MainFile["AresVersion"] = "Unknown";
		}

		MainFile["PhobosVersion"] = "Devbuild" _STR(BUILD_NUMBER)

#ifdef STR_GIT_COMMIT
			"(" STR_GIT_COMMIT " @ " STR_GIT_BRANCH ")"
#endif
			;
		const auto now = std::chrono::system_clock::now();
		const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
		MainFile["DumpTime"] = std::ctime(&t_c);

		MainFile["CurrentPlayerName"] = HouseClass::CurrentPlayer->PlainName;

		// TODO: more info

	}

	void WriteOffendingEvent()
	{
		if (!OffendingEvent)return;
		json offend;

		offend["Type"] = EventClass::EventNames[(int)OffendingEvent->Type];
		offend["Frame"] = OffendingEvent->Frame;
		offend["House"] = (int)OffendingEvent->HouseIndex;
		if (OffendingEvent->Type == EventType::FrameInfo)
		{
			offend["CommandCount"] = OffendingEvent->FrameInfo.CommandCount;
			offend["CRC"] = OffendingEvent->FrameInfo.CRC;
			offend["Delay"] = OffendingEvent->FrameInfo.Delay;
		}

		MainFile["OffendingEvent"] = std::move(offend);
		//...
	}

	void WriteRNG()
	{
		json randomizer;
		auto const& random = ScenarioClass::Instance->Random;
		randomizer["unknown_00"] = random.unknown_00;
		randomizer["Next1"] = random.Next1;
		randomizer["Next2"] = random.Next2;

		json randomizer_table = json::array();
		for (auto const& i : random.Table)
			randomizer_table.push_back(i);
		randomizer["Table"] = std::move(randomizer_table);

		MainFile["Randomizer"] = std::move(randomizer);
	}

	static json FromAbstract(AbstractClass const* abst)
	{
		json jAbs;
		jAbs["UniqueID"] = abst->UniqueID;
		jAbs["Dirty"] = abst->Dirty;
		CRCEngine c {};
		abst->ComputeCRC(c);
		jAbs["Checksum"] = (DWORD)c.CRC;
		jAbs["RTTI"] = abst->GetClassName();
		return jAbs;
	}

	static json FromObject(ObjectClass const* obj)
	{
		json jObj = FromAbstract(obj);
		if (obj->NextObject)
			jObj["NextObject"] = obj->NextObject->UniqueID;
		if (obj->AttachedTag)
			jObj["AttachedTag"] = obj->AttachedTag->UniqueID;
		jObj["Health"] = obj->Health;
		jObj["IsOnMap"] = obj->IsOnMap;
		jObj["InLimbo"] = obj->InLimbo;
		jObj["HasParachute"] = obj->HasParachute;
		jObj["OnBridge"] = obj->OnBridge;
		jObj["IsFallingDown"] = obj->IsFallingDown;
		jObj["IsABomb"] = obj->IsABomb;
		jObj["IsAlive"] = obj->IsAlive;
		jObj["Location"] = { obj->Location.X,obj->Location.Y,obj->Location.Z };
		return jObj;
	}

	static json FromAnim(AnimClass const* anim)
	{
		json jAnim = FromObject(anim);
		if (anim->OwnerObject)
			jAnim["OwnerObject"] = anim->OwnerObject->UniqueID;
		if (anim->Owner)
			jAnim["Owner"] = anim->Owner->PlainName;
		jAnim["TintColor"] = anim->TintColor;
		jAnim["ZAdjust"] = anim->ZAdjust;
		jAnim["YSortAdjust"] = anim->YSortAdjust;
		jAnim["LoopDelay"] = anim->LoopDelay;
		jAnim["Accum"] = anim->Accum;
		jAnim["AnimFlags"] = (int)anim->AnimFlags;
		jAnim["HasExtras"] = anim->HasExtras;
		jAnim["RemainingIterations"] = anim->RemainingIterations;
		jAnim["Type"] = anim->Type->ID;

		json phoboshit;
		auto const aext = AnimExt::ExtMap.Find(anim);
		if (aext->Invoker)
			phoboshit["Invoker"] = aext->Invoker->UniqueID;
		if (aext->InvokerHouse)
			phoboshit["InvokerHouse"] = aext->InvokerHouse->PlainName;
		phoboshit["FromDeathUnit"] = aext->FromDeathUnit;
		jAnim["Phobos"] = std::move(phoboshit);

		return jAnim;
	}

	static json FromRad(RadSiteClass const* rad)
	{
		json jRad = FromAbstract(rad);
		// not crc-ed
		json phoboshit;
		auto radext = RadSiteExt::ExtMap.Find(rad);
		if (radext->RadHouse)
			phoboshit["RadHouse"] = radext->RadHouse->PlainName;
		if (radext->RadInvoker)
			phoboshit["RadInvoker"] = radext->RadInvoker->UniqueID;
		phoboshit["Type"] = radext->Type->Name.data();
		jRad["Phobos"] = std::move(phoboshit);

		return jRad;
	}

	static json FromBullet(BulletClass const* bullet)
	{
		json jBul = FromObject(bullet);
		// not crc-ed
		if (bullet->Owner)
			jBul["Owner"] = bullet->Owner->UniqueID;
		jBul["Type"] = bullet->Type->ID;
		if (bullet->Target)
			jBul["Target"] = bullet->Target->UniqueID;

		json phoboshit;
		auto const bext = BulletExt::ExtMap.Find(bullet);
		phoboshit["CurrentStrength"] = bext->CurrentStrength;
		if (bext->FirerHouse)
			phoboshit["FirerHouse"] = bext->FirerHouse->PlainName;
		jBul["Phobos"] = std::move(phoboshit);

		return jBul;
	}

	static json FromTechno(TechnoClass const* techno)
	{
		json jTech = FromObject(techno);
		jTech["Type"] = techno->get_ID();
		// MissionClass
		jTech["CurrentMission"] = MissionControlClass::FindName(techno->CurrentMission);
		jTech["SuspendedMission"] = MissionControlClass::FindName(techno->SuspendedMission);
		jTech["QueuedMission"] = MissionControlClass::FindName(techno->QueuedMission);
		jTech["MissionStatus"] = techno->MissionStatus;
		jTech["UpdateTimerTimeLeft"] = techno->UpdateTimer.GetTimeLeft();
		jTech["CurrentMissionStartTime"] = techno->CurrentMissionStartTime;

		// RadioClass

		// TechnoClass
		jTech["ArmorMultiplier"] = techno->ArmorMultiplier;
		jTech["FirepowerMultiplier"] = techno->FirepowerMultiplier;
		jTech["CloakState"] = (int)techno->CloakState;
		jTech["Ammo"] = techno->Ammo;
		jTech["ROFTimeLeft"] = techno->DiskLaserTimer.GetTimeLeft();

		jTech["AngleRotatedForwards"] = techno->AngleRotatedForwards;
		jTech["AngleRotatedSideways"] = techno->AngleRotatedSideways;
		jTech["RockingSidewaysPerFrame"] = techno->RockingSidewaysPerFrame;
		jTech["RockingSidewaysPerFrame"] = techno->RockingSidewaysPerFrame;
		jTech["PrimaryFacing.Current"] = techno->PrimaryFacing.Current().GetFacing<32>();
		jTech["PrimaryFacing.Desired"] = techno->PrimaryFacing.Desired().GetFacing<32>();
		jTech["SecondaryFacing.Current"] = techno->SecondaryFacing.Current().GetFacing<32>();
		jTech["SecondaryFacing.Desired"] = techno->SecondaryFacing.Desired().GetFacing<32>();
		// BuildingClass
		if (auto bld = specific_cast<BuildingClass const*>(techno))
		{
			jTech["BState"] = bld->BState;
			jTech["HasPower"] = bld->HasPower;
			jTech["IsReadyToCommence"] = bld->IsReadyToCommence;
			jTech["IsBeingRepaired"] = bld->IsBeingRepaired;
			jTech["ActuallyPlacedOnMap"] = bld->ActuallyPlacedOnMap;
		}

		// FootClass
		if (auto foot = generic_cast<FootClass const*>(techno))
		{
			jTech["CurrentMapCoords"] = { foot->CurrentMapCoords.X,foot->CurrentMapCoords.Y };
			if (foot->Destination)
				jTech["Destination"] = foot->Destination->UniqueID;
			jTech["SpeedPercentage"] = foot->SpeedPercentage;
			jTech["SpeedMultiplier"] = foot->SpeedMultiplier;
			if (foot->Team)
				jTech["Team"] = foot->Team->UniqueID;
			jTech["IsDeploying"] = foot->IsDeploying;
			jTech["IsFiring"] = foot->IsFiring;
			jTech["ShouldScanForTarget"] = foot->ShouldScanForTarget;

			jTech["LocomotorVtbl"] = VTable::Get(foot->Locomotor.GetInterfacePtr());

			//...
		}
		// Infantry
		if (auto inf = specific_cast<InfantryClass const*>(techno))
		{
			jTech["SequenceAnim"] = (int)inf->SequenceAnim;
			jTech["Crawling"] = inf->Crawling;
			jTech["ShouldDeploy"] = inf->ShouldDeploy;
			//...
		}
		// Unit
		else if (auto unit = specific_cast<UnitClass const*>(techno))
		{
			jTech["Unloading"] = unit->Unloading;
			//...
		}
		// Aircraft
		else if (auto airc = specific_cast<AircraftClass const*>(techno))
		{
			jTech["HasPassengers"] = airc->HasPassengers;
			if (airc->DockNowHeadingTo)
				jTech["DockNowHeading"] = airc->DockNowHeadingTo->UniqueID;
		}

		return jTech;
	}


	static json FromHouse(HouseClass const* house)
	{
		json jHouse = FromAbstract(house);

		jHouse["PlainName"] = house->PlainName;
		jHouse["Type"] = house->Type->ID;
		jHouse["IsHumanPlayer"] = house->IsHumanPlayer;
		jHouse["Production"] = house->Production;
		jHouse["OwnedInfanty"] = house->OwnedInfantry;
		jHouse["OwnedUnit"] = house->OwnedUnits;
		jHouse["OwnedAircraft"] = house->OwnedAircraft;
		jHouse["OwnedBuilding"] = house->OwnedBuildings;
		jHouse["Balance"] = house->Balance;
		jHouse["NumAirpads"] = house->NumAirpads;
		jHouse["NumBarracks"] = house->NumBarracks;
		jHouse["NumConYards"] = house->NumConYards;
		jHouse["NumOrePurifiers"] = house->NumOrePurifiers;
		jHouse["NumShipyards"] = house->NumShipyards;
		jHouse["NumWarFactories"] = house->NumWarFactories;
		jHouse["PowerOutput"] = house->PowerOutput;
		jHouse["PowerDrain"] = house->PowerDrain;

		return jHouse;
	}

	static json FromTeam(TeamClass const* team)
	{
		json jTeam = FromAbstract(team);

		jTeam["Type"] = team->Type->ID;
		jTeam["Owner"] = team->Owner->PlainName;

		return jTeam;
	}

	void WriteAnims()
	{
		json jAnims = json::array();
		for (auto const* anim : *AnimClass::Array)
			jAnims.push_back(FromAnim(anim));
		MainFile["Animations"] = std::move(jAnims);
	}

	void WriteRadSites()
	{
		json jRads = json::array();
		for (auto const* rad : *RadSiteClass::Array)
			jRads.push_back(FromRad(rad));
		MainFile["RadSites"] = std::move(jRads);
	}

	void WriteBullets()
	{
		json jBullets = json::array();
		for (auto const* rad : *BulletClass::Array)
			jBullets.push_back(FromBullet(rad));
		MainFile["Bullets"] = std::move(jBullets);
	}

	void WriteTechnos()
	{
		json jTechnos = json::array();
		for (auto const* techno : *TechnoClass::Array)
			jTechnos.push_back(FromTechno(techno));
		MainFile["Technos"] = std::move(jTechnos);
	}

	void WriteTeams()
	{
		json jTeam = json::array();
		for (auto const* team : *TeamClass::Array)
			jTeam.push_back(FromTeam(team));
		MainFile["Teams"] = std::move(jTeam);
	}

	void WriteHouses()
	{
		json jHouse = json::array();
		for (auto const* house : *HouseClass::Array)
			jHouse.push_back(FromHouse(house));
		MainFile["Houses"] = std::move(jHouse);
	}

	void WriteStarkkuAttachEffects()
	{

	}



public:
	void Poop()
	{
		this->WriteMetaInfo();
		this->WriteOffendingEvent();
		this->WriteRNG();

		this->WriteAnims();
		this->WriteBullets();
		this->WriteRadSites();
		this->WriteTechnos();
		this->WriteTeams();
		this->WriteHouses();
	}
};


/*
void __fastcall DesyncLogging_MPDEBUG(int slot, EventClass* pOffendingEvent)
{

	std::ofstream oss { std::format("debug\\SYNC{:01d}_{:03d}.json", HouseClass::CurrentPlayer->ArrayIndex, slot) };
	if (oss.is_open())
	{
		DesyncLogger logger { slot, pOffendingEvent };
		logger.Poop();
		oss << logger.MainFile.dump(4) << std::endl;
	}
	oss.close();

	reinterpret_cast<decltype(DesyncLogging_MPDEBUG)*>(0x6516F0)(slot, pOffendingEvent);
}

DEFINE_JUMP(CALL, 0x64735F, GET_OFFSET(DesyncLogging_MPDEBUG));
DEFINE_JUMP(CALL, 0x64CC98, GET_OFFSET(DesyncLogging_MPDEBUG));
*/

void __fastcall DesyncLogging_Normal(EventClass* pOffendingEvent)
{
	Debug::Log("Ares dumping its SYNC.TXT...\n");
	//Ares SYNC.TXT
	reinterpret_cast<decltype(DesyncLogging_Normal)*>(0x64DEA0)(pOffendingEvent);
	// My json
	Debug::Log("I am dumping a json format of it...\n");
	std::ofstream oss { std::format("debug\\SYNC{:01d}.json", HouseClass::CurrentPlayer->ArrayIndex) };
	if (oss.is_open())
	{
		DesyncLogger logger { pOffendingEvent };
		logger.Poop();
		oss << logger.MainFile.dump(4) << std::endl;
	}
	oss.close();
}

DEFINE_JUMP(CALL, 0x647368, GET_OFFSET(DesyncLogging_Normal));
DEFINE_JUMP(CALL, 0x64CCBA, GET_OFFSET(DesyncLogging_Normal));
#endif
