// Copyright Jordan Cain. All Rights Reserved.


#include "AT_MoveTo.h"
#include "AISystem.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Purpose/Abilities/Behavior_AI.h"
#include "Purpose/DataChunks/Vector.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Abilities/AI/AI_SequentialActions.h"
#include "NativeGameplayTags.h"
#include "DataAssets/QueryContextAsset.h"
#include "Purpose/DataChunks/ContextualHistory.h"
#include "Purpose/PurposeAbilityComponent.h"

/// UAITask_MoveTo includes
#include "UObject/Package.h"
#include "TimerManager.h"
#include "AISystem.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Moving, "Moving");

UAT_MoveTo::UAT_MoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	dataMap.Set(FFloatChunk(TAG_Data_Task_MoveTo_Speed));
	dataMap.Set(FActorVectorChunk(TAG_Data_Task_MoveTo_Goal));
	dataMap.Set(FFloatChunk(TAG_Data_Task_MoveTo_PathLength));
	dataMap.Set(FFloatChunk(TAG_Data_Task_MoveTo_Duration));

	contextualTagForTask.AddTag(TAG_Moving);
}

UAT_MoveTo* UAT_MoveTo::MoveTo(AAIController* Controller, FVector InGoalLocation, float moveSpeed, AActor* InGoalActor,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath,
	bool bUsePathfinding, bool bLockAILogic, bool bUseContinuousGoalTracking, EAIOptionFlag::Type ProjectGoalOnNavigation)
{
	UAT_MoveTo* MyTask = Controller ? UAITask::NewAITask<UAT_MoveTo>(*Controller, EAITaskPriority::High) : nullptr;
	if (MyTask)
	{
		FAIMoveRequest MoveReq;
		if (InGoalActor)
		{
			MoveReq.SetGoalActor(InGoalActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InGoalLocation);
		}
		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
		MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
		MoveReq.SetUsePathfinding(bUsePathfinding);
		MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(ProjectGoalOnNavigation, MoveReq.IsProjectingGoal()));
		if (Controller && Controller->GetPawn())
		{
			MoveReq.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
			if (UCharacterMovementComponent* moveComp = Controller->GetPawn()->FindComponentByClass<UCharacterMovementComponent>())
			{
				moveComp->MaxWalkSpeed = moveSpeed;
			}
		}

		MyTask->SetUp(Controller, MoveReq);
		MyTask->SetContinuousGoalTracking(bUseContinuousGoalTracking);

		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}
	}

	return MyTask;
}

