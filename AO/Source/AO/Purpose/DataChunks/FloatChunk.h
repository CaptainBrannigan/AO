// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Data/GlobalLog.h"
#include "FloatChunk.generated.h"

UENUM(BlueprintType)
enum class EDataModifier : uint8
{
	None/// Default to ensure a value is actually selected or not
	, Increase
	, Decrease
	, SetValue
};

UCLASS(Abstract)
/// <summary>
/// A base type of data chunk for float, with base structure for conditions
/// This allows specific uses for float values to be represented as different classes
/// </summary>
class AO_API UFloatChunk : public UDataChunk
{
	GENERATED_BODY()
public:

	UFloatChunk() {}

	///Overload this to require new object arguments
	UFloatChunk& Initialize()
	{
		return *this;
	}	
	
	UFloatChunk& Initialize(float inValue)
	{
		value = inValue;
		return *this;
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return true;/// Value can never be a nullptr, so always return true to allow evaluation
	}

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCanEditValueInEditor"))
	///
	float value = 0;

	UPROPERTY()
	bool bCanEditValueInEditor = true;

	const float Value() const 
	{
		return value;
	}

	///@return float; value of chunk updated with inValue
	const float AddToValue(float inValue) 
	{
		return value += inValue;
	}

	virtual void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) override
	{
		if (ChunkToCopyFrom && ChunkToCopyFrom->IsA<UFloatChunk>())
		{
			Global::Log(DATATRIVIAL, BEHAVIOR, GetName(), "CopyDataFrom", TEXT("Copying value %f from %s!"), Cast<UFloatChunk>(ChunkToCopyFrom)->value, *ChunkToCopyFrom->GetName());
			value = Cast<UFloatChunk>(ChunkToCopyFrom)->value;
		}
		else
		{
			Global::Log(DATATRIVIAL, PURPOSE, "FloatChunk", "CopyDataFrom", TEXT("Chunk %s is either invalid or not a float chunk!"), ChunkToCopyFrom ? *ChunkToCopyFrom->GetName() : TEXT(""));
		}
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Value: %f"), value);
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }

#pragma region DataAdjustment
public:

	/// Override this method so that datachunks specified in a purpose asset may be modified when purpose is selected
	TObjectPtr<UDataChunk> AdjustData(const UDataChunk& inModifier) final
	{
		if (!inModifier.IsA<UFloatChunk>())
		{
			Global::Log(DATADEBUG, LogData, "UFloatChunk", "AdjustData", TEXT("Provided a non-FloatChunk modifier chunk."));
			return this;
		}

		EDataModifier modifier = Cast<UFloatChunk>(&inModifier)->dataModifier;
		float modValue = Cast<UFloatChunk>(&inModifier)->value;
		switch (modifier)
		{
			case EDataModifier::Increase: value += modValue; break;
			case EDataModifier::Decrease: value -= modValue; break;
			case EDataModifier::SetValue: value = modValue; break;
			default:
				break;
		}
		Global::Log(DATATRIVIAL, PURPOSE, *this, "AdjustData", TEXT("Adjusted by %s. Value: %f."), *Global::EnumValueOnly<EDataModifier>(modifier), Value());

		return this;
	}

	/// Override in subclasses so they may return the correct enum representing modification logic
	const uint8 DataModifier() override { return static_cast<uint8>(dataModifier); }

protected:

	UPROPERTY(EditAnywhere)
		/// Set on a purpose asset in the DataAdjustment array
		/// Not otherwise relevant
		EDataModifier dataModifier = EDataModifier::None;

#pragma endregion
};
