#pragma once

#include "Data/Data.h"
#include "Enum.generated.h"

UCLASS(Abstract)
/// <summary>
/// Base class for FVector type data
/// </summary>
class AO_API UEnumChunk : public UDataChunk
{
	GENERATED_BODY()
public:

	UEnumChunk() {}

	///Overload this to require new object arguments
	UEnumChunk& Initialize() { return *this; }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return true;
	}

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	virtual const uint8 Value() const PURE_VIRTUAL(UEnumChunk::Value, return uint8(););

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	virtual FString Description() const
	{
		return TEXT("Type: ") + GetClass()->GetName() + TEXT(" ");
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
