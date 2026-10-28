// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/Enum.h"
#include "Data/DataTags.h"
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

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Archetype"))
/// <summary>
/// Actor Archetype represents an important distinction between levels of autonomy (Data.Archetype)
/// Hero archetypes are both players and autonomous AI
/// Minion represents a group of lesser AI acting as one vessel
/// </summary>
struct AO_API FArchetypeChunk : public FEnumChunk
{
	GENERATED_BODY()

	FArchetypeChunk() : FEnumChunk(TAG_Data_Archetype) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	EActorArchetype archetype = EActorArchetype::None;

	EActorArchetype Value() const { return archetype; }

	uint8 GetEnumValue() const override { return static_cast<uint8>(archetype); }
	const UEnum* GetEnumType() const override { return StaticEnum<EActorArchetype>(); }
};
