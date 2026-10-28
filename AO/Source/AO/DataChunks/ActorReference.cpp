// Copyright Jordan Cain. All Rights Reserved.


#include "ActorReference.h"
#include "ActorVector.h"

bool FActorReferenceChunk::CopyValueFrom(const FInstancedStruct& source)
{
	if (const FActorReferenceChunk* referenceSource = source.GetPtr<FActorReferenceChunk>())
	{
		actor = referenceSource->actor;
		return true;
	}

	if (const FActorVectorChunk* actorVectorSource = source.GetPtr<FActorVectorChunk>())
	{
		actor = actorVectorSource->actor;
		return true;
	}

	Global::LogError(LogData, "FActorReferenceChunk", "CopyValueFrom", TEXT("Incoming chunk %s has no actor!"), source.GetScriptStruct() ? *source.GetScriptStruct()->GetName() : TEXT("Invalid"));
	return false;
}
