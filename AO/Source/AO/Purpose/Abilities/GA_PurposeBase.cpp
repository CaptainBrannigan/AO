// Copyright Jordan Cain. All Rights Reserved.


#include "GA_PurposeBase.h"
#include "Purpose/PurposeAbilityComponent.h"
#include "Tasks/AITask.h"
#include "AIController.h"

UGA_PurposeBase::UGA_PurposeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_PurposeBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    timeOfAbilityActivation = FTimespan(FDateTime::UtcNow().GetTicks());
	Global::Log(DATADEBUG, PURPOSE, AbilityNameForLog(), "ActivateAbility", TEXT(""));

	//if(IsValid(Vessel()) && IsValid(Vessel()->Manager())) { Vessel()->Manager()->Occurrence(UOccurrenceContext::CreateContext(Vessel(), context)); }
}

TObjectPtr<UGameplayTask> UGA_PurposeBase::NewAbilityTask(TSubclassOf<UGameplayTask> templateClass)
{
	TObjectPtr<UGameplayTask> task = NewObject<UGameplayTask>(this, templateClass);

	if (task && task->IsA<UAITask>())
	{
		Cast<UAITask>(task)->InitAITask(*Cast<AAIController>(GetControllerFromActorInfo()), *this);
	}

	/// We cannot call InitTask directly, so we have to rely on UGameplayTasksComponent::RunGameplayTask

	return task;
}

TObjectPtr<UGameplayTask> UGA_PurposeBase::NewAbilityTask(TObjectPtr<UGameplayTask> templateTask)
{
	UGameplayTask* taskDuplicate = DuplicateObject<UGameplayTask>(templateTask, this);
	if (!taskDuplicate)
	{
		Global::LogError(BEHAVIOR, AbilityNameForLog(), "AttemptSequenceOfActions", TEXT("Duplication of %s failed!"), templateTask ? *templateTask->GetName() : TEXT("invalid inTask"));
		return nullptr;
	}
	
	if (taskDuplicate->IsA<UAITask>())
	{
		Cast<UAITask>(taskDuplicate)->InitAITask(*Cast<AAIController>(GetControllerFromActorInfo()), *this);
	}

	/// We cannot call InitTask directly, so we have to rely on UGameplayTasksComponent::RunGameplayTask

	return taskDuplicate;
}

void UGA_PurposeBase::VisualLog(FString inText)
{
	UE_VLOG(GetOwningActorFromActorInfo(), LogAbility, Log, TEXT("%s"), *inText);
}

void UGA_PurposeBase::VisualAndOutputLog(FString VLogText, bool useLogText, FString LogText)
{
	if (useLogText)
	{
		UE_VLOG(GetOwningActorFromActorInfo(), LogAbility, Log, TEXT("%s"), *VLogText);
		//		UE_LOG(LogGameplayAbilities, Log, TEXT("%s"), *LogText);
	}
	else
	{
		UE_VLOG_UELOG(GetOwningActorFromActorInfo(), LogAbility, Log, TEXT("%s"), *VLogText);
	}
}

void UGA_PurposeBase::VisualAndWarningLog(FString VLogText, bool useLogText, FString LogText)
{
	if (useLogText)
	{
		UE_VLOG(GetOwningActorFromActorInfo(), LogAbility, Warning, TEXT("%s"), *LogText);
		UE_LOG(LogAbility, Warning, TEXT("%s"), *LogText);
	}
	else
	{
		UE_VLOG_UELOG(GetOwningActorFromActorInfo(), LogAbility, Warning, TEXT("%s"), *VLogText);
	}
}

void UGA_PurposeBase::VisualAndErrorLog(FString VLogText, bool useLogText, FString LogText)
{
	if (useLogText)
	{
		UE_VLOG(GetOwningActorFromActorInfo(), LogAbility, Error, TEXT("%s"), *VLogText);
		UE_LOG(LogAbility, Error, TEXT("%s"), *LogText);
	}
	else
	{
		UE_VLOG_UELOG(GetOwningActorFromActorInfo(), LogAbility, Error, TEXT("%s"), *VLogText);
	}
}

TObjectPtr<UPurposeAbilityComponent> UGA_PurposeBase::Vessel()
{ 
	return Cast<UPurposeAbilityComponent>(GetAbilitySystemComponentFromActorInfo()); }

AController* UGA_PurposeBase::GetControllerFromActorInfo() const
{
	if (CurrentActorInfo)
	{
		if (AController* PC = CurrentActorInfo->PlayerController.Get())
		{
			return PC;
		}

		// Look for a player controller or pawn in the owner chain.
		AActor* TestActor = CurrentActorInfo->OwnerActor.Get();
		while (TestActor)
		{
			if (AController* C = Cast<AController>(TestActor))
			{
				return C;
			}

			if (APawn* Pawn = Cast<APawn>(TestActor))
			{
				return Pawn->GetController();
			}

			TestActor = TestActor->GetOwner();
		}
	}

	return nullptr;
}


void UGA_PurposeBase::TickAbility(float DeltaTime)
{
}

#pragma region Gameplay Ability

void UGA_PurposeBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	VisualLog(FString::Printf(TEXT("Ending %s. UGA_PurposeBase::EndAbility"), *GetName()));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#pragma endregion

