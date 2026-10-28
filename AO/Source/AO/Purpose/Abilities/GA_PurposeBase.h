// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "Data/GlobalLog.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Timespan.h"
#include "Data/DataMapInterface.h"
#include "Purpose/PurposeEvaluationThread.h"
#include "GA_PurposeBase.generated.h"

UENUM()
///These priorities are strictly for AI logic
///Design: GAS Tags Priority Cancel; Priority + EndAbilitiesOf(class) is a hand-built version of GAS ability tags
/// Give behaviors AbilityTags (Behavior.Combat, Behavior.Idle) and use CancelAbilitiesWithTag / BlockAbilitiesWithTag on the higher priority ones,
/// then EndAbilitiesOf becomes CancelAbilities(&WithTags) and overlapping-resource logic can be data instead of code
enum class EAbilityPriority : uint8
{
	Lowest = 0,
	Low = 64, //0.5 * FGameplayAbilities::DefaultPriority,
	AutonomousAI = 127, //FGameplayAbilities::DefaultPriority,
	High = 192, //1.5 * FGameplayAbilities::DefaultPriority,
	Ultimate = 254,
};

UENUM()
/// Different from EGameplayAbilityState in that this result is utilized by the Purpose system in determining what happens when a Ability is finished
enum class EAbilityPurposeFeedback : uint8
{
	None/// Default to provide unintialized state
	, InterruptedByOverlappingResources
	, InterruptedForNewObjective
	, InterruptedByDeath
	, FinishedUninterrupted
};

/**
 *
 */
UCLASS(Abstract, BlueprintType)
class AO_API UGA_PurposeBase : public UGameplayAbility, public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()
public:

	UGA_PurposeBase(const FObjectInitializer& ObjectInitializer);

	virtual TObjectPtr<UGA_PurposeBase> Initialize()
	{
		//Global::Log(FULLTRACE, AbilityLog, *this, "Initialize", TEXT("InitAbility."));
		return this;
	}

	/// Actually activate ability, do not call this directly 
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void AbilityFinished(const EAbilityPurposeFeedback reasonAbilityEnded) { abilityFeedbackState = reasonAbilityEnded; }

	/** Called when the ability is removed from an AbilitySystemComponent */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override { Super::OnRemoveAbility(ActorInfo, Spec); }

	/// @param templateClass: Create a new task based off UGameplayTask class
	/// @return TObjectPtr<UGameplayTask>: What's important here is the TaskOwner is correctly established as the ability. If not, the task will not be added to ActiveTasks, and will not be cleaned up correctly
	/// which will cause a number of issues for the TasksComponent
	TObjectPtr<UGameplayTask> NewAbilityTask(TSubclassOf<UGameplayTask> templateClass);

	/// @param templateClass: Create a new task by duplicating the provided task
	/// @return TObjectPtr<UGameplayTask>: What's important here is the TaskOwner is correctly established as the ability. If not, the task will not be added to ActiveTasks, and will not be cleaned up correctly
	/// which will cause a number of issues for the TasksComponent
	TObjectPtr<UGameplayTask> NewAbilityTask(TObjectPtr<UGameplayTask> templateTask);

	bool HasAuthority() const
	{
		return (CurrentActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Authority);
	}

	const EAbilityPurposeFeedback FeedbackState() const { return abilityFeedbackState; }

	virtual FString AbilityNameForLog() { return GetName() + "(" + GetOwningActorFromActorInfo()->GetName() + ")"; }

	///Logs a string to the Visual Log
	void VisualLog(FString inText);

	///Logs a string to the Visual Log and Output Log as Log
	///If useLogText, Output Log will use LogText seperately
	void VisualAndOutputLog(FString VLogText, bool useLogText = false, FString LogText = TEXT("None"));

	///Logs a string to the Visual Log and Output Log as Warning
	///If useLogText, Output Log will use LogText seperately
	void VisualAndWarningLog(FString VLogText, bool useLogText = false, FString LogText = TEXT("None"));

	///Logs a string to the Visual Log and Output Log as Error
	///If useLogText, Output Log will use LogText seperately
	void VisualAndErrorLog(FString VLogText, bool useLogText = false, FString LogText = TEXT("None"));

	const FTimespan TimeAbilityWasActivated() { return timeOfAbilityActivation; }

	/// @return bool: True when the Ability has not reached it's minimum time of being activated to begin + a buffer or past the minimum time to allow a ability to be performed once begun
	///Design: GAS Tags Interruption GameplayEffects; The uninterruptable window (actionMinTime) can be a duration GameplayEffect granting State.Committed on activation
	/// New behaviors then list State.Committed in ActivationBlockedTags, and purpose scoring can skip candidates that have it (snapshot the owned tags)
	bool AbilityIsInterruptable()
	{
		return (FTimespan(FDateTime::UtcNow().GetTicks()).GetTotalSeconds() - TimeAbilityWasActivated().GetTotalSeconds()) < (reactionMinTime * 3) /// Multiply minimum reaction to give AI time to visibly begin Ability
			|| (FTimespan(FDateTime::UtcNow().GetTicks()).GetTotalSeconds() - TimeAbilityWasActivated().GetTotalSeconds()) > actionMinTime;
	}

	TObjectPtr<UAnimInstance> abilityAnimInstance = nullptr;

	TObjectPtr<class UPurposeAbilityComponent> Vessel();
	TObjectPtr<AActor> VesselActor() { return GetOwningActorFromActorInfo(); }


	/// Lyra CopyOver

	UFUNCTION(BlueprintCallable, Category = "Ability")
	AController* GetControllerFromActorInfo() const;

protected:

	FString NetInfo()
	{
		return Global::NetModeAsString(GetOwningActorFromActorInfo()->GetNetMode());
	}

	/// A minimum amount of timer before an AI may perform a ticking logic, to emulate human reaction times
	float reactionMinTime = 0.3f;/// 0.25f for simple ability and 0.4f for reaction requiring more thought, according to studies

	/// A minimum time in which the action is un-interruptable
	/// It both enforces that characters stick to the actions that have been chosen, and prevent them from interrupting themselves over every occurrence
	float actionMinTime = 1.5f;

	virtual void TickAbility(float DeltaTime);

	bool bTickingAbility = false;

private:

	/// We store this feedback as a state to track the status of the Ability in the Purpose System
	/// For example, if a AbilityFinished() is called because it needs to be interrupted by a Reaction, then it will eventually lead to EndObjective, which will attempt to call AbilityFinished() on all of Objective's Abilities
		/// Which will include the Ability which originally led to EndObjective. So abilityFeedbackState is necessary to prevent a recursive calls of AbilityFinished()
	/// Private so that only AbilityFinished may affect it
	EAbilityPurposeFeedback abilityFeedbackState = EAbilityPurposeFeedback::None;

	FTimespan timeOfAbilityActivation;

# pragma region Gameplay Ability
public:

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

# pragma endregion
};
