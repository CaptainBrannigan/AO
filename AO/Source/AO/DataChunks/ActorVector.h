// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/DataChunks/Vector.h"
#include "GameFramework/Actor.h"
#include "ActorVector.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Vector"))
/// <summary>
/// A vector read from an actor when one is set, otherwise the stored vector
/// On the game thread GetValue reads the actor live. On a snapshot (evaluation thread) it returns the value copied by ResolveForSnapshot
/// Derived chunks change what is read from the actor by overriding ReadFromActor (location, velocity, forward vector...)
/// </summary>
struct AO_API FActorVectorChunk : public FVectorChunk
{
	GENERATED_BODY()

	FActorVectorChunk() {}
	explicit FActorVectorChunk(FGameplayTag inKey) : FVectorChunk(inKey) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override { return HasActor() || FVectorChunk::HasValue(); }

	FVector GetValue() const override
	{
		if (!bResolved && IsInGameThread())
		{
			if (const AActor* liveActor = actor.Get()) { return ReadFromActor(*liveActor); }
		}
		return value;
	}

	void ResolveForSnapshot() override
	{
		bResolved = true;
		bResolvedActorValid = false;
		if (const AActor* liveActor = actor.Get())
		{
			bResolvedActorValid = true;
			value = ReadFromActor(*liveActor);
		}
	}

	void InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem) override
	{
		if (bBindToOwner && inAvatar) { actor = inAvatar; }
	}

	/// Game thread only
	AActor* GetActor() const
	{
		ensureMsgf(IsInGameThread(), TEXT("FActorVectorChunk::GetActor called off the game thread!"));
		return actor.Get();
	}

	/// Whether an actor is set, safe on any thread for a snapshot
	bool HasActor() const { return bResolved ? bResolvedActorValid : actor.IsValid(); }

	void SetValueWithActor(AActor* inActor)
	{
		actor = inActor;
		if (inActor) { value = ReadFromActor(*inActor); }
	}

	/// Accepts actor references (takes the actor), other actor vectors (takes actor and value) and plain vectors (takes the value, clears the actor)
	bool CopyValueFrom(const FInstancedStruct& source) override;

	FString Description() const override
	{
		return FVectorChunk::Description() + (HasActor() ? TEXT(" (from actor)") : TEXT(""));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TWeakObjectPtr<AActor> actor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
	/// If set, the actor is filled in with the owner of the DataMap when it is initialized (replaces NonOverloadInitialize)
	bool bBindToOwner = false;

protected:

	/// Game thread only. What this chunk reads from its actor
	virtual FVector ReadFromActor(const AActor& inActor) const { return inActor.GetActorLocation(); }

	UPROPERTY(Transient, NotReplicated)
	bool bResolved = false;

	UPROPERTY(Transient, NotReplicated)
	bool bResolvedActorValid = false;
};
