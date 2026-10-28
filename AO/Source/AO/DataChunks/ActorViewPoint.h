// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "ActorVector.h"
#include "Data/DataTags.h"
#include "ActorViewPoint.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Actor View Point"))
/// <summary>
/// The actor's eye location (the vector value) and view direction (Data.ViewPoint), read with GetActorEyesViewPoint
/// Used where the actor's root and forward aren't what matters: facing (Condition_RelativePosition) and sightlines (Condition_SubjectVisibility)
/// Binds to the owner of its DataMap by default, both values are copied at snapshot for the evaluation thread
///
/// VR: APawn's default is BaseEyeHeight + controller rotation, so the player pawn should override GetActorEyesViewPoint to return the HMD camera
/// Otherwise ducking behind cover in real life won't change what this reports
/// </summary>
struct AO_API FActorViewPointChunk : public FActorVectorChunk
{
	GENERATED_BODY()

	FActorViewPointChunk()
		: FActorVectorChunk(TAG_Data_ViewPoint)
	{
		bBindToOwner = true;
	}

	/// The view point only comes from an actor
	bool HasValue() const override { return HasActor(); }

	void ResolveForSnapshot() override
	{
		FActorVectorChunk::ResolveForSnapshot();/// Copies the eye location into value
		direction = FVector::ZeroVector;
		if (const AActor* liveActor = actor.Get()) { direction = ReadDirectionFromActor(*liveActor); }
	}

	/// Unit view direction. Like GetValue, read live on the game thread and copied at snapshot otherwise
	FVector GetDirection() const
	{
		if (!bResolved && IsInGameThread())
		{
			if (const AActor* liveActor = actor.Get()) { return ReadDirectionFromActor(*liveActor); }
		}
		return direction;
	}

	FString Description() const override
	{
		return FActorVectorChunk::Description() + TEXT(" Direction: ") + direction.ToString();
	}

protected:

	FVector ReadFromActor(const AActor& inActor) const override
	{
		FVector eyeLocation;
		FRotator eyeRotation;
		inActor.GetActorEyesViewPoint(eyeLocation, eyeRotation);
		return eyeLocation;
	}

	/// Game thread only
	FVector ReadDirectionFromActor(const AActor& inActor) const
	{
		FVector eyeLocation;
		FRotator eyeRotation;
		inActor.GetActorEyesViewPoint(eyeLocation, eyeRotation);
		return eyeRotation.Vector();
	}

	UPROPERTY(VisibleAnywhere, Transient, NotReplicated, Category = "Data")
	/// Only set on snapshots, the game thread reads the actor live
	FVector direction = FVector::ZeroVector;
};
