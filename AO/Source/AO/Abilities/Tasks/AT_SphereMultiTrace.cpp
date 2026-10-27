// Copyright Jordan Cain. All Rights Reserved.


#include "AT_SphereMultiTrace.h"
#include "Data/GlobalLog.h"

FString UAT_SphereMultiTrace::GetDebugString() const
{
    return Super::GetDebugString();
}

UAT_SphereMultiTrace* UAT_SphereMultiTrace::QueryForHitResults(TWeakObjectPtr<UGameplayAbility> OwningAbility, TWeakObjectPtr<UObject> WorldContextObject, FVector Start, FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool SortAscending, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	UAT_SphereMultiTrace* newTask = NewAbilityTask<UAT_SphereMultiTrace>(OwningAbility.Get());

	newTask->contextObj = WorldContextObject;
	newTask->startPos = Start;
	newTask->endPos = End;
	newTask->sphereRadius = Radius;
	newTask->traceChannel = TraceChannel;
	newTask->traceComplex = bTraceComplex;
	newTask->actorsToIgnore = ActorsToIgnore;
	newTask->drawDebugType = DrawDebugType;
	newTask->sortAscending = SortAscending;
	newTask->ignoreSelf = bIgnoreSelf;
	newTask->traceColor = TraceColor;
	newTask->traceHitColor = TraceHitColor;
	newTask->drawDebugTime = DrawTime;
	
	return newTask;
}

void UAT_SphereMultiTrace::Activate()
{
    Super::Activate();

	if (!contextObj.IsValid())
	{
		Global::LogError(LogAbility, "UAT_SphereMultiTrace", "Activate", TEXT("contextObject is invalid!"));
		EndTask();
		return;
	}

	TArray<FHitResult> OutResults;

	UKismetSystemLibrary::SphereTraceMulti
	(
		contextObj.Get()
		, startPos
		, endPos
		, sphereRadius
		, traceChannel
		, traceComplex
		, actorsToIgnore
		, drawDebugType
		, OutResults
		, ignoreSelf
	);

	OutResults.Sort
	(
		[this](const FHitResult& A, const FHitResult& B)
		{
			return sortAscending
				? FVector::Dist(A.Location, startPos) > FVector::Dist(B.Location, startPos)
				: FVector::Dist(A.Location, startPos) < FVector::Dist(B.Location, startPos);
		}
	);
}

void UAT_SphereMultiTrace::OnDestroy(bool AbilityIsEnding)
{
    Super::BeginDestroy();
}
