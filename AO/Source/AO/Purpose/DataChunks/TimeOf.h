// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Misc/TimeSpan.h"
#include "Data/GlobalLog.h"
#include "TimeOf.generated.h"

UENUM(BlueprintType)
enum class ETimeOfModifier : uint8
{
	None/// Default to ensure a value is actually selected or not
	, UpdateTimeToNow
};

UCLASS(Abstract)
/// <summary>
/// </summary>
class AO_API UTimeOf : public UDataChunk
{
	GENERATED_BODY()
public:

	UTimeOf() 
	{
		value = FDateTime::UtcNow();
	}


	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return true;/// Value can never be a nullptr, so always return true to allow evaluation
	}

	FDateTime value;

	const FDateTime Value() const
	{
		return value;
	}

	virtual void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) override
	{
		if (ChunkToCopyFrom && ChunkToCopyFrom->IsA<UTimeOf>())
		{
			Global::Log(DATATRIVIAL, BEHAVIOR, GetName(), "CopyDataFrom", TEXT("Copying value %s from %s!"), *Cast<UTimeOf>(ChunkToCopyFrom)->value.ToString(), *ChunkToCopyFrom->GetName());
			value = Cast<UTimeOf>(ChunkToCopyFrom)->value;
		}
		else
		{
			Global::Log(DATATRIVIAL, PURPOSE, "UTimeOf", "CopyDataFrom", TEXT("Chunk %s is either invalid or not a float chunk!"), ChunkToCopyFrom ? *ChunkToCopyFrom->GetName() : TEXT(""));
		}
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Value: %s"), *value.ToString());
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }

#pragma region DataAdjustment
public:

	/// Override this method so that datachunks specified in a purpose asset may be modified when purpose is selected
	TObjectPtr<UDataChunk> AdjustData(const UDataChunk& inModifier) final
	{
		if (!inModifier.IsA<UTimeOf>())
		{
			Global::Log(DATADEBUG, LogData, "UTimeOf", "AdjustData", TEXT("Provided a non-ContextualTags modifier chunk."));
			return this;
		}

		ETimeOfModifier modifier = Cast<UTimeOf>(&inModifier)->dataModifier;
		switch (modifier)
		{
			case ETimeOfModifier::UpdateTimeToNow: value = FDateTime::UtcNow(); break;
			default:
				break;
		}
		Global::Log(DATATRIVIAL, PURPOSE, *this, "AdjustData", TEXT("Adjusted by %s. Value: %s."), *Global::EnumValueOnly<ETimeOfModifier>(modifier), *Value().ToString());

		return this;
	}

	/// Override in subclasses so they may return the correct enum representing modification logic
	const uint8 DataModifier() override { return static_cast<uint8>(dataModifier); }

protected:

	UPROPERTY(EditAnywhere)
		/// Set on a purpose asset in the DataAdjustment array
		/// Not otherwise relevant
		ETimeOfModifier dataModifier = ETimeOfModifier::UpdateTimeToNow;

#pragma endregion
};


UCLASS()
/// <summary>
/// </summary>
class AO_API UTimeOfOccurrence : public UTimeOf
{
	GENERATED_BODY()
};
