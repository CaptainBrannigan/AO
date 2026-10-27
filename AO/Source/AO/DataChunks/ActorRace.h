// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/GlobalLog.h"
#include "ActorRace.generated.h"

UENUM(BlueprintType)//"BlueprintType" is essential to include
//
enum class EActorRace : uint8
{
	None UMETA(DisplayName = "None")///Default to ensure data is set with HasData()
	, Dwarf
	, Human
	, Wealdman
	, Plaeg /// Design: Rename orc race; Mutated, Plagued, Misshapen, Monster, Deformed, Malformed
	/// Orks can be Gobs and Goblins can be Gibs or Giblets
};

UCLASS()
/// <summary>
/// Actor Race represents the highest level of faction, grouped by a race
/// </summary>
class UActorRace : public UDataChunk
{
	GENERATED_BODY()
public:

	UActorRace() {}

	///Overload this to require new object arguments
	UActorRace& Initialize(EActorRace inRace) 
	{ 
		race = inRace;
		return *this; 
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return race != EActorRace::None;
	}

	UPROPERTY(EditAnywhere)
	EActorRace race = EActorRace::None;

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const EActorRace Value() const { return race; }

	const FString ValueAsString() const
	{
		return Global::EnumValueOnly<EActorRace>(race);
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Race: %s"), *ValueAsString());
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
