#pragma once
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "ProfileData.h"

enum ParamIndex {
 SOURCE_INPUT, PATH, AMOUNT, RADIUS, FEATHER, STREAK, FREQUENCY, ORIGINAL, REVERSE, SEED, PREVIEW,
 END_CAP, SAMPLING, PROFILE, PROFILE_SMOOTH, PROFILE_RESET, PROFILE_FLIP, PROFILE_DELETE,
 SOURCE_MATTE, MATTE_CHANNEL, MATTE_INVERT, COUNT
};
constexpr int PROFILE_UI_WIDTH=300,PROFILE_UI_HEIGHT=184;
inline void* const PROFILE_REFCON=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x43535031));

PF_Err CreateDefaultProfile(PF_InData*,PF_ArbitraryH*);
PF_Err HandleArbitrary(PF_InData*,PF_OutData*,PF_ArbParamsExtra*);
PF_Err HandleProfileEvent(PF_InData*,PF_OutData*,PF_ParamDef*[],PF_EventExtra*);
PF_Err HandleProfileButton(PF_InData*,PF_OutData*,PF_ParamDef*[],PF_UserChangedParamExtra*);
