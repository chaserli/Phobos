#include "Body.h"
#include <Ext/Scenario/Body.h>
#include <Ext/Anim/Body.h>
#include <Helpers/Macro.h>

#include <HouseClass.h>
#include <BuildingClass.h>
#include <OverlayTypeClass.h>
#include <LightSourceClass.h>
#include <RadSiteClass.h>
#include <VocClass.h>
#include <SuperClass.h>

#include <Utilities/Macro.h>

DEFINE_HOOK(0x6DD8B0, TActionClass_Execute, 0x6)
{
	GET(TActionClass*, pThis, ECX);
	GET_STACK(HouseClass*, pHouse, 0x4);
	GET_STACK(ObjectClass*, pObject, 0x8);
	GET_STACK(TriggerClass*, pTrigger, 0xC);
	GET_STACK(CellStruct const*, pLocation, 0x10);

	bool handled;

	R->AL(TActionExt::Execute(pThis, pHouse, pObject, pTrigger, *pLocation, handled));

	return handled ? 0x6DD910 : 0;
}

// TODO: Sometimes Buildup anims plays while the building image is already there in faster gamespeed.
// Bugfix: TAction 125 Build At could neither display the buildups nor be AI-repairable in singleplayer mode
DEFINE_HOOK(0x6E427D, TActionClass_CreateBuildingAt, 0x9)
{
	GET(TActionClass*, pThis, ESI);
	GET(BuildingTypeClass*, pBldType, ECX);
	GET(HouseClass*, pHouse, EDI);
	REF_STACK(CoordStruct, coord, STACK_OFFSET(0x24, -0x18));

	bool bPlayBuildUp = pBldType->LoadBuildup();
	//Param3 can be used for other purposes in the future
	bool bCreated = false;
	if (auto pBld = static_cast<BuildingClass*>(pBldType->CreateObject(pHouse)))
	{
		if (bPlayBuildUp)
		{
			pBld->BeginMode(BStateType::Construction);
			pBld->QueueMission(Mission::Construction, false);
		}
		else
		{
			pBld->BeginMode(BStateType::Idle);
			pBld->QueueMission(Mission::Guard, false);
		}

		if (!pBld->ForceCreate(coord))
		{
			pBld->UnInit();
		}
		else
		{
			if (!bPlayBuildUp)
				pBld->Place(false);

			pBld->IsReadyToCommence = true;

			if (SessionClass::IsCampaign() && !pHouse->IsControlledByHuman())
				pBld->ShouldRebuild = pThis->Param4 > 0;

			bCreated = true;
		}
	}

	R->AL(bCreated);
	return 0x6E42C1;
}

#pragma region RetintFix

namespace RetintTemp
{
	bool UpdateLightSources = false;
}

DEFINE_HOOK(0x53AD00, Scen_RecalcLighting_NoteArgs, 0x5)
{
	GET(int, r, ECX);
	GET(int, g, EDX);
	GET_STACK(int, b, 0x4);
	GET_STACK(bool, tint, 0x8);
	ScenarioExt::Global()->LightingSLFixBuffer.ArgsLastTime = { r,g,b,tint };
	return 0;
}

// Bugfix, #issue 429: Retint map script disables RGB settings on light source
// Author: secsome, Starkku
DEFINE_HOOK_AGAIN(0x6E2F47, TActionClass_Retint_LightSourceFix, 0x3) // Blue
DEFINE_HOOK_AGAIN(0x6E2EF7, TActionClass_Retint_LightSourceFix, 0x3) // Green
DEFINE_HOOK(0x6E2EA7, TActionClass_Retint_LightSourceFix, 0x3) // Red
{
	// Flag the light sources to update, actually do it later and only once to prevent redundancy.
	RetintTemp::UpdateLightSources = true;

	return 0;
}

// Update light sources if they have been flagged to be updated.
DEFINE_HOOK(0x6D4455, Tactical_Render_UpdateLightSources, 0x8)
{
	if (RetintTemp::UpdateLightSources)
	{
		for (auto lSource : *LightSourceClass::Array)
		{
			if (lSource->Activated)
			{
				lSource->Activated = false;
				lSource->Activate();
			}
		}

		RetintTemp::UpdateLightSources = false;
	}

	return 0;
}

DEFINE_HOOK(0x6851AC, ScenarioClass_PostLoad_Lighting, 0x5)
{
	bool sw_inactive = NukeFlash::Status != NukeFlashStatus::FadeIn && !ChronoScreenEffect::Status && !LightningStorm::Active &&
					   (PsyDom::Status == PsychicDominatorStatus::Inactive || PsyDom::Status == PsychicDominatorStatus::Over);

	auto swap_data = []()
	{
		auto &buffer = ScenarioExt::Global()->LightingSLFixBuffer;
		std::swap(buffer.INIAmbientOriginal, ScenarioClass::Instance->AmbientOriginal);
		std::swap(buffer.INIAmbientCurrent, ScenarioClass::Instance->AmbientCurrent);
		std::swap(buffer.INIAmbientTarget, ScenarioClass::Instance->AmbientTarget);
		std::swap(buffer.ININormalLighting, ScenarioClass::Instance->NormalLighting);
	};

	if (sw_inactive) swap_data();
	MapClass::Instance->CellIteratorReset();
	for (auto pCell = MapClass::Instance->CellIteratorNext(); pCell; pCell = MapClass::Instance->CellIteratorNext())
	{
		if (pCell->LightConvert)
			delete pCell->LightConvert;
		pCell->LightConvert = nullptr;
		pCell->InitLightConvert();
	}
	if (sw_inactive) swap_data();

	auto &[r, g, b, t] = ScenarioExt::Global()->LightingSLFixBuffer.ArgsLastTime;
	ScenarioClass::RecalcLighting(r, g, b, t);

	for (auto lSource : *LightSourceClass::Array)
	{
		if (lSource->Activated)
		{
			lSource->Activated = false;
			lSource->Activate();
		}
	}

	HouseClass::CurrentPlayer->RecheckRadar = true;
	return 0x6851B1;
}
#pragma endregion

DEFINE_HOOK(0x6E2368, TActionClass_PlayAnimAt, 0x7)
{
	enum { SkipGameCode = 0x6E236F };

	GET(TActionClass*, pThis, ESI);
	GET(AnimClass*, pAnim, EAX);
	GET_STACK(HouseClass*, pHouse, STACK_OFFSET(0x18, 0x4));

	AnimExt::SetAnimOwnerHouseKind(pAnim, pHouse, nullptr, false, true);
	pAnim->IsInert = !pThis->Param3;

	return SkipGameCode;
}
