// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "Data/DataTags.h"
#include "Misc/TimeSpan.h"
#include "Data/GlobalLog.h"
#include "TimeOf.generated.h"

UENUM(BlueprintType)
enum class ETimeOfModifier : uint8
{
	None/// Default to ensure a value is actually selected or not
	, UpdateTimeToNow
};

USTRUCT(BlueprintType, meta = (DisplayName = "Time"))
/// <summary>
/// A point in time. Defaults to the time the chunk was created
/// </summary>
struct AO_API FTimeChunk : public FDataChunk
{
	GENERATED_BODY()

	FTimeChunk() : value(FDateTime::UtcNow()) {}
	explicit FTimeChunk(FGameplayTag inKey) : FDataChunk(inKey), value(FDateTime::UtcNow()) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return true;/// Value can never be a nullptr, so always return true to allow evaluation
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data")
	FDateTime value;

	FDateTime Value() const
	{
		return value;
	}

	bool CopyValueFrom(const FInstancedStruct& source) override
	{
		if (const FTimeChunk* timeSource = source.GetPtr<FTimeChunk>())
		{
			value = timeSource->value;
			return true;
		}
		return false;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + FString::Printf(TEXT("Value: %s"), *value.ToString());
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Time Of Occurrence"))
/// When an occurrence was raised (Data.Time.Occurrence)
struct AO_API FTimeOfOccurrenceChunk : public FTimeChunk
{
	GENERATED_BODY()

	FTimeOfOccurrenceChunk() : FTimeChunk(TAG_Data_Time_Occurrence) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Time"))
/// Sets a time chunk to now. Creates an FTimeChunk under the key if the subject doesn't have one
struct AO_API FTimeModifier : public FDataModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	ETimeOfModifier dataModifier = ETimeOfModifier::UpdateTimeToNow;

	bool Apply(FDataMap& map, FGameplayTag key) const override
	{
		FTimeChunk* chunk = map.FindOrAdd<FTimeChunk>(key);
		if (!chunk)
		{
			return false;
		}

		switch (dataModifier)
		{
			case ETimeOfModifier::UpdateTimeToNow: chunk->value = FDateTime::UtcNow(); break;
			default:
				return false;
		}
		Global::Log(DATATRIVIAL, PURPOSE, "FTimeModifier", "Apply", TEXT("Adjusted %s by %s. Value: %s."), *key.ToString(), *Global::EnumValueOnly<ETimeOfModifier>(dataModifier), *chunk->Value().ToString());

		return true;
	}

	FString Description() const override { return Global::EnumValueOnly<ETimeOfModifier>(dataModifier); }
};
