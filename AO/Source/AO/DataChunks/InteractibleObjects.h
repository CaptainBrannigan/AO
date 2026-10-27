// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "GameFramework/Actor.h"
#include "InteractibleObjects.generated.h"

USTRUCT()
struct FInteractibleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
		/// 
		FVector interactionPoint = FVector();

	UPROPERTY(EditAnywhere)
		/// Alternative method for indicating interaction location
		TWeakObjectPtr<AActor> interactionObject = nullptr;

	UPROPERTY(EditAnywhere)
		/// 
		TArray<TSoftObjectPtr<UAnimMontage>> interactionAnim;
		
	UPROPERTY(EditAnywhere, Instanced)
		/// 
		TObjectPtr<class UGA_PurposeBase> interactionAbility = nullptr;

	UPROPERTY(EditAnywhere)
		float numberOfInteractions = 0;
		
	UPROPERTY(EditAnywhere)
		float timeOfInteractionAfterStartInMinutes = 0;
		
	UPROPERTY(EditAnywhere)
		float interactionIntervalMax = 0;

	float InteractionLimitInSeconds() const { return timeOfInteractionAfterStartInMinutes * 60; }
};

UCLASS(MinimalAPI)
/// <summary>
/// A Datachunk used for Activities to store interactible points and relevant data to interaction
/// </summary>
class UInteractibleObjects : public UDataChunk
{
	GENERATED_BODY()
public:

	UInteractibleObjects() {}

	///Overload this to require new object arguments
	UInteractibleObjects& Initialize()
	{
		return *this;
	}	

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return interactions.Num() > 0;
	}

	UPROPERTY(EditAnywhere)
	/// Allows seeking of interaction actors by tag
	TArray<FName> interactionTags;
	
	UPROPERTY(EditAnywhere)
	/// 
	TArray<FInteractibleEntry> interactions;

	const TArray<FInteractibleEntry> Value() const
	{
		return interactions;
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Interactions: %d"), interactions.Num());
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
