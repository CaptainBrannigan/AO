// Copyright Jordan Cain. All Rights Reserved.
#pragma once

#include "Tasks/AITask_MoveTo.h"
#include "Abilities/SequenceTaskInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/DataMapInterface.h"
#include "Purpose/DataChunks/FloatChunk.h"
#include "DataChunks/ActorVector.h"
#include "DataChunks/ActorReference.h"
#include "Data/DataTags.h"
#include "Engine/EngineTypes.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Tasks/AITask.h"
#include "AT_MoveTo.generated.h"

class UPurposeAbilityComponent;

/// Debug: AI MoveTo Behavior PlayerPerception; Ensure failed which also broke player perception eqs
/*

[2693.10][920]LogOccurrence: DebugTrivial: End of Occurrence caused by PurposeAbilityComponent_0 PurposeSystem::Occurrence
[2696.63][920]LogStats: FPlatformStackWalk::StackWalkAndDump -  3.572 s
[2696.63][920]LogOutputDevice: Error: === Handled ensure: ===
[2696.63][920]LogOutputDevice: Error: Ensure condition failed: InvocationList[ CurFunctionIndex ] != InDelegate  [File:C:\Epic Games\UE_5.1\Engine\Source\Runtime\Core\Public\UObject\ScriptDelegates.h] [Line: 561]
[2696.63][920]LogOutputDevice: Error: Stack:
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5e03233 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!DispatchCheckVerify<bool,<lambda_d538de0460441ec358549f25f82b830d> >() [C:\Epic Games\UE_5.1\Engine\Source\Runtime\Core\Public\Misc\AssertionMacros.h:172]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5db8683 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!TMulticastScriptDelegate<FWeakObjectPtr>::AddInternal() [C:\Epic Games\UE_5.1\Engine\Source\Runtime\Core\Public\UObject\ScriptDelegates.h:561]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5dc4cc5 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!UAT_MoveTo::EstablishTaskFinishedCallback() [C:\Liege-Root\LiegeWarzone\Plugins\GameFeatures\Warzone\Source\Liege\Public\Abilities\Tasks\AT_MoveTo.h:47]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5dda2d8 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!UAI_SequentialActions::PerformTask() [C:\Liege-Root\LiegeWarzone\Plugins\GameFeatures\Warzone\Source\Liege\Private\Abilities\AI\AI_SequentialActions.cpp:188]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5dbde76 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!UAI_SequentialActions::AttemptSequenceOfActions() [C:\Liege-Root\LiegeWarzone\Plugins\GameFeatures\Warzone\Source\Liege\Private\Abilities\AI\AI_SequentialActions.cpp:83]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffcd5dad9d4 UnrealEditor-Liege-Win64-DebugGame.patch_1.exe!UAI_SequentialActions::ActivateAbility() [C:\Liege-Root\LiegeWarzone\Plugins\GameFeatures\Warzone\Source\Liege\Private\Abilities\AI\AI_SequentialActions.cpp:36]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd60e48c09 UnrealEditor-GameplayAbilities.dll!UGameplayAbility::CallActivateAbility() [D:\build\++UE5\Sync\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\Private\Abilities\GameplayAbility.cpp:831]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd60ed6587 UnrealEditor-GameplayAbilities.dll!UAbilitySystemComponent::InternalTryActivateAbility() [D:\build\++UE5\Sync\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\Private\AbilitySystemComponent_Abilities.cpp:1636]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd60ef19b2 UnrealEditor-GameplayAbilities.dll!UAbilitySystemComponent::TryActivateAbility() [D:\build\++UE5\Sync\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\Private\AbilitySystemComponent_Abilities.cpp:1412]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5efa822d UnrealEditor-LyraGame-Win64-DebugGame.dll!UPurposeAbilityComponent::PerformAbility() [C:\Liege-Root\LiegeWarzone\Source\LyraGame\Purpose\PurposeAbilityComponent.cpp:145]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5efb1643 UnrealEditor-LyraGame-Win64-DebugGame.dll!UPurposeAbilityComponent::ProvidePurposeToOwner() [C:\Liege-Root\LiegeWarzone\Source\LyraGame\Purpose\PurposeAbilityComponent.cpp:375]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5ef2d9a1 UnrealEditor-LyraGame-Win64-DebugGame.dll!PurposeSystem::Private::AttemptToPassSelectedPurposeToOwner() [C:\Liege-Root\LiegeWarzone\Source\LyraGame\Purpose\PurposeEvaluationThread.cpp:365]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5efb196d UnrealEditor-LyraGame-Win64-DebugGame.dll!PurposeSystem::PurposeSelected() [C:\Liege-Root\LiegeWarzone\Source\LyraGame\Purpose\PurposeEvaluationThread.h:791]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5efb1844 UnrealEditor-LyraGame-Win64-DebugGame.dll!FAsyncGraphTask_PurposeSelected::PurposeSelected() [C:\Liege-Root\LiegeWarzone\Source\LyraGame\Purpose\PurposeEvaluationThread.cpp:358]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd5ef6076a UnrealEditor-LyraGame-Win64-DebugGame.dll!TGraphTask<FAsyncGraphTask_PurposeSelected>::ExecuteTask() [C:\Epic Games\UE_5.1\Engine\Source\Runtime\Core\Public\Async\TaskGraphInterfaces.h:1348]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd9517257d UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread() [D:\build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:760]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd9517296d UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksUntilIdle() [D:\build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:660]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffd92e1aa34 UnrealEditor-Engine.dll!FFrameEndSync::Sync() [D:\build\++UE5\Sync\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:12391]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc43b58c UnrealEditor-Win64-DebugGame.exe!FEngineLoop::Tick() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\LaunchEngineLoop.cpp:5590]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc45287d UnrealEditor-Win64-DebugGame.exe!GuardedMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Launch.cpp:202]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc45296a UnrealEditor-Win64-DebugGame.exe!GuardedMainWrapper() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:107]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc455680 UnrealEditor-Win64-DebugGame.exe!LaunchWindowsStartup() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:244]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc4671b4 UnrealEditor-Win64-DebugGame.exe!WinMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:282]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ff7cc46a516 UnrealEditor-Win64-DebugGame.exe!__scrt_common_main_seh() [D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288]
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffe46be7614 KERNEL32.DLL!UnknownFunction []
[2696.63][920]LogOutputDevice: Error: [Callstack] 0x00007ffe486826a1 ntdll.dll!UnknownFunction []
[2696.64][920]LogStats:                SubmitErrorReport -  0.000 s
[2714.65][920]LogAudioMixer: Warning: AudioMixerPlatformInterface Timeout [17559ms] waiting for h/w. InstanceID=9
[2714.65][920]LogStats:                    SendNewReport - 18.011 s
[2714.65][920]LogStats:             FDebug::EnsureFailed - 21.593 s
[2714.65][920]LogOccurrence: DebugTrivial: Ocurrence caused by PurposeAbilityComponent_0 PurposeSystem::Occurrence
[2714.65][920]LogBehavior: DebugTrivial: 1 tasks registered as active for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_0)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 active tasks left for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_0)::TaskFinished
[2714.65][920]LogPurpose: Core: Providing B_AI_Controller_Warzone_C_1 with Behavior: The enemy target hasn't fired at us for awhile and is slightly engaged, move up quickly. PurposeAbilityComponent_0::ProvidePurposeToOwner
[2714.65][920]LogBehavior: Core: currentBehavior for B_AI_Controller_Warzone_C_1 invalid! PurposeAbilityComponent_0::EndCurrentBehavior
[2714.65][920]LogOccurrence: DebugTrivial: Ocurrence caused by PurposeAbilityComponent_0 PurposeSystem::Occurrence
[2714.65][920]LogPurpose: Debug:  Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::ActivateAbility
[2714.65][920]LogBehavior: Debug: Action to perform Get a position a close distance from the previous target. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: Error: Task for entry 0 invalid! Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: Debug: Action to perform Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: DebugTrivial: 1 tasks registered as active for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 active tasks left for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 tasks registered as active for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 active tasks left for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogPurpose: Core: Providing B_AI_Controller_Warzone_C_1 with Behavior: The enemy target hasn't fired at us for awhile and is slightly engaged, move up quickly. PurposeAbilityComponent_0::ProvidePurposeToOwner
[2714.65][920]LogBehavior: Core: Ending Abilities of currentBehavior The enemy target hasn't fired at us for awhile and is slightly engaged, move up quickly(PurposeAbilityComponent_0::B_AI_Controller_Warzone_C_1) for B_AI_Controller_Warzone_C_1. PurposeAbilityComponent_0::EndCurrentBehavior
[2714.65][920]LogPurpose: Debug: Ending AI_SequentialActions_0 for PurposeAbilityComponent_0::B_AI_Controller_Warzone_C_1. PurposeAbilityComponent_0::EndCurrentBehavior
[2714.65][920]LogAbility: Debug: Ending ability: AI_SequentialActions_0 for InterruptedForNewObjective PurposeAbilityComponent_0::EndAbilitiesOf
[2714.65][920]LogAbility: Core: Feedback: InterruptedForNewObjective. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AbilityFinished
[2714.65][920]LogPurpose: Debug:  Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::ActivateAbility
[2714.65][920]LogBehavior: Debug: Action to perform Get a position a close distance from the previous target. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: Error: Task for entry 0 invalid! Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: Debug: Action to perform Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::AttemptSequenceOfActions
[2714.65][920]LogBehavior: DebugTrivial: 1 tasks registered as active for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 active tasks left for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 tasks registered as active for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished
[2714.65][920]LogBehavior: DebugTrivial: 1 active tasks left for action Move near to previous target and idle. Move to a position near the previous target(B_AI_Controller_Warzone_C_1)::TaskFinished

*/

