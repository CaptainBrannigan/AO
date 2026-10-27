// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/FloatChunk.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Data/GlobalLog.h"
#include "LevelOfAlarm.generated.h"


UCLASS(MinimalAPI)
/// <summary>
/// Indicates a Candidate's level of alarm, where the lowest (0) means no perception of danger, and the highest meaning danger close and imminent
/// </summary>
class ULevelOfAlarm : public UFloatChunk
{
	GENERATED_BODY()
public:


};