bool UAT_MoveTo::PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source)
{
	if (!OwnerController)
	{
		Global::LogError(BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("owner controller invalid!"));
		return false;
	}

	FAIMoveRequest MoveReq;
	bool bGoalSet = false;

	//UBlackboardComponent* MyBlackboard = OwnerController->GetBlackboardComponent();
	//if (MyBlackboard)
	//{
	//	FVector location = FVector::ZeroVector;
	//	location = MyBlackboard->GetValueAsVector(keyNameOfRelevantBlackboardData);
	//	if (location != FVector::ZeroVector)
	//	{
	//		MoveReq.SetGoalLocation(location);
	//		bGoalSet = true;
	//	}
	//	else if (AActor* actor = Cast<AActor>(MyBlackboard->GetValueAsObject(keyNameOfRelevantBlackboardData)))
	//	{
	//		MoveReq.SetGoalLocation(actor->GetActorLocation());
	//		bGoalSet = true;
	//	}
	//	bGoalSet = false;
	//}

	/// Refactor: Tasks Data; Perhas this shouldn't be allowed. With the AT system, if we run an EQS, then it should be using the results to set data in the blackboard
		/// To perceive anything we're utilizing EQS. We are avoiding having a background perception tick in favor of a controlled approach
		/// So from somewhere we're telling EQS to run. We should also be telling the EQS how to store the result.
	//else if (entryData.subjectForData != ESubject::None && entryData.dataToUse)
	//{
	//	if (source.IsA<UBehavior_AI>())
	//	{
	//		UDataChunk* chunk = Cast<UBehavior_AI>(&source)->context.DataChunk(entryData.subjectForData, entryData.dataToUse);
	//		if (chunk && chunk->IsA<UVectorChunk>())
	//		{
	//			MoveReq.SetGoalLocation(Cast<UVectorChunk>(chunk)->Value());
	//			bGoalSet = true;
	//		}
	//		else
	//		{
	//			Global::LogError(BEHAVIOR, "UAT_MoveTo", "ReceiveSequenceEntryData", TEXT("%s!"), chunk ? TEXT("Chunk is not a vector chunk!") : TEXT("Chunk is invalid!"));
	//		}
	//	}
	//}

	const FActorVectorChunk* moveGoal = DataChunk<FActorVectorChunk>(TAG_Data_Task_MoveTo_Goal);
	FVector location = moveGoal ? moveGoal->GetValue() : FVector::ZeroVector;
	if (!location.ContainsNaN() || location != FVector::ZeroVector)
	{
		MoveReq.SetGoalLocation(location);
		bGoalSet = true;
	}

	if (!bGoalSet)
	{
		Global::LogError(BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("Goal was not set!"));
		return false;
	}
	
	//if (FVector::PointsAreNear(location, GetOwnerActor()->GetActorLocation(), 100))
	if (OwnerController->GetPawn() && FVector::PointsAreNear(location, OwnerController->GetPawn()->GetActorLocation(), 100))
	{
		Global::Log(DATADEBUGTRIVIAL, BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("%s already at location!"), *OwnerController->GetPawn()->GetName());
		return false;
	}
	Global::Log(DATATRIVIAL, BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("Move Goal: %s. Distance: %f."), *MoveReq.GetGoalLocation().ToString(), FVector::Dist(location, OwnerController->GetPawn()->GetActorLocation()));
	Global::Log(DATATRIVIAL, BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("OwnerActor: %s. OwnerController->Pawn: %s.")
		, *GetOwnerActor()->GetName()
		, OwnerController->GetPawn() ? *OwnerController->GetPawn()->GetName() : TEXT("Invalid")
	);

	MoveReq.SetAcceptanceRadius(-1.0f);
	//MoveReq.SetAcceptanceRadius(20.0f);
	MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(EAIOptionFlag::Default, MoveReq.IsReachTestIncludingAgentRadius()));
	MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(EAIOptionFlag::Default, MoveReq.IsUsingPartialPaths()));
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(EAIOptionFlag::Default, MoveReq.IsProjectingGoal()));
	if (OwnerController->GetPawn())
	{
		MoveReq.SetNavigationFilter(OwnerController->GetDefaultNavigationFilterClass());
		if (UCharacterMovementComponent* moveComp = OwnerController->GetPawn()->FindComponentByClass<UCharacterMovementComponent>())
		{
			const FFloatChunk* moveSpeed = DataChunk<FFloatChunk>(TAG_Data_Task_MoveTo_Speed);
			if (moveSpeed)
			{
				Global::Log(DATATRIVIAL, BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("Move Speed Original: %f. MinAnalogSpeed: %f."), moveComp->MaxWalkSpeed, moveComp->MinAnalogWalkSpeed);
				moveComp->MaxWalkSpeed = moveSpeed->GetValue() * 100;/// Have to compensate for uu, data is set as human readable m/s
				moveComp->MinAnalogWalkSpeed = moveSpeed->GetValue() * 10;/// Have to compensate for uu, data is set as human readable m/s
				/// Min Analog WalkSpeed will prevent speeds lower than 2 m/s by default
				Global::Log(DATATRIVIAL, BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("Move Speed Updated: %f. MinAnalogSpeed: %f."), moveComp->MaxWalkSpeed, moveComp->MinAnalogWalkSpeed);
			}
			else
			{
				moveComp->MaxWalkSpeed = 350.f;
				Global::LogError(BEHAVIOR, "UAT_MoveTo", "PreActivationInitialization", TEXT("MovementSpeed chunk invalid!"));

			}
		}
	}

	SetUp(OwnerController, MoveReq);
	SetContinuousGoalTracking(false);
	RequestAILogicLocking();

	///Debug: AbilityTask AI Behavior; Why, when AI is at a spot not visible to the instigator, after the first move has finished, subsequent moves are causing this. It has to be because they're already being ended as they're already in position
		///	[0938.75] [503] LogBehavior: Error: Ability not set as task owner for AT_MoveTo_1, when ability ends task will not be automatically ended!Find / Move to  a position in cover from the instigator(B_AI_Controller_Warzone_C_0)::PerformTask

	InitAITask(*OwnerController, source);

	vessel = source.Vessel();

	/// Check to see if we have a tag that indicates a found position, and modify that tag to indicate we are moving to that position
	///Design: GAS LooseTags Position Moving; Seeking/Moving/AtLocation is current state, which fits ASC loose tags (AddLooseGameplayTag/RemoveLooseGameplayTag)
	/// or the task ability's ActivationOwnedTags, which GAS removes automatically when the ability ends. History keeps only the timestamps
	if (FContextualHistoryChunk* contextualTagData = vessel ? vessel->EditData<FContextualHistoryChunk>(TAG_Data_ContextualHistory) : nullptr)
	{

		TArray<FContextHistoryEntry>& tagsContainer = contextualTagData->contextualHistory;

		for (int i = 0; i < tagsContainer.Num(); ++i)
		{
			FGameplayTag tag = tagsContainer[i].contextualTag;
			//Global::Log(DATATRIVIAL, TASK, "UAT_MoveTo", "PreActivationInitialization", TEXT("Tag: %s."), *tag.ToString());

			/// If we have a position tag
			if (tag.MatchesTag(TAG_Contextual_Position))
			{
				/// We want to append the moving context to that tag
				FString tagName = tag.GetTagName().ToString();
				FString stringToAppend = TAG_Moving.GetTag().GetTagName().ToString();
				FString stringToRemove = ".Seeking";

				tagName.RemoveFromEnd(stringToRemove);/// Remove seeking status if available

				FString appendedTagName = tagName + "." + stringToAppend;

				FGameplayTag finishedPositionTag = FGameplayTag::RequestGameplayTag(FName(appendedTagName), false);
				FGameplayTag newTag = FGameplayTag::RequestGameplayTag(FName(appendedTagName), false);

				if (newTag.IsValid())
				{
					Global::Log(DATATRIVIAL, TASK, "UAT_MoveTo", "PreActivationInitialization", TEXT("Appended tag with movement tag: %s. StringName: %s."), *newTag.ToString(), *appendedTagName);
					contextualTagData->AddToValue(newTag);
					finishedTagCache.Add(finishedPositionTag);/// Store the finished position tag (which is a child of the finishedPositionTag so we know it's valid if newTag is)
					/// to be added to contextual history on move to finished
					continue;
				}
			}
		}

		/*for (int i = 0; i < contextualTagData->contextualHistory.Num(); ++i)
		{
			FGameplayTag tag = contextualTagData->contextualHistory[i].contextualTag;
			Global::Log(DATATRIVIAL, TASK, "UAT_MoveTo", "PreActivationInitialization", TEXT("Tag in container: %s."), *tag.ToString());
		}*/

	}

	return true;
}

