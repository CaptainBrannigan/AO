// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/Enum.h"
#include "Data/DataTags.h"
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

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Race"))
/// <summary>
/// Actor Race represents the highest level of faction, grouped by a race (Data.Race)
/// </summary>
struct AO_API FRaceChunk : public FEnumChunk
{
	GENERATED_BODY()

	FRaceChunk() : FEnumChunk(TAG_Data_Race) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	EActorRace race = EActorRace::None;

	EActorRace Value() const { return race; }

	uint8 GetEnumValue() const override { return static_cast<uint8>(race); }
	const UEnum* GetEnumType() const override { return StaticEnum<EActorRace>(); }
};