/// Task data (Data/DataTags.h), previously UMovementSpeed, UMoveGoal, UPathLength and UTimeLengthOfMovement
/// Data.Task.MoveTo.Speed		FFloatChunk, m/s
/// Data.Task.MoveTo.Goal		FActorVectorChunk, an actor to move to or a location
/// Data.Task.MoveTo.PathLength	FFloatChunk, meters, set after activation
/// Data.Task.MoveTo.Duration	FFloatChunk, seconds, set after activation

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UAT_MoveTo : public UAITask, public ISequenceTaskInterface, public IDataMapInterface
{
	GENERATED_BODY()
public:

	UAT_MoveTo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuousGoalTracking,ProjectGoalOnNavigation", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "Move To Location or Actor"))
		static UAT_MoveTo* MoveTo(AAIController* Controller, FVector GoalLocation, float moveSpeed, AActor* GoalActor = nullptr,
			float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
			bool bUsePathfinding = true, bool bLockAILogic = true, bool bUseContinuousGoalTracking = false, EAIOptionFlag::Type ProjectGoalOnNavigation = EAIOptionFlag::Default);

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		//OnMoveFinished.Add(callback);
		//OnRequestFailed.Add(callback);
		taskFinished.Add(callback);
		OnMoveFinished.Add(callback);

	/*	FScriptDelegate successDel;
		successDel.BindUFunction(this, "MoveSuccessful");
		OnMoveFinished.Add(successDel);
		FScriptDelegate failDel;
		failDel.BindUFunction(this, "MoveFailed");
		OnRequestFailed.Add(failDel);
		Global::Log(FULLTRACE, TASK, "UAT_MoveTo", "EstablishTaskFiinishedCallback", TEXT("%s. %s.")
			, OnMoveFinished.IsBound() ? TEXT("MoveFinished is bound") : TEXT("MoveFinished not bound")
			, OnRequestFailed.IsBound() ? TEXT("OnRequestFailed is bound") : TEXT("OnRequestFailed not bound")
		);*/

	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override;
	bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		if (Path.IsValid())
		{
			const FFloatChunk* moveSpeed = DataChunk<FFloatChunk>(TAG_Data_Task_MoveTo_Speed);
			const float speed = moveSpeed && moveSpeed->GetValue() > 0.0f ? moveSpeed->GetValue() : 3.5f;/// Same fallback as PreActivationInitialization's 350uu
			const float pathLengthMeters = Path->GetLength() / 100;/// / 100 to translate from uu to meters

			if (FFloatChunk* pathLength = EditData<FFloatChunk>(TAG_Data_Task_MoveTo_PathLength)) { pathLength->value = pathLengthMeters; }
			if (FFloatChunk* duration = EditData<FFloatChunk>(TAG_Data_Task_MoveTo_Duration)) { duration->value = pathLengthMeters / speed; }
			Global::Log(DATATRIVIAL, TASK, GetName(), "PostActivationAdjustment", TEXT("Path Length: %f. Move speed: %f. Time Length: %f."), Path->GetLength(), speed, pathLengthMeters / speed);
			
			return true;
		}
		else
		{
			Global::Log(DATATRIVIAL, TASK, GetName(), "PostActivationAdjustment", TEXT("Unfortunately path appears to not be set at this point!"));
		}
		return false;
	}

	/// Used so that we may dictate lifetimes of tasks that may not have an explicit end, such as a face direction task, if we want it to last as long as the current action is active
	ETaskDurationType GetTaskDurationType() override { return taskDuration; }

	UPROPERTY(EditAnywhere)
		ETaskDurationType taskDuration = ETaskDurationType::EndOnTaskFinished;

	FGameplayTagContainer  GetContextualTagsOfTask() final { return contextualTagForTask; }
	UPROPERTY(EditAnywhere)
		FGameplayTagContainer contextualTagForTask;

	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	const FDataMap& DataMap() const final { return dataMap; }


	UPROPERTY(EditAnywhere)
	FDataMap dataMap;

	/// Force implementers to provide an editable datamap
	FDataMap& MutableDataMap() final { return dataMap; }