void UAT_MoveTo::MoveSuccessful()
{
	Global::Log(FULLTRACE, TASK, "UAT_MoveTo", "MoveSuccessful", TEXT(""));
	/// Since movement was successful, add the positional tags that were originally indicated to move to
	if (FContextualHistoryChunk* contextualTagData = vessel ? vessel->EditData<FContextualHistoryChunk>(TAG_Data_ContextualHistory) : nullptr)
	{

		for (int i = 0; i < finishedTagCache.Num(); ++i)
		{
			contextualTagData->AddToValue(finishedTagCache[i]);
		}
	}

	/// EndTask was already called by AITask_MoveTo
	taskFinished.Broadcast();
}

void UAT_MoveTo::MoveFailed()
{
	Global::Log(FULLTRACE, TASK, "UAT_MoveTo", "MoveFailed", TEXT(""));
	/// EndTask was already called by AITask_MoveTo
	taskFinished.Broadcast();
}


#pragma region UAITask_MoveTo Duplicate


void UAT_MoveTo::SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest)
{
	OwnerController = Controller;
	MoveRequest = InMoveRequest;
}

void UAT_MoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}

void UAT_MoveTo::FinishMoveTask(EPathFollowingResult::Type InResult)
{
	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp && PFComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			ResetObservers();
			PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	MoveResult = InResult;
	EndTask();

	if (InResult == EPathFollowingResult::Invalid)
	{
		//OnRequestFailed.Broadcast();
		MoveFailed();
	}
	else
	{
		//OnMoveFinished.Broadcast(InResult, OwnerController);
		MoveSuccessful();
	}
}

void UAT_MoveTo::Activate()
{
	Super::Activate();

	UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
		MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	MoveRequestID = FAIRequestID::InvalidRequest;
	ConditionalPerformMove();
}

void UAT_MoveTo::ConditionalPerformMove()
{
	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(MoveRetryTimerHandle, this, &UAT_MoveTo::ConditionalPerformMove, 0.2f, false);
	}
	else
	{
		MoveRetryTimerHandle.Invalidate();
		PerformMove();
	}
}

void UAT_MoveTo::PerformMove()
{
	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (PFComp == nullptr)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	ResetObservers();
	ResetTimers();

	// start new move request
	FNavPathSharedPtr FollowedPath;
	const FPathFollowingRequestResult ResultData = OwnerController->MoveTo(MoveRequest, &FollowedPath);

	switch (ResultData.Code)
	{
		case EPathFollowingRequestResult::Failed:
			FinishMoveTask(EPathFollowingResult::Invalid);
			break;

		case EPathFollowingRequestResult::AlreadyAtGoal:
			MoveRequestID = ResultData.MoveId;
			OnRequestFinished(ResultData.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
			break;

		case EPathFollowingRequestResult::RequestSuccessful:
			MoveRequestID = ResultData.MoveId;
			PathFinishDelegateHandle = PFComp->OnRequestFinished.AddUObject(this, &UAT_MoveTo::OnRequestFinished);
			SetObservedPath(FollowedPath);

			if (IsFinished())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s> re-Activating Finished task!"), *GetName());
			}
			break;

		default:
			checkNoEntry();
			break;
	}
}

