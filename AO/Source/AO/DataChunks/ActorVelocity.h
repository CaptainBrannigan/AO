// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "ActorVector.h"
#include "Data/DataTags.h"
#include "ActorVelocity.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Velocity"))
/// <summary>
/// The actor's velocity (Data.Velocity), read at snapshot so Condition_Velocity never touches the actor on the evaluation thread
/// </summary>
struct AO_API FActorVelocityChunk : public FActorVectorChunk
{
	GENERATED_BODY()

	FActorVelocityChunk()
		: FActorVectorChunk(TAG_Data_Velocity)
	{
		bBindToOwner = true;
	}

	/// A zero velocity is a valid value, so only the actor matters
	bool HasValue() const override { return HasActor(); }

protected:

	FVector ReadFromActor(const AActor& inActor) const override { return inActor.GetVelocity(); }
};