protected:

	FSequenceTaskFinished taskFinished;

	/// This cache holds all the tags that will be added to contextual history once task finishes
	/// In this case they are all Contextual.Position tags (without the .Seeking)
	TArray<FGameplayTag> finishedTagCache;

	UPROPERTY()
	TObjectPtr<UPurposeAbilityComponent> vessel = nullptr;

	UFUNCTION()/// Has to be UFUNCTION() otherwise FScriptDelegate cannot bind to it
	void MoveSuccessful();
	UFUNCTION()/// Has to be UFUNCTION() otherwise FScriptDelegate cannot bind to it
	void MoveFailed();

#pragma region UAITask_MoveTo Duplicate

protected:
	/** tries to start move request and handles retry timer */
	void ConditionalPerformMove();

	/** prepare move task for activation */
	void SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest);

	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }
	bool WasMoveSuccessful() const { return MoveResult == EPathFollowingResult::Success; }
	bool WasMovePartial() const { return Path.IsValid() && Path->IsPartial(); }

	/** Allows custom move request tweaking. Note that all MoveRequest need to
	 *	be performed before PerformMove is called. */
	FAIMoveRequest& GetMoveRequestRef() { return MoveRequest; }

	/** Switch task into continuous tracking mode: keep restarting move toward goal actor. Only pathfinding failure or external cancel will be able to stop this task. */
	void SetContinuousGoalTracking(bool bEnable);

