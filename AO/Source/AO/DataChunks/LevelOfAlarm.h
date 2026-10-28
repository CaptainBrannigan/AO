// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/FloatChunk.h"
#include "Data/DataTags.h"
#include "LevelOfAlarm.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Level Of Alarm"))
/// <summary>
/// Indicates a Candidate's level of alarm, where the lowest (0) means no perception of danger, and the highest meaning danger close and imminent
/// A named float: selecting this type fills in its key (Data.LevelOfAlarm)
/// </summary>
///Design: GAS Attributes Alarm Perception; Alarm rising from perception and decaying over time maps to an attribute + periodic GameplayEffect
/// A tag threshold (State.Alarmed at > X) can then be granted by the effect, which conditions read through the subject's tags
struct AO_API FLevelOfAlarmChunk : public FFloatChunk
{
	GENERATED_BODY()

	FLevelOfAlarmChunk() : FFloatChunk(TAG_Data_LevelOfAlarm) {}
};
