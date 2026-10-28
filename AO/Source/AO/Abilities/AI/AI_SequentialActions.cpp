// Copyright Jordan Cain. All Rights Reserved.


#include "AI_SequentialActions.h"
#include "Abilities/Tasks/TaskResources.h"
#include "GameplayTask.h"
#include "Tasks/AITask.h"

///Refactor: Abilities AI Purpose; 
	/// Also need to figure how we want to do AT_Repeat. Should there be a "AT_RepeatInput" Task? If we do that, we can just make an AI_SequentialActions ability 
	/// Make subclasses of AT_Repeat, such as for input

void UAI_SequentialActions::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (sequenceOfActions.Num() < 1)
	{
		Global::Log(DATADEBUG, BEHAVIOR, *AbilityNameForLog(), "ActivateAbility", TEXT("No actions added to sequence!"));
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	if (!Vessel())
	{
		Global::LogError(BEHAVIOR, * AbilityNameForLog(), "ActivateAbility", TEXT("UPurposeAbilityComponent for ability is invalid!!"));
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	/// Ensure that the context data for the current behavior has a data chunk for behavior tags
	///Design: GAS AbilityTags BehaviorTags; Behavior tags describe the running ability, which is what AbilityTags/ActivationOwnedTags are for
	/// ActivationOwnedTags are added to the ASC while the ability runs and removed on end, so conditions could read them from the subject's tags
	ContextDataForBehavior().Set(FBehaviorTagsChunk());

	AttemptSequenceOfActions();
}

void UAI_SequentialActions::AttemptSequenceOfActions()
{
	Global::Log(DATATRIVIAL, BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Action index to perform %d!"), actionIndex);
	if (!sequenceOfActions.IsValidIndex(actionIndex))
	{
		Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Action index %d is invalid, ending Ability!"), actionIndex);
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	FActionSequenceEntry& currentAction = sequenceOfActions[actionIndex];
	Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Action to perform %s."), *currentAction.description.ToString());

	if (actionAttemptsOnFail > maxFailAttempts)
	{
		Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Too many attempts for %s!"), *currentAction.description.ToString());
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	/// Firstly, we need to duplicate all Tasks that are a part of this sequence
	/// This way we can modify the Tasks without affecting the static version in the purpose asset
	for (int i = 0; i < currentAction.tasksForAction.Num(); ++i)
	{
		FActionEntry& taskEntry = currentAction.tasksForAction[i];
		if (!taskEntry.task || !taskEntry.requiredResource)
		{
			Global::LogError(BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Task for entry %d invalid!"), i);
			continue;
		}

		UGameplayTask* taskDuplicate = NewAbilityTask(taskEntry.task);
		
		taskDuplicate->AddClaimedResource(taskEntry.requiredResource);
		taskDuplicatesForCurrentAction.Add(taskDuplicate);/// the task now has required resources
		/// The index of the cached duplicate and the original entry are identical, so when we go to perform data adjustments we just use the same index
	}

	/// If no tasks were added for an action, then attempt the next action
	if (taskDuplicatesForCurrentAction.Num() <= 0)
	{
		AttemptNextSequenceOfActions();
	}

	PerformTask();
}

void UAI_SequentialActions::PerformTask()
{
	Global::Log(FULLTRACE, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT(""));
	if (!IsActive() || bIsAbilityEnding)/// As we are utilizing gameplay tasks and delegates, we need to account for callbacks when ability is ending
	{
		return;
	}
	Global::Log(CALLTRACETRIVIAL, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("Attempting to Perform Task."));

	if (!sequenceOfActions.IsValidIndex(actionIndex))
	{
		Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("Action index %d is invalid!"), actionIndex);
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	if (!Vessel())
	{
		Global::LogError(BEHAVIOR, *AbilityNameForLog(), "PerformTask", TEXT("UPurposeAbilityComponent for ability is invalid!!"));
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	FActionSequenceEntry& entry = sequenceOfActions[actionIndex];

	TArray<int> taskIndicesToCancel;

	/// Perform all tasks of this action
	for (int i = 0; i < taskDuplicatesForCurrentAction.Num(); ++i)
	{
		TObjectPtr<UGameplayTask> task = taskDuplicatesForCurrentAction[i];

		if (!task)
		{
			Global::LogError(TASK, AbilityNameForLog(), "PerformTask", TEXT("Task invalid!"));
			continue;
		}

		Global::Log(DATATRIVIAL, TASK, AbilityNameForLog(), "PerformTask", TEXT("Performing %s."), *task->GetName());

		/// Now that we are ready to activate our task, we want a pre activation data adjustment
		/// This allows us to take datachunks from one task and apply them to another, modifying how the task is performed based on adjustments requested in the details panel in editor
		/// And we only perform adjustments on the current task, so that any post activation initialization may occur before data adjustments (except on the very first task)
		/// Because the tasks were duplicated from tasksForAction, the indices will align with taskDuplicatesForCurrentAction
		FActionEntry& taskEntry = entry.tasksForAction[i];

		/// Perform any data adjustments specified on this task entry
		for (const FTaskAndDataPair& adjustment : taskEntry.dataAdjustmentsForThisTask)
		{
			IDataMapInterface* taskData = Cast<IDataMapInterface>(task);
			FDataMapEntry* chunkToAdjust = taskData ? taskData->EditDataMap().FindEntry(adjustment.dataToModify) : nullptr;

			const FDataMapEntry* chunkToGetAdjustmentFrom = nullptr;
			if (adjustment.subjectToRetrieveFrom == ESubject::Context)/// As the context data is not stored in the subjects map (isn't an IDataMapInterface), we have to manually access the data map for context
			{
				chunkToGetAdjustmentFrom = ContextDataForBehavior().FindEntry(adjustment.dataToRetrieve);
			}
			else/// But for any other subject it will be stored via context.subjects
			{
				if (UPurposeAbilityComponent* subjectWithBehavior = Vessel()->CurrentBehavior().Subject<UPurposeAbilityComponent>(adjustment.subjectToRetrieveFrom))
				{
					/// The subject's own data first, then the data of the behavior it is performing
					chunkToGetAdjustmentFrom = subjectWithBehavior->DataMap().FindEntry(adjustment.dataToRetrieve);
					if (!chunkToGetAdjustmentFrom)
					{
						chunkToGetAdjustmentFrom = subjectWithBehavior->CurrentBehavior().contextData.FindEntry(adjustment.dataToRetrieve);
					}
				}
				else if (const FDataMap* subjectData = Vessel()->CurrentBehavior().DataMapForSubject(adjustment.subjectToRetrieveFrom))
				{
					chunkToGetAdjustmentFrom = subjectData->FindEntry(adjustment.dataToRetrieve);
				}
			}

			if (!chunkToAdjust || !chunkToGetAdjustmentFrom)
			{
				Global::Log(DATATRIVIAL, TASK, AbilityNameForLog(), "PerformTask", TEXT("DataChunk to adjust: %s; DataChunk to get adjustment from: %s.")
					, chunkToAdjust ? *adjustment.dataToModify.ToString() : TEXT("invalid")
					, chunkToGetAdjustmentFrom ? *adjustment.dataToRetrieve.ToString() : TEXT("invalid")
				);
				continue;
			}

			Global::Log(DATATRIVIAL, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("Attempting to copy %s to %s for %s!"), *adjustment.dataToRetrieve.ToString(), *adjustment.dataToModify.ToString(), *task->GetName());
			chunkToAdjust->CopyValueFrom(*chunkToGetAdjustmentFrom);
		}

		bool bShouldPerformTask = true;

		ISequenceTaskInterface* sequenceTask = Cast<ISequenceTaskInterface>(task);
		if (sequenceTask)
		{
			//bShouldPerformTask = sequenceTask->PreActivationInitialization(*this, (uint8)EAITaskPriority::Low);
			bShouldPerformTask = sequenceTask->PreActivationInitialization(entry, *this);

			if (!bShouldPerformTask) {
				taskIndicesToCancel.Add(i);
				Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("%s should not be performed! State: %s"), *entry.description.ToString(), *Global::EnumValueOnly<EGameplayTaskState>(task->GetState()));
				continue; }/// If the task notifies us it shouldn't be performed simply ignore it

			FScriptDelegate finishDel;
			finishDel.BindUFunction(this, "TaskFinished");

			sequenceTask->EstablishTaskFinishedCallback(finishDel);
		}

		if (!bShouldPerformTask) { 
			taskIndicesToCancel.Add(i);
			Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("%s should not be performed! State: %s"), *entry.description.ToString(), *Global::EnumValueOnly<EGameplayTaskState>(task->GetState()));
			continue; }/// If the task notifies us it shouldn't be performed simply ignore it

		/// Task may have data relevant to conditions established by pre activation, which we may need access to at the time of the occurrence
		if (task->Implements<UDataMapInterface>())
		{
			AttemptToAppendTaskDataToContextData(Cast<IDataMapInterface>(task.Get())->DataMap());
		}

		/// BugWatch: AbilityTasks GameplayTasks; If InitTask() is not called with this ability as the owner, then tasks will not be destroyed correctly when ability ends
		/// This requires that the TasksComponent be valid for task
		/// Which will be set via InitTask(), which this will call
		UGameplayTasksComponent::RunGameplayTask(*this, *task, (uint8)EAITaskPriority::Low, FGameplayResourceSet(), FGameplayResourceSet());

		if (task->GetTaskOwner() != this)
		{
			Global::LogError(BEHAVIOR, AbilityNameForLog(), "PerformTask", TEXT("Ability not set as task owner for %s, when ability ends task will not be automatically ended!"), *task->GetName());
		}

		/// Task activation should be synchronous, so task should be activated and running by the time we get here
		/// This is important, as some data adjustments for a task require the task to be activate (such as getting the path from a MoveTo)
		/// We also check for whether the task is finished, because activation logic will run through by this point, which may result in an immediate ending of the task
		if (sequenceTask && !task->IsFinished())
		{
			sequenceTask->PostActivationAdjustment(entry, *this);
		}
	}

	/// Allow action to adjust the current context data as desired
	for (const FContextAdjustmentFromAction& adjustment : entry.contextAdjustmentByAction)
	{
		if (adjustment.subjectToAdjust == ESubject::None)
		{
			continue;
		}

		if (!adjustment.dataAdjustment.IsValid())
		{
			continue;
		}

		Vessel()->CurrentBehavior().AdjustData(adjustment.subjectToAdjust, adjustment.dataKey, adjustment.dataAdjustment.GetPtr<FDataModifier>());
	}

	/// Ordinarily ActionPerformed, and thus an ability Occurrence, is only called via ability activation
	/// We also want an occurrence every time a candidate performs a new action within that behavior
	Vessel()->ActionPerformed(this);

	/// IIf the only task of the last action is not performed, the ability wil be in a state of limbo until an occurrence interrupts with a new behavior
	/// So we need to manually check for tasks to remove/end, then check task count
	for (int indexToRemove : taskIndicesToCancel)
	{
		if (!taskDuplicatesForCurrentAction.IsValidIndex(indexToRemove))
		{
			continue;
		}

		TObjectPtr<UGameplayTask> task = taskDuplicatesForCurrentAction[indexToRemove];

		task->EndTask();
		taskDuplicatesForCurrentAction.RemoveAt(indexToRemove);
	}
	
	/// Now we can run through TaskFinished logic
	/// If the only tasks left for action "EndOnActionFinished" then we will simply move to the next action
		/// Since these tasks never call to TaskFinished we have to manually attempt to finish this action
		/// This way AddTaskReadyForActivation() is respected by only ending action if there are tasks considered finished or "EndOnActionFinished"
		/// At this point tasks have not yet been activated, so we can't simply check for != Active || Paused
		/// All tasks at this point are AwaitingActivation 
	TaskFinished();
}

void UAI_SequentialActions::TaskFinished()
{
	if (!this)/// This is called via delegate, so we want to handle any possible deprecated callbacks
	{
		return;
	}

	if (!IsActive())/// This is called via delegate, so we want to handle any possible deprecated callbacks
	{
		return;
	}

	Global::Log(DATADEBUGTRIVIAL, BEHAVIOR, AbilityNameForLog(), "TaskFinished", TEXT("%d tasks registered as active for action %s."), taskDuplicatesForCurrentAction.Num(), *sequenceOfActions[actionIndex].description.ToString());

	/// In order to dictate if the task is completed, we check for how many tasks exist
	/// However, we decrease this number if a task states it is meant to end when the action ends
	int activeTaskCount = taskDuplicatesForCurrentAction.Num();

	for (int i = taskDuplicatesForCurrentAction.Num() - 1; i >= 0; --i)/// Iterate through all activeTasks for the current action
	{
		//TWeakObjectPtr<UGameplayTask> taskOfCurrentAction = taskDuplicatesForCurrentAction[i];
		TObjectPtr<UGameplayTask> taskOfCurrentAction = taskDuplicatesForCurrentAction[i];

		//if (!taskOfCurrentAction.IsValid())/// If invalid, reduce active tasks
		if (!taskOfCurrentAction)/// If invalid, reduce active tasks
		{
			taskDuplicatesForCurrentAction.RemoveAt(i);
			activeTaskCount--;
			continue;
		}

		//if (taskOfCurrentAction->IsFinished() || taskOfCurrentAction->GetState() == EGameplayTaskState::Uninitialized || taskOfCurrentAction->GetState() == EGameplayTaskState::AwaitingActivation)/// If finished, reduce active tasks
			///// if task is either unintialized
		/*if (taskOfCurrentAction->GetState() != EGameplayTaskState::Active || taskOfCurrentAction->GetState() != EGameplayTaskState::Paused)
			///If the task is neither active nor paused, consider it complete*/
		if(taskOfCurrentAction->IsFinished())/// If the task is considered finished, reduce active tasks
		{
			/// Since task is finished, let's copy what DataChunks are stored on the task to the context so that we can access them in the future
			if (taskOfCurrentAction->Implements<UDataMapInterface>())
			{
				AttemptToAppendTaskDataToContextData(Cast<IDataMapInterface>(taskOfCurrentAction.Get())->DataMap());
			}

			Global::Log(DATATRIVIAL, BEHAVIOR, AbilityNameForLog(), "TaskFinished", TEXT("%s is not active, decreasing count!"), *taskOfCurrentAction->GetName());

			taskDuplicatesForCurrentAction.RemoveAt(i);
			activeTaskCount--;/// Ensure the active task count is also reduced
			continue;
		}

		if (taskOfCurrentAction->Implements<USequenceTaskInterface>())
		{
			if (Cast<ISequenceTaskInterface>(taskOfCurrentAction)->GetTaskDurationType() == ETaskDurationType::EndOnActionFinished)
			{
				/// Though the task is still active, it indicated it does not have logic that will end the task before action completion
				/// And so we wish to ignore this task when considering if action is complete
				activeTaskCount--;
				continue;
			}
		}
	}

	//if (taskDuplicatesForCurrentAction.Num() < 1)/// If there are no more active tasks for the current action
	if (activeTaskCount < 1)/// If there are no more tasks considered active for the current action
	{
		Global::Log(DATADEBUG, BEHAVIOR, AbilityNameForLog(), "TaskFinished", TEXT("All tasks for action %s finished!"), *sequenceOfActions[actionIndex].description.ToString());
		AttemptNextSequenceOfActions();/// Then we can attempt the next action in the sequence
		return;
	}

	Global::Log(DATADEBUGTRIVIAL, BEHAVIOR, AbilityNameForLog(), "TaskFinished", TEXT("%d active tasks left for action %s."), activeTaskCount, *sequenceOfActions[actionIndex].description.ToString());
}

void UAI_SequentialActions::AttemptToAppendTaskDataToContextData(const FDataMap& dataToAppend)
{
	if (!Vessel())
	{
		Global::LogError(BEHAVIOR, *AbilityNameForLog(), "AttemptToAppendTaskDataToContextData", TEXT("UPurposeAbilityComponent for ability is invalid!!"));
		AbilityFinished(EAbilityPurposeFeedback::FinishedUninterrupted);
		return;
	}

	for (const FDataMapEntry& entryToAdd : dataToAppend.Entries)
	{
		const FDataChunk* chunk = entryToAdd.Get();
		if (!chunk || !chunk->HasValue())
		{
			continue;
		}

		ContextDataForBehavior().Set(entryToAdd.Chunk);/// If the data is already contained it is replaced, otherwise added
	}
}


