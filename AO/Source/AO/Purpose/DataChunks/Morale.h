// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "FloatChunk.h"
#include "Data/DataTags.h"
#include "Morale.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Morale"))
/// <summary>
/// Effectively a health indicator
/// A named float: selecting this type fills in its key (Data.Morale) and a default of 100
/// </summary>
///Design: GAS Attributes Morale Health; Morale is the strongest attribute candidate, it behaves like health
/// As an attribute, damage/suppression GameplayEffects lower it, regen is a periodic effect, and clamping lives in PreAttributeChange
struct AO_API FMoraleChunk : public FFloatChunk
{
	GENERATED_BODY()

	FMoraleChunk() : FFloatChunk(TAG_Data_Morale, 100.0f) {}
};
