// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_SubjectVisibility.h"
#include "DataChunks/ActorViewPoint.h"

UCondition_SubjectVisibility::UCondition_SubjectVisibility()
{
	primaryKey = TAG_Data_ViewPoint;
}

/* TODO: AI Purpose Visibility; Evaluation is commented out until the sightline is gathered on the game thread
Warzone traced here on the evaluation thread. Chaos scene queries read the acceleration structure the game thread is moving actors in, so that's a race
(the crash noted in Warzone was DrawDebugLine, but the trace has the same problem)
The trace result has to be in the snapshot, the open question is how it gets there:
 - Snapshot requirements: conditions declare what they need (visibility between primary/secondary subject on a channel), cached per purpose asset,
   resolved per candidate when the occurrence is raised, written into the candidate's snapshot as a derived chunk that this reads
 - Perception first: if the observer has a UAIPerceptionComponent with sight, its sensed state answers AI -> target with no new traces
 - Fallback: a synchronous game thread trace at snapshot (cheap at occurrence rates), or AsyncLineTraceByChannel with the occurrence queued once results return
*/
float UCondition_SubjectVisibility::Evaluate(const FSubjectDataView& data) const
{
	const FActorViewPointChunk* primary = Primary<FActorViewPointChunk>(data);
	const FActorViewPointChunk* secondary = Secondary<FActorViewPointChunk>(data);
	if (!primary || !secondary || !primary->HasValue() || !secondary->HasValue())
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_SubjectVisibility", "Evaluate", TEXT("Primary||Secondary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	Global::Log(DATADEBUG, LogCondition, "UCondition_SubjectVisibility", "Evaluate", TEXT("Visibility evaluation is not implemented yet, scoring 0. Description: %s"), *description.ToString());
	return 0.0f;

	/// Warzone's evaluation, using the view point chunks. GetActor() and the trace are game thread only, which is why this can't run here as is
	/*
	AActor* primaryValue = primary->GetActor();
	AActor* secondaryValue = secondary->GetActor();
	if (!primaryValue || !secondaryValue) { return 0.0f; }

	// FHitResult will hold all data returned by our line collision query
	FHitResult outHit;
	const FVector traceStart = primary->GetValue();/// Eyes to eyes, rather than actor root to actor root
	const FVector traceEnd = secondary->GetValue();

	/// Here we add both actors to the ignored list so they won't block the trace
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(primaryValue);
	QueryParams.AddIgnoredActor(secondaryValue);
	QueryParams.bIgnoreBlocks = false;/// If the trace is blocked on its way to the actor, then the actor is not visible
	QueryParams.bTraceComplex = false;
	QueryParams.bDebugQuery = false;

	/// UWorld()->LineTraceSingleByChannel runs a line trace and returns the first actor hit over the provided collision channel.
	primaryValue->GetWorld()->LineTraceSingleByChannel(outHit, traceStart, traceEnd, TraceChannelProperty, QueryParams);

	switch (conditional)
	{
		case EConditional_Visibility::IsVisible:
			return !outHit.bBlockingHit;/// As we are ignoring both actors involved, we only need to check if the hit is blocked or not
		case EConditional_Visibility::IsNotVisible:
			return outHit.bBlockingHit;
		default:
			return 0.0f;
	}
	*/
}
