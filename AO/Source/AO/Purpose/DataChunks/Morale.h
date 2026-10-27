// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "FloatChunk.h"
#include "Morale.generated.h"

UCLASS(MinimalAPI)
/// <summary>
/// Effectively a health indicator
/// </summary>
class UMorale : public UFloatChunk
{
	GENERATED_BODY()
public:

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }

	/// While it can not be overloaded, this method will provide datachunks the opportunity to access their outer object and potentially initialize themselves from there
	UDataChunk& NonOverloadInitialize() final
	{
		value = 100.0f;

		return *this;
	}
};
