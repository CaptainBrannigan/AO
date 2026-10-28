#pragma once

#include "Data/Data.h"
#include "Data/GlobalLog.h"
#include "Vector.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Vector"))
/// <summary>
/// Base type for FVector data
/// </summary>
struct AO_API FVectorChunk : public FDataChunk
{
	GENERATED_BODY()

	FVectorChunk() {}
	explicit FVectorChunk(FGameplayTag inKey) : FDataChunk(inKey) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override { return !value.ContainsNaN() && value != FVector::ZeroVector; }

	/// Virtual so derived chunks can compute their value. Conditions call this, never value directly
	/// Overrides must be safe on the evaluation thread
	virtual FVector GetValue() const { return value; }

	void SetValue(FVector inValue) { value = inValue; }

	/// Any vector chunk can be copied into any other, regardless of name
	bool CopyValueFrom(const FInstancedStruct& source) override
	{
		if (const FVectorChunk* vectorSource = source.GetPtr<FVectorChunk>())
		{
			SetValue(vectorSource->GetValue());
			return true;
		}
		return false;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + TEXT("Value: ") + GetValue().ToString();
	}

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FVector value = FVector::ZeroVector;
};