protected:
	UPROPERTY(BlueprintAssignable)
		FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
		FMoveTaskCompletedSignature OnMoveFinished;

	/** parameters of move request */
	UPROPERTY()
		FAIMoveRequest MoveRequest;

	/** handle of path following's OnMoveFinished delegate */
	FDelegateHandle PathFinishDelegateHandle;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	/** handle of active ConditionalPerformMove timer  */
	FTimerHandle MoveRetryTimerHandle;

	/** handle of active ConditionalUpdatePath timer */
	FTimerHandle PathRetryTimerHandle;

	/** request ID of path following's request */
	FAIRequestID MoveRequestID;

	/** currently followed path */
	FNavPathSharedPtr Path;

	TEnumAsByte<EPathFollowingResult::Type> MoveResult;
	uint8 bUseContinuousTracking : 1;

	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	/** finish task */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** stores path and starts observing its events */
	void SetObservedPath(FNavPathSharedPtr InPath);

	/** remove all delegates */
	virtual void ResetObservers();

	/** remove all timers */
	virtual void ResetTimers();

	/** tries to update invalidated path and handles retry timer */
	void ConditionalUpdatePath();

	/** start move request */
	virtual void PerformMove();

	/** event from followed path */
	virtual void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	/** event from path following */
	virtual void OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

#pragma endregion
};