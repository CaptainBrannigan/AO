// Copyright Jordan Cain, All Rights Reserved.


#include "ActorVector.h"
#include "ActorReference.h"

bool FActorVectorChunk::CopyValueFrom(const FInstancedStruct& source)
{
	if (const FActorReferenceChunk* referenceSource = source.GetPtr<FActorReferenceChunk>())
	{
		Global::Log(DATATRIVIAL, LogData, "FActorVectorChunk", "CopyValueFrom", TEXT("Copying actor reference %s."), *referenceSource->Key.ToString());
		SetValueWithActor(referenceSource->actor.Get());
		return true;
	}

	if (const FActorVectorChunk* actorVectorSource = source.GetPtr<FActorVectorChunk>())
	{
		Global::Log(DATATRIVIAL, LogData, "FActorVectorChunk", "CopyValueFrom", TEXT("Copying actor vector %s."), *actorVectorSource->Key.ToString());
		value = actorVectorSource->GetValue();
		actor = actorVectorSource->actor;
		return true;
	}

	if (FVectorChunk::CopyValueFrom(source))/// If the chunk being copied is just a vector chunk
	{
		actor = nullptr;
		return true;
	}

	Global::LogError(LogData, "FActorVectorChunk", "CopyValueFrom", TEXT("Incoming chunk %s is not an actor or vector chunk!"), source.GetScriptStruct() ? *source.GetScriptStruct()->GetName() : TEXT("Invalid"));
	return false;
}
