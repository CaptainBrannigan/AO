// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataTags.h"
#include "GameFramework/Actor.h"
#include "Data/GlobalLog.h"
#include "ActorReference.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Reference"))
/// <summary>
/// Direct reference to a vessel actor
/// The reference is weak: a chunk never keeps an actor alive
///
/// Threading: the actor is only dereferenced on the game thread (GetActor)
/// On a snapshot, validity is baked in by ResolveForSnapshot, so HasValue never touches GUObjectArray on the evaluation thread
/// Comparing two references (IsSameActor) only compares index + serial number, which is safe anywhere
/// </summary>
struct AO_API FActorReferenceChunk : public FDataChunk
{
	GENERATED_BODY()

	FActorReferenceChunk() : FDataChunk(TAG_Data_Actor) {}
	FActorReferenceChunk(FGameplayTag inKey, AActor* inActor) : FDataChunk(inKey), actor(inActor) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TWeakObjectPtr<AActor> actor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
	/// If set, the actor is filled in with the owner of the DataMap when it is initialized (replaces NonOverloadInitialize)
	bool bBindToOwner = false;

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return bResolved ? bResolvedValid : actor.IsValid();
	}

	void ResolveForSnapshot() override
	{
		bResolved = true;
		const AActor* liveActor = actor.Get();
		bResolvedValid = liveActor != nullptr;
		resolvedClass = liveActor ? liveActor->GetClass() : nullptr;
	}

	/// Snapshot only: the actor's class as of the snapshot, nullptr if unresolved or invalid
	/// Classes don't change during play, so comparing or IsChildOf on this is safe on the evaluation thread
	const UClass* ResolvedClass() const { return bResolved ? resolvedClass.Get() : nullptr; }

	void InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem) override
	{
		if (bBindToOwner && inAvatar) { actor = inAvatar; }
	}

	/// Game thread only
	AActor* GetActor() const
	{
		ensureMsgf(IsInGameThread(), TEXT("FActorReferenceChunk::GetActor called off the game thread, use HasValue/IsSameActor on the evaluation thread"));
		return actor.Get();
	}

	void SetValue(AActor* inActor) { actor = inActor; }

	/// Safe on any thread, never dereferences
	bool IsSameActor(const FActorReferenceChunk& other) const { return actor.HasSameIndexAndSerialNumber(other.actor); }

	bool CopyValueFrom(const FInstancedStruct& source) override;

	FString Description() const override
	{
		return FDataChunk::Description() + TEXT("Actor: ") + (IsInGameThread() && actor.IsValid() ? actor->GetName() : FString(bResolved ? (bResolvedValid ? TEXT("Valid") : TEXT("Invalid")) : TEXT("Unresolved")));
	}

protected:

	UPROPERTY(Transient, NotReplicated)
	bool bResolved = false;

	UPROPERTY(Transient, NotReplicated)
	bool bResolvedValid = false;

	UPROPERTY(Transient, NotReplicated)
	TObjectPtr<UClass> resolvedClass = nullptr;
};
