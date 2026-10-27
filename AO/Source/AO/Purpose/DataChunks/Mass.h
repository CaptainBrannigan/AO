// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "FloatChunk.h"
#include "Mass.generated.h"

UCLASS( MinimalAPI)
/// <summary>
/// Represents the weight of the object it belongs to
/// </summary>
class UMass : public UFloatChunk
{
	GENERATED_BODY()
public:

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
