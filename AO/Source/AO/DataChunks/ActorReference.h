// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "GameFramework/Actor.h"
#include "Data/GlobalLog.h"
#include "ActorReference.generated.h"

UCLASS(MinimalAPI)
/// <summary>
/// Direct reference to a vessel actor
/// </summary>
class UActorReference : public UDataChunk
{
	GENERATED_BODY()
public:

	UActorReference() {}

	/// While it can not be overloaded, this method will provide datachunks the opportunity to access their outer object and potentially initialize themselves from there
	UDataChunk& NonOverloadInitialize() final;

	///Overload this to require new object arguments
	UActorReference& Initialize()
	{
		return *this;
	}	
	
	UActorReference& Initialize(TObjectPtr<AActor> inActor)
	{
		actor = inActor;
		return *this;
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return IsValid(actor);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	/// If valid, will return actor->GetActorLocation.MakeRelative(actorRelativeLocation)
	TObjectPtr<AActor> actor = nullptr;

	const TObjectPtr<AActor> Value() const
	{
		return actor;
	}

	void SetValue(TObjectPtr<AActor> inActor)
	{
		actor = inActor;
	}

	void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) override
	{
		if (!ChunkToCopyFrom)
		{
			Global::LogError(LogData, "UVectorChunk", "CopyDataFrom", TEXT("Incoming chunk invalid!"));
			return;
		}

		if (!ChunkToCopyFrom->IsA<UActorReference>())
		{
			Global::LogError(LogData, "UVectorChunk", "CopyDataFrom", TEXT("Incoming chunk not a vector chunk!"));
			return;
		}

		SetValue(Cast<UActorReference>(ChunkToCopyFrom)->Value());
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + TEXT("Actor: ") + (IsValid(actor) ? *actor->GetName() : TEXT("Invalid"));
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
