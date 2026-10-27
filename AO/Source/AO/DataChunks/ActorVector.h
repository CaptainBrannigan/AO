// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/DataChunks/Vector.h"
#include "Purpose/PurposeAbilityComponent.h"
#include "ActorVector.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class AO_API UActorVector : public UVectorChunk
{
	GENERATED_BODY()
public:

	/// While it can not be overloaded, this method will provide datachunks the opportunity to access their outer object and potentially initialize themselves from there
	UDataChunk& NonOverloadInitialize() final
	{
		if (GetOuter() && GetOuter()->IsA<UPurposeAbilityComponent>())
		{
			Global::Log(DATATRIVIAL, LogData, "UActorReference", "NonOverloadInitialize", TEXT("Was able to initialize actor with avatar actor of outer."));
			actorForLocation = Cast<UPurposeAbilityComponent>(GetOuter())->GetAvatarActor();
		}
		return *this;
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() override { return IsValid(actorForLocation) || Super::HasValue(); }

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const FVector Value() const override { return actorForLocation ? actorForLocation->GetActorLocation() : Super::Value(); }
	TObjectPtr<AActor> ActorValue() const { return actorForLocation; }

	void SetValueWithActor(TObjectPtr<AActor> inActorForLocation) 
	{ 
		actorForLocation = inActorForLocation; 
		if (actorForLocation) { location = actorForLocation->GetActorLocation(); }
	}

	void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) override;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> actorForLocation;
};
