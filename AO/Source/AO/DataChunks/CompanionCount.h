// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/FloatChunk.h"
#include "Data/DataTags.h"
#include "CompanionCount.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Companion Count"))
/// <summary>
/// A named float: selecting this type fills in its key (Data.CompanionCount)
/// </summary>
struct AO_API FCompanionCountChunk : public FFloatChunk
{
	GENERATED_BODY()

	FCompanionCountChunk() : FFloatChunk(TAG_Data_CompanionCount) {}
};
