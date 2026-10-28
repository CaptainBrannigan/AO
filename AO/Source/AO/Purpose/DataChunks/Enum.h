#pragma once

#include "Data/Data.h"
#include "Data/GlobalLog.h"
#include "Enum.generated.h"

USTRUCT(BlueprintType, meta = (Hidden))
/// <summary>
/// Base type for enum data, so one condition (Condition_Enum) can compare any enum chunk
/// </summary>
struct AO_API FEnumChunk : public FDataChunk
{
	GENERATED_BODY()

	FEnumChunk() {}
	explicit FEnumChunk(FGameplayTag inKey) : FDataChunk(inKey) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override { return GetEnumValue() != 0; }/// Every enum in the system reserves 0 for None

	virtual uint8 GetEnumValue() const { return 0; }

	/// So conditions can show and compare by name
	virtual const UEnum* GetEnumType() const { return nullptr; }

	FString Description() const override
	{
		const UEnum* enumType = GetEnumType();
		return FDataChunk::Description() + TEXT("Value: ") + (enumType ? enumType->GetNameStringByValue(GetEnumValue()) : FString::FromInt(GetEnumValue()));
	}
};
