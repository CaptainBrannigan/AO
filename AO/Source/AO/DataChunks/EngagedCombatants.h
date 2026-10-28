// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/FloatChunk.h"
#include "Data/DataTags.h"
#include "EngagedCombatants.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Engaged Combatants"))
/// <summary>
/// A named float: selecting this type fills in its key (Data.EngagedCombatants)
/// </summary>
struct AO_API FEngagedCombatantsChunk : public FFloatChunk
{
	GENERATED_BODY()

	FEngagedCombatantsChunk() : FFloatChunk(TAG_Data_EngagedCombatants) {}
};
