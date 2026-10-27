#pragma once

#include "Data/Data.h"
#include "Data/GlobalLog.h"
#include "Vector.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
/// <summary>
/// Base class for FVector type data
/// </summary>
class AO_API UVectorChunk : public UDataChunk
{
	GENERATED_BODY()
public:

	UVectorChunk() {}

	///Overload this to require new object arguments
	UVectorChunk& Initialize() { return *this; }

public:

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() override { return !location.ContainsNaN() && location != FVector::ZeroVector; }

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	virtual const FVector Value() const { return location; }

	void SetValue(FVector inValue) { location = inValue; }

	void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) override 
	{
		if (!ChunkToCopyFrom)
		{
			Global::LogError(LogData, "UVectorChunk", "CopyDataFrom", TEXT("Incoming chunk invalid!"));
			return;
		}

		if (!ChunkToCopyFrom->IsA<UVectorChunk>())
		{
			Global::LogError(LogData, "UVectorChunk", "CopyDataFrom", TEXT("Incoming chunk %s not a vector chunk!"), *ChunkToCopyFrom->GetName());
			return;
		}

		SetValue(Cast<UVectorChunk>(ChunkToCopyFrom)->Value());
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	virtual FString Description() const
	{
		return TEXT("Type: ") + GetClass()->GetName() + TEXT(" ");
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }

protected:

	UPROPERTY(VisibleAnywhere)
	FVector location = FVector::ZeroVector;
};
