// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataTags.h"
#include "GameFramework/Actor.h"
#include "InteractibleObjects.generated.h"

class UAnimMontage;

USTRUCT()
struct FInteractibleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data")
		///
		FVector interactionPoint = FVector();

	UPROPERTY(EditAnywhere, Category = "Data")
		/// Alternative method for indicating interaction location
		TWeakObjectPtr<AActor> interactionObject = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
		///
		TArray<TSoftObjectPtr<UAnimMontage>> interactionAnim;

	UPROPERTY(EditAnywhere, Instanced, NotReplicated, Category = "Data")
		/// Not replicated: this is an inline template, abilities are given on the server
		TObjectPtr<class UGA_PurposeBase> interactionAbility = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
		float numberOfInteractions = 0;

	UPROPERTY(EditAnywhere, Category = "Data")
		float timeOfInteractionAfterStartInMinutes = 0;

	UPROPERTY(EditAnywhere, Category = "Data")
		float interactionIntervalMax = 0;

	float InteractionLimitInSeconds() const { return timeOfInteractionAfterStartInMinutes * 60; }
};

USTRUCT(BlueprintType, meta = (DisplayName = "Interactible Objects"))
/// <summary>
/// A Datachunk used for Activities to store interactible points and relevant data to interaction (Data.InteractibleObjects)
/// </summary>
struct AO_API FInteractibleObjectsChunk : public FDataChunk
{
	GENERATED_BODY()

	FInteractibleObjectsChunk() : FDataChunk(TAG_Data_InteractibleObjects) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return interactions.Num() > 0;
	}

	UPROPERTY(EditAnywhere, Category = "Data")
	/// Allows seeking of interaction actors by tag
	TArray<FName> interactionTags;

	UPROPERTY(EditAnywhere, Category = "Data")
	///
	TArray<FInteractibleEntry> interactions;

	const TArray<FInteractibleEntry>& Value() const
	{
		return interactions;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + FString::Printf(TEXT("Interactions: %d"), interactions.Num());
	}
};
