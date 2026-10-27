// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/GlobalLog.h"
#include "ActorArchetype.generated.h"

UENUM(BlueprintType)//"BlueprintType" is essential to include
//
enum class EActorArchetype : uint8
{
	None UMETA(DisplayName = "None")///Default to ensure data is set with HasData()
	, Hero
	, Minion
};

UCLASS()
/// <summary>
/// Actor Archetype represents an important distinction between levels of autonomy
/// Hero archetypes are both players and autonomous AI
/// Minion represents a group of lesser AI acting as one vessel
/// </summary>
class UActorArchetype : public UDataChunk
{
	GENERATED_BODY()
public:

	UActorArchetype() {}

	///Overload this to require new object arguments
	UActorArchetype& Initialize(EActorArchetype inArchetype) 
	{ 
		archetype = inArchetype;
		return *this; 
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return archetype != EActorArchetype::None;
	}

	UPROPERTY(EditAnywhere)
	EActorArchetype archetype = EActorArchetype::None;

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const EActorArchetype Value() const { return archetype; }

	const FString ValueAsString() const
	{
		return Global::EnumValueOnly<EActorArchetype>(archetype);
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Archetype: %s"), *ValueAsString());
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
