// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/PurposeEvaluationThread.h"
#include "EventAsset.generated.h"

UCLASS()
/// The Event purpose is the highest layer, establishing conditions to represent specific happenings
/// EventAssets are made up of GoalAssets in order to establish differing perspectives against one purpose
class AO_API UEventAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	UEventAsset();

public:
	UPROPERTY(EditAnywhere)
	FPurpose eventPurpose;

	/// Static definitions to establish consistency when seeking assets
	/// Event and Reaction are the only two types an asset manager will need to discover
	/// All other asset types are contained by Event Structure
	static const FPrimaryAssetType EventAssetType;

	static FPrimaryAssetType PrimaryAssetType() { return EventAssetType; }
	//FPrimaryAssetId GetPrimaryAssetId() const final { return FPrimaryAssetId(EventAssetType, EventAssetType); }
	FPrimaryAssetId GetPrimaryAssetId() const final { return FPrimaryAssetId(PrimaryAssetType(), GetFName()); }

};