void UAT_MoveTo::Pause()
{
	if (OwnerController && MoveRequestID.IsValid())
	{
		OwnerController->PauseMove(MoveRequestID);
	}

	ResetTimers();
	Super::Pause();
}

void UAT_MoveTo::Resume()
{
	Super::Resume();

	if (!MoveRequestID.IsValid() || (OwnerController && !OwnerController->ResumeMove(MoveRequestID)))
	{
		UE_CVLOG(MoveRequestID.IsValid(), GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Resume move failed, starting new one."), *GetName());
		ConditionalPerformMove();
	}
}

void UAT_MoveTo::SetObservedPath(FNavPathSharedPtr InPath)
{
	if (PathUpdateDelegateHandle.IsValid() && Path.IsValid())
	{
		Path->RemoveObserver(PathUpdateDelegateHandle);
	}

	PathUpdateDelegateHandle.Reset();

	Path = InPath;
	if (Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UAT_MoveTo::OnPathEvent));
	}
}

void UAT_MoveTo::ResetObservers()
{
	if (Path.IsValid())
	{
		Path->DisableGoalActorObservation();
	}

	if (PathFinishDelegateHandle.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp)
		{
			PFComp->OnRequestFinished.Remove(PathFinishDelegateHandle);
		}

		PathFinishDelegateHandle.Reset();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (Path.IsValid())
		{
			Path->RemoveObserver(PathUpdateDelegateHandle);
		}

		PathUpdateDelegateHandle.Reset();
	}
}

void UAT_MoveTo::ResetTimers()
{
	if (OwnerController)
	{
		// Remove all timers including the ones that might have been set with SetTimerForNextTick 
		OwnerController->GetWorldTimerManager().ClearAllTimersForObject(this);
	}
	MoveRetryTimerHandle.Invalidate();
	PathRetryTimerHandle.Invalidate();
}

void UAT_MoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	ResetObservers();
	ResetTimers();

	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp && PFComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	Path = nullptr;
}

void UAT_MoveTo::OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID == MoveRequestID)
	{
		if (Result.HasFlag(FPathFollowingResultFlags::UserAbort) && Result.HasFlag(FPathFollowingResultFlags::NewRequest) && !Result.HasFlag(FPathFollowingResultFlags::ForcedScript))
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> ignoring OnRequestFinished, move was aborted by new request"), *GetName());
		}
		else
		{
			// reset request Id, FinishMoveTask doesn't need to update path following's state
			MoveRequestID = FAIRequestID::InvalidRequest;

			if (bUseContinuousTracking && MoveRequest.IsMoveToActorRequest() && Result.IsSuccess())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> received OnRequestFinished and goal tracking is active! Moving again in next tick"), *GetName());
				GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAT_MoveTo::PerformMove);
			}
			else
			{
				FinishMoveTask(Result.Code);
			}
		}
	}
	else if (IsActive())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s> received OnRequestFinished with not matching RequestID!"), *GetName());
	}
}

void UAT_MoveTo::OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = StaticEnum<ENavPathEvent::Type>();
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
		case ENavPathEvent::NewPath:
		case ENavPathEvent::UpdatedDueToGoalMoved:
		case ENavPathEvent::UpdatedDueToNavigationChanged:
			if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
				UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
				FinishMoveTask(EPathFollowingResult::Aborted);
			}
#if ENABLE_VISUAL_LOG
			else if (!IsActive())
			{
				UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
			}
#endif // ENABLE_VISUAL_LOG
			break;

		case ENavPathEvent::Invalidated:
			ConditionalUpdatePath();
			break;

		case ENavPathEvent::Cleared:
		case ENavPathEvent::RePathFailed:
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
			FinishMoveTask(EPathFollowingResult::Aborted);
			break;

		case ENavPathEvent::MetaPathUpdate:
		default:
			break;
	}
}

void UAT_MoveTo::ConditionalUpdatePath()
{
	// mark this path as waiting for repath so that PathFollowingComponent doesn't abort the move while we 
	// micro manage repathing moment
	// note that this flag fill get cleared upon repathing end
	if (Path.IsValid())
	{
		Path->SetManualRepathWaiting(true);
	}

	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(PathRetryTimerHandle, this, &UAT_MoveTo::ConditionalUpdatePath, 0.2f, false);
	}
	else
	{
		PathRetryTimerHandle.Invalidate();

		ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		if (NavData)
		{
			NavData->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		}
		else
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
	}
}


#pragma endregion

