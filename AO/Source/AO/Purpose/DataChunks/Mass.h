// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "FloatChunk.h"
#include "Data/DataTags.h"
#include "Mass.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Mass"))
/// <summary>
/// Represents the weight of the object it belongs to
/// A named float: selecting this type fills in its key (Data.Mass)
/// </summary>
///Design: GAS Attributes Mass; Per-actor stat, candidate for a GameplayAttribute exposed through FAttributeChunk
struct AO_API FMassChunk : public FFloatChunk
{
	GENERATED_BODY()

	FMassChunk() : FFloatChunk(TAG_Data_Mass) {}
};
