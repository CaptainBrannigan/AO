// Copyright Jordan Cain, All Rights Reserved.


#include "ActorVector.h"
#include "ActorReference.h"

void UActorVector::CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom)
{
	if (!ChunkToCopyFrom)
	{
		Global::LogError(LogData, "UActorVector", "CopyDataFrom", TEXT("Incoming chunk invalid!"));
		return;
	}

	Super::CopyDataFrom(ChunkToCopyFrom);/// If the chunk being copied is just a vector chunk
	/// otherwise we can copy from an actor reference chunk
	
	Global::Log(DATATRIVIAL, LogData, "UActorVector", "CopyDataFrom", TEXT("Incoming chunk %s!"), *ChunkToCopyFrom->GetName());

	if (ChunkToCopyFrom->IsA<UActorReference>())
	{
		Global::Log(DATATRIVIAL, LogData, "UActorVector", "CopyDataFrom", TEXT("Copying %s"), Cast<UActorReference>(ChunkToCopyFrom)->Value() ? *Cast<UActorReference>(ChunkToCopyFrom)->Value()->GetName() : TEXT("Invalid Actor!"));
		actorForLocation = Cast<UActorReference>(ChunkToCopyFrom)->Value();
		Global::Log(DATATRIVIAL, LogData, "UActorVector", "CopyDataFrom", TEXT("Actor: %s; Location: %s."), actorForLocation ? *actorForLocation->GetName() : TEXT("Invalid Actor!"), location.ContainsNaN() ? TEXT("Invalid location") : *location.ToString());
		return;
	}

	if (ChunkToCopyFrom->IsA<UActorVector>())
	{
		Global::Log(DATATRIVIAL, LogData, "UActorVector", "CopyDataFrom", TEXT("Copying %s"), Cast<UActorVector>(ChunkToCopyFrom)->actorForLocation ? *Cast<UActorVector>(ChunkToCopyFrom)->actorForLocation->GetName() : TEXT("Invalid Actor!"));
		actorForLocation = Cast<UActorVector>(ChunkToCopyFrom)->actorForLocation;
		Global::Log(DATATRIVIAL, LogData, "UActorVector", "CopyDataFrom", TEXT("Actor: %s; Location: %s."), actorForLocation ? *actorForLocation->GetName() : TEXT("Invalid Actor!"), location.ContainsNaN() ? TEXT("Invalid location") : *location.ToString());
		return;
	}

}
