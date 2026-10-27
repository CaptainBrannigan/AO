// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Vector.h"
#include "GameFramework/Actor.h"
#include "Purpose/PurposeAbilityComponent.h"
#include "ActorLocation.generated.h"

UCLASS()
/// <summary>
/// The actor's world location
/// </summary>
class AO_API UActorLocation : public UVectorChunk
{
	GENERATED_BODY()
public:

	UActorLocation() {}

	/// While it can not be overloaded, this method will provide datachunks the opportunity to access their outer object and potentially initialize themselves from there
	UDataChunk& NonOverloadInitialize() final
	{
		if (GetOuter() && GetOuter()->IsA<UPurposeAbilityComponent>())
		{
			Global::Log(DATATRIVIAL, LogData, "UActorReference", "NonOverloadInitialize", TEXT("Was able to initialize actor with avatar actor of outer."));
			actor = Cast<UPurposeAbilityComponent>(GetOuter())->GetAvatarActor();
		}
		return *this;
	}

	///Overload this to require new object arguments
	UActorLocation& Initialize(TObjectPtr<AActor> inActor)
	{
		actor = inActor;
		return *this; 
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return IsValid(actor);
	}

	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> actor = nullptr;

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const FVector Value() const final { return IsValid(actor) ? actor->GetActorLocation() : FVector(0,0,0); }

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + TEXT("Actor Location: ") + (IsValid(actor) ? actor->GetActorLocation().ToString() : "Actor Invalid");
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
