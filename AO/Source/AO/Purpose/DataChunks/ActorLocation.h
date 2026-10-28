// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "DataChunks/ActorVector.h"
#include "Data/DataTags.h"
#include "ActorLocation.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Location"))
/// <summary>
/// The actor's world location (Data.Location)
/// Binds to the owner of its DataMap by default, so a spawner's or component's location chunk needs no actor set in editor
/// Previously read actor->GetActorLocation() on whichever thread asked, now the evaluation thread reads the location copied at snapshot
/// </summary>
struct AO_API FActorLocationChunk : public FActorVectorChunk
{
	GENERATED_BODY()

	FActorLocationChunk()
		: FActorVectorChunk(TAG_Data_Location)
	{
		bBindToOwner = true;
	}

	explicit FActorLocationChunk(AActor* inActor)
		: FActorLocationChunk()
	{
		SetValueWithActor(inActor);
	}
};
