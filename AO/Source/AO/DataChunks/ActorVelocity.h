// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/Vector.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Purpose/PurposeAbilityComponent.h"
#include "ActorVelocity.generated.h"

UCLASS(MinimalAPI)
/// <summary>
///
/// </summary>
class UActorVelocity : public UVectorChunk
{
	GENERATED_BODY()
public:

	UActorVelocity() {}

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
	UActorVelocity& Initialize()
	{
		return *this;
	}

	UActorVelocity& Initialize(TObjectPtr<AActor> inActor)
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

	UFUNCTION(BlueprintCallable)
	/// @return FVector: If actor is valid, their current velocity, else a zero vector
	const FVector Value() const final
	{
		if (IsValid(actor))
		{
			return actor->GetVelocity();
		}
		else
		{
			return FVector::ZeroVector;
		}
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + TEXT("Actor Velocity: ") + (IsValid(actor) ? actor->GetVelocity().ToString() : ("Actor Invalid!"));
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
