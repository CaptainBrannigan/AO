// Copyright Jordan Cain. All Rights Reserved.



#include "PurposeAbilityComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

#include "AbilitySystemLog.h"
#include "AttributeSet.h"
#include "GameplayPrediction.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitySpec.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/PlayerController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "TickableAttributeSetInterface.h"
#include "Purpose/Abilities/GA_PurposeBase.h"
#include "Purpose/Assets/EventAsset.h"
//director #include "Purpose/Director_Level.h"
#include "Purpose/DataChunks/TimeOf.h"
#include "Purpose/DataChunks/ContextualHistory.h"
#include "Data/DataTags.h"
#include "Purpose/Abilities/GA_Behavior.h"
#include "Purpose/Abilities/Behavior_AI.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

UPurposeAbilityComponent::UPurposeAbilityComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Global::Log(CALLTRACEESSENTIAL, PURPOSE, *this, "UPurposeAbilityComponent", TEXT("Owner: %s"), GetOwner() ?  *GetOwner()->GetName() : TEXT("Invalid"));

}

//void UPurposeAbilityComponent::InitializePurposeSystem(TObjectPtr<class ADirector_Level> inDirector)
//{
//	director = inDirector;
//
//	AbilityActivatedCallbacks.AddUObject(this, &UPurposeAbilityComponent::ActionPerformed);/// Primarily set up since player input goes straight to the ability system
//	/// But now all behavior occurrences are routed through BehaviorPerformed
//
//	SetData(FContextualHistoryChunk());
//}

void UPurposeAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	/// Here we list the variables we want to replicate + a condition if wanted
	
	/// Push model: data is only compared for replication after OnDataMapChanged marks it dirty
	FDoRepLifetimeParams params;
	params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPurposeAbilityComponent, data, params);
}

#pragma region Datamap Interface

void UPurposeAbilityComponent::OnDataMapChanged()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UPurposeAbilityComponent, data, this);
}

void UPurposeAbilityComponent::InitializeDataForOwner()
{
	data.InitializeForOwner(GetAvatarActor(), this);
	OnDataMapChanged();
}

#pragma endregion

/// BugWatch: AI Purpose PerformAbility; If an AI reaches a point where their Activity purpose ends, and they have no other Activity behaviors to perform
	/// They may just stand there helplessly
	/// Or they may bounce back and forth between unintended Behaviors
		/// If that happens too quickly, exiting the game may force an !IsRooted assertion from PurposeThread as too many ASyncAbilities are going in and out to clean up on shutdown

#pragma region Gameplay Abilities

void UPurposeAbilityComponent::PerformAbility(FContextData& inContext, TSubclassOf<UGA_PurposeBase> abilityClass)
{
	////Global::Log(Debug, AbilityLog, "UPurposeAbilityComponent", "PerformAbility", TEXT("New ability of %s"), *inContext->GetName());
	if (abilityClass)
	{
		FGameplayAbilitySpecHandle handle = GiveAbility(FGameplayAbilitySpec(abilityClass, 1, INDEX_NONE, this));

		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(handle);
		if(Spec)
		{
			SetRemoveAbilityOnEnd(Spec->Handle);/// Our abilities are set as a one time use, as they are selected via purpose system individually and contextually, not like in a set state machine or behavior tree

			TryActivateAbility(handle);

			inContext.purpose.behaviorAbility = Cast<UBehavior_AI>(Spec->Ability);/// Since the context data purpose is a copy, we can use the instance of the gameplay ability rather than the static version held in an asset 
			/// Otherwise the datamap will always be empty when the purpose evaluation thread attempts to append a behavior's data prior to condition evaluation
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("PerformAbility(TSubclassOf<>) found invalid Handle"));
			Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Owner: , Class: %s, Context: %s."),
				//IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid"),
				abilityClass.Get() ? *abilityClass->GetName() : TEXT("Invalid")
				, *inContext.GetName()
			);
		}
	}
	else
	{
		Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Owner: , Class: %s, Context: %s."),
			//IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid"),
			abilityClass.Get() ? *abilityClass->GetName() : TEXT("Invalid")
			, *inContext.GetName()
		);
	}
}

void UPurposeAbilityComponent::PerformAbility(FContextData& inContext, const TObjectPtr<UGA_PurposeBase> ability, uint8 priority)
{
	if (!IsValid(ability))
	{
		Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Owner: %s, Ability: %s, Context: %s."),
			IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid"),
			IsValid(ability) ? *ability->GetName() : TEXT("Invalid")
			, *inContext.GetName()
		);
		return;
	}
	FGameplayAbilitySpec Spec = GiveAbilityDuplicate(FGameplayAbilitySpec(ability, 1, INDEX_NONE, this));

	FGameplayAbilitySpecHandle handle = Spec.Handle;
	if (Spec.Ability)
	{
		inContext.purpose.behaviorAbility = Cast<UBehavior_AI>(Spec.Ability);/// Since the context data purpose is a copy, we can use the instance of the gameplay ability rather than the static version held in an asset 
		/// Otherwise the datamap will always be empty when the purpose evaluation thread attempts to append a behavior's data prior to condition evaluation

		//SetRemoveAbilityOnEnd(Spec->Handle);/// Our abilities are set as a one time use, as they are selected via purpose system individually and contextually, not like in a set state machine or behavior tree
		/// I believe the above version was clearing the ability prematurely, otherwise it does this anyways
		///Spec->RemoveAfterActivation = true;/// Our abilities are set as a one time use, as they are selected via purpose system individually and contextually, not like in a set state machine or behavior tree

		TryActivateAbility(handle);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("PerformAbility(TSubclassOf<>) found invalid Handle"));

		Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Owner: %s, Ability: %s, Context: %s."),
			IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid"),
			IsValid(ability) ? *ability->GetName() : TEXT("Invalid")
			, *inContext.GetName()
		);
	}
}

void UPurposeAbilityComponent::PerformAbility(FContextData& inContext, UGA_PurposeBase& ability)
{
	FGameplayAbilitySpec Spec = GiveAbilityDuplicate(FGameplayAbilitySpec(&ability, 1, INDEX_NONE, this));

	FGameplayAbilitySpecHandle handle = Spec.Handle;
	if (Spec.Ability)
	{
		inContext.purpose.behaviorAbility = Cast<UBehavior_AI>(Spec.Ability);/// Since the context data purpose is a copy, we can use the instance of the gameplay ability rather than the static version held in an asset 
		/// Otherwise the datamap will always be empty when the purpose evaluation thread attempts to append a behavior's data prior to condition evaluation

		bool activated =TryActivateAbility(handle);
	
		if(!activated)
		{
			ABILITY_LOG(Warning, TEXT("PerformAbility(TSubclassOf<>) found invalid Handle"));

			Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Owner: %s, Ability: %s. Failed to activate reference to ability!")
				, IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid")
				, *ability.GetName()
			);
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("PerformAbility(TSubclassOf<>) found invalid Handle"));

		Global::LogError(TASK, "UPurposeAbilityComponent", "PerformAbility", TEXT("Failed to find spec.Ability! Owner: %s, Ability: %s, Context: %s.")
			, IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("Invalid")
			, *ability.GetName()
			, *inContext.GetName()
		);
	}
}

void UPurposeAbilityComponent::EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TObjectPtr<UGA_PurposeBase> abilityToExclude)
{
	/** Cancels the specified ability CDO. */
	///void CancelAbility(UGameplayAbility * Ability);

	/** Cancels the ability indicated by passed in spec handle. If handle is not found among reactivated abilities nothing happens. */
	///void CancelAbilityHandle(const FGameplayAbilitySpecHandle & AbilityHandle);

	/** Cancel all abilities with the specified tags. Will not cancel the Ignore instance */
	///void CancelAbilities(const FGameplayTagContainer * WithTags = nullptr, const FGameplayTagContainer * WithoutTags = nullptr, UGameplayAbility * Ignore = nullptr);

	/** Cancels all abilities regardless of tags. Will not cancel the ignore instance */
	///void CancelAllAbilities(UGameplayAbility * Ignore = nullptr);
	TArray<FGameplayAbilitySpec> abilitySpecs = ActivatableAbilities.Items;
	for (FGameplayAbilitySpec& Spec : abilitySpecs)
	{
		///if (ability->IsA(inClass)) In this case we may actually want to be specific, and not affect derived classes
		/// Design: Ability Purpose Feedback; In the future, we may consider allowing this to use IsA, and affect derived ability classes
		if (Spec.Ability->GetClass()->IsChildOf(inClass) && Spec.Ability != abilityToExclude)
		{
			Global::Log(DATADEBUG, TASK, *this, "EndAbilitiesOf", TEXT("Ending ability: %s for %s"), *Spec.Ability->GetName(), *Global::EnumValueOnly<EAbilityPurposeFeedback>(reasonAbilityEnded));
			Cast<UGA_PurposeBase>(Spec.Ability)->AbilityFinished(reasonAbilityEnded);

			/// Crucial that ClearAbility is called!
			/// Gas works by keeping abilities for recall, whereas my system was built off GameplayTasks, which are destroyed an unavailable on finish
			ClearAbility(Spec.Handle);
		}
	}
}

void UPurposeAbilityComponent::EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TArray<TObjectPtr<UGA_PurposeBase>> abilitiesToExclude)
{
	TArray<FGameplayAbilitySpec> abilitySpecs = ActivatableAbilities.Items;
	for (FGameplayAbilitySpec& Spec : abilitySpecs)
	{
		///if (ability->IsA(inClass)) In this case we may actually want to be specific, and not affect derived classes
		/// Design: Ability Purpose Feedback; In the future, we may consider allowing this to use IsA, and affect derived ability classes
		if (Spec.IsActive() && Spec.Ability->GetClass()->IsChildOf(inClass) && !abilitiesToExclude.Contains(Spec.Ability))
		{
			Global::Log(DATADEBUG, TASK, *this, "EndAbilitiesOf", TEXT("Ending ability: %s for %s"), *Spec.Ability->GetName(), *Global::EnumValueOnly<EAbilityPurposeFeedback>(reasonAbilityEnded));
			Cast<UGA_PurposeBase>(Spec.Ability)->AbilityFinished(reasonAbilityEnded);

			/// Crucial that ClearAbility is called!
			/// Gas works by keeping abilities for recall, whereas my system was built off GameplayTasks, which are destroyed an unavailable on finish
			ClearAbility(Spec.Handle);
		}
	}
}

void UPurposeAbilityComponent::EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TSubclassOf<UGA_PurposeBase> abilitiesToExclude)
{
	TArray<FGameplayAbilitySpec> abilitySpecs = ActivatableAbilities.Items;
	for (FGameplayAbilitySpec& Spec : abilitySpecs)
	{
		///if (ability->IsA(inClass)) In this case we may actually want to be specific, and not affect derived classes
		/// Design: Ability Purpose Feedback; In the future, we may consider allowing this to use IsA, and affect derived ability classes
		if (Spec.IsActive() && Spec.Ability->GetClass()->IsChildOf(inClass) && Spec.Ability->GetClass() != abilitiesToExclude)
		{
			Global::Log(DATADEBUG, TASK, *this, "EndAbilitiesOf", TEXT("Ending ability: %s for %s"), *Spec.Ability->GetName(), *Global::EnumValueOnly<EAbilityPurposeFeedback>(reasonAbilityEnded));
			Cast<UGA_PurposeBase>(Spec.Ability)->AbilityFinished(reasonAbilityEnded);

			/// Crucial that ClearAbility is called!
			/// Gas works by keeping abilities for recall, whereas my system was built off GameplayTasks, which are destroyed an unavailable on finish
			ClearAbility(Spec.Handle);
		}
	}
}

FGameplayAbilitySpec UPurposeAbilityComponent::GiveAbilityDuplicate(FGameplayAbilitySpec Spec)
{
	if (!IsValid(Spec.Ability))
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called with an invalid Ability Class."));

		return FGameplayAbilitySpec();
	}

	if (!IsOwnerActorAuthoritative())
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called on ability %s on the client, not allowed!"), *Spec.Ability->GetName());

		return FGameplayAbilitySpec();
	}

	// If locked, add to pending list. The Spec.Handle is not regenerated when we receive, so returning this is ok.
	if (AbilityScopeLockCount > 0)
	{
		AbilityPendingAdds.Add(Spec);
		return Spec;
	}

	ABILITYLIST_SCOPE_LOCK();
	FGameplayAbilitySpec& OwnedSpec = ActivatableAbilities.Items[ActivatableAbilities.Items.Add(Spec)];

	if (OwnedSpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		// Create the instance at creation time
		///CreateNewInstanceOfAbility(OwnedSpec, Spec.Ability);
		///But instead of utilizing provided method, we want to instead duplicate the ability so that we can utilize any properties set in ability asset ability came from

		AActor* Owner = GetOwner();
		check(Owner);

		UGameplayAbility* AbilityInstance = DuplicateObject<UGameplayAbility>(OwnedSpec.Ability, Owner);
		check(AbilityInstance);

		OwnedSpec.Ability = AbilityInstance;/// We have set the specs ability to the duplicate
		/// Else it will attempt to modify the Task Asset ability!
		/// This is actually supposed to be the CDO of the ability
		/// In the future they may make it const, so we may need a better method of giving the ability from the purpose asset

		// Add it to one of our instance lists so that it doesn't GC.
		///TODO: AI Purpose Replication; AddReplicatedInstanceAbility is no longer exported, and cannot be called
	/*	if (AbilityInstance->GetReplicationPolicy() != EGameplayAbilityReplicationPolicy::ReplicateNo)
		{
			OwnedSpec.ReplicatedInstances.Add(AbilityInstance);
			AddReplicatedInstancedAbility(AbilityInstance);
		}
		else
		{*/
			OwnedSpec.NonReplicatedInstances.Add(AbilityInstance);
		//}
	}

	OnGiveAbility(OwnedSpec);
	MarkAbilitySpecDirty(OwnedSpec, true);

	return OwnedSpec;
}

#pragma endregion

#pragma region Ability System Overrides

void UPurposeAbilityComponent::OnGameplayTaskActivated(UGameplayTask& Task)
{
	Super::OnGameplayTaskActivated(Task);


}

void UPurposeAbilityComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (IsOwnerActorAuthoritative())
	{
		InitializeDataForOwner();
	}
}


#pragma endregion

#pragma region Purpose

bool UPurposeAbilityComponent::ProvidePurposeToOwner(const FContextData& purposeToStore)
{
	/// Since we already establish an existing behavior purpose score as the high score for purpose selection, do we need to check for similarity?
		/// If anything we should only check is the behavior is the same
	/*if (ObjectivesAreSimilar(purposeToStore, CurrentBehavior()))
	{
		Global::Log(DATADEBUG, PURPOSE, *this, "ProvidePurposeToOwner", TEXT("Incoming purpose %s already being performed by %s.")
			, *purposeToStore.GetName()
			, *GetOwner()->GetName()
		);
		return false;
	}*/
	
	if 
	(
		purposeToStore.HasSubject(ESubject::EventTarget) && CurrentBehavior().HasSubject(ESubject::EventTarget)
		&& purposeToStore.Subject(ESubject::EventTarget) == CurrentBehavior().Subject(ESubject::EventTarget) /// If the target
		&& purposeToStore.purpose.behaviorAbility == CurrentBehavior().purpose.behaviorAbility)/// And the ability are both the same
		///Then we don't want to restart behavior
	{
		Global::Log(DATADEBUGTRIVIAL, PURPOSE, *this, "ProvidePurposeToOwner", TEXT("Incoming purpose %s already being performed by %s.")
			, *purposeToStore.GetName()
			, *GetOwner()->GetName()
		);
		return false;
	}
	
	Global::Log(DATAESSENTIAL, PURPOSE, *this, "ProvidePurposeToOwner", TEXT("Providing %s with Behavior: %s.")
		, *GetOwner()->GetName()
		, *purposeToStore.purpose.descriptionOfPurpose
	);

	EndCurrentBehavior();/// 

	SetCurrentBehavior(purposeToStore);///Set the current objective of the actor

	PerformAbility(CurrentBehavior(), CurrentBehavior().purpose.behaviorAbility, (uint8)1);

	return true;
}

void UPurposeAbilityComponent::EndCurrentBehavior()
{
	if (!CurrentBehavior().ContextIsValid())
	{
		Global::Log(DATAESSENTIAL, BEHAVIOR, *this, "EndCurrentBehavior", TEXT("currentBehavior for %s invalid!")
			, *GetOwner()->GetName()
		);
		return;/// If the Objective is invalid, it likely means it comes from a nullptr of FActorData::currentBehavior
		/// Which is nullified when currentBehavior begins clean up
		/// Because AbilityFinished will be called for every Ability in the Objective, which will tell the Objective to clean up
	}

	Global::Log(DATAESSENTIAL, BEHAVIOR, *this, "EndCurrentBehavior", TEXT("Ending Abilities of currentBehavior %s for %s.")
		, *CurrentBehavior().GetName()
		, *GetOwner()->GetName()
	);

	/// Crucial that on finished data adjustment is made if needed
	CurrentBehavior().AdjustDataIfPossible(EPurposeSelectionEvent::OnFinished, BEHAVIOR, "EndObjective");

	if (CurrentBehavior().purpose.behaviorAbility)
	{
		Global::Log(DATADEBUG, PURPOSE, *this, "EndCurrentBehavior", TEXT("Ending %s for %s.")
			, *CurrentBehavior().purpose.behaviorAbility->GetName()
			, *GetName()
		);
		EndAbilitiesOf(CurrentBehavior().purpose.behaviorAbility->GetClass(), EAbilityPurposeFeedback::InterruptedForNewObjective);/// This could lead to a recursed call to AbilityFinished() without Ability()->FeedbackState()
		/// Bugwatch: Abilities Purpose Feedback; If abilityFeedbackState is not set in AbilityFinished() this could lead to an infinite loop of AbilityFinished() -> EndObjective() -> AbilityFinished()
	}
	else
	{

		Global::LogError(PURPOSE, * this, "EndCurrentBehavior", TEXT("Behavior from %s for %s is invalid!")
			, *CurrentBehavior().GetName()
			, *GetName()
		);
	}

	SetCurrentBehavior(FContextData());/// Ensure the previous objective is no longer referenced
}

void UPurposeAbilityComponent::ActionPerformed(UGameplayAbility* Ability)
{
	if (!IsValid(Ability))
	{
		Global::Log(DATADEBUG, BEHAVIOR, *this, "ActionPerformed", TEXT("Ability invalid!"));
		return;
	}

	if (!Ability->IsActive())
	{
		Global::Log(DATADEBUG, BEHAVIOR, *this, "ActionPerformed", TEXT("Ability no longer active!"));
		return;
	}

	/*if (!director.IsValid())
	{
		Global::Log(DATADEBUG, BEHAVIOR, *this, "ActionPerformed", TEXT("Director invalid!"));
		return;
	}*/

	Global::Log(DATATRIVIAL, BEHAVIOR, *this, "ActionPerformed", TEXT("%s is performing %s.")
		, *GetOwner()->GetName()
		, *Ability->GetName()
	);

	/// Ensure the behavior history is updated
	if (FGameplayAbilitySpec* spec = Ability->GetCurrentAbilitySpec())
	{
		if (spec->GetDynamicSpecSourceTags().Num() > 0)
		{
			if (FContextualHistoryChunk* history = FindOrAddData<FContextualHistoryChunk>(TAG_Data_ContextualHistory))
			{
				for (int i = 0; i < spec->GetDynamicSpecSourceTags().Num(); ++i)
				{
					history->AddToValue(spec->GetDynamicSpecSourceTags().GetByIndex(i));
				}
			}
		}
	}

	if (!activatedAbilityTimeCache.Contains(Ability->GetClass()))
	{
		activatedAbilityTimeCache.Add(Ability->GetClass(), FDateTime::UtcNow());
	}
	else/// If we have a history of occurrences from this ability, we need to ensure that we don't create too many unnecessary occurrences from it (primarily for weapon fire abilities)
	{
		FTimespan timeSinceLastActivation = FDateTime::UtcNow() - activatedAbilityTimeCache[Ability->GetClass()];
		Global::Log(DATATRIVIAL, PURPOSE, GetName(), "ActionPerformed", TEXT("Time since last %s activation: %f."), *Ability->GetName(), timeSinceLastActivation.GetTotalSeconds());

		if (FMath::Abs(timeSinceLastActivation.GetTotalSeconds()) < occurenceTimerMin)
		{
			return;/// Since the time since last activation is less than min, no occurrence
		}

		activatedAbilityTimeCache[Ability->GetClass()] = FDateTime::UtcNow();
	}
	
	/// SequentialActions abilities activation will be ignored if they are activated within minimum time
	/// But there is an occurrence for each action, so the initial activation occurrence can safely be ignored

	/// Establish an occurrence for the gameplay ability
	///Design: GAS GameplayEvents Occurrences; An occurrence is "something happened, with an instigator, a target and tags", which is FGameplayEventData
	/// Raising occurrences from SendGameplayEventToActor / HandleGameplayEvent would let abilities, cues and occurrences share one event path,
	/// and EventTag + InstigatorTags/TargetTags could become context data without hand-building it here
	FSubjectMap subjectMap = CurrentBehavior().subjectMap;
	FDataMap contextData = CurrentBehavior().contextData;
		
	subjectMap.Add(ESubject::Instigator, this);

	if (CurrentBehavior().HasSubject(ESubject::Instigator))
	{
		subjectMap.Add(ESubject::EventTarget, CurrentBehavior().Subject(ESubject::Instigator).Get());/// Instigator of occurrence which resulted in this behavior is the target
	}

	contextData.Set(FTimeOfOccurrenceChunk());/// Defaults to now

	//PurposeSystem::Occurrence(subjectMap, contextData, director->GetPurposeEvaluationThreads());

}

void UPurposeAbilityComponent::AbilityHasFinished(const EAbilityPurposeFeedback reasonAbilityEnded)
{
	//inContext.AdjustDataIfPossible(inContext.purpose.DataAdjustments(), EPurposeSelectionEvent::OnFinished, TASK, "ActorFinishedAbility", this);

	/// Either the Ability was ended because a new Objective was selected over previous, and previous Objective's Abilities are being ended
	/// Or we received a new Ability to perform which has OverlappingResources and is taking precedence over this Ability
	bool bShouldNotSeekNewPurpose =
		reasonAbilityEnded == EAbilityPurposeFeedback::InterruptedForNewObjective
		|| reasonAbilityEnded == EAbilityPurposeFeedback::InterruptedByOverlappingResources
		|| reasonAbilityEnded == EAbilityPurposeFeedback::InterruptedByDeath /// In this case we don't have a new purpose already selected, but character is entering death state
		;

	if (bShouldNotSeekNewPurpose)
	{
		return;
	}

	/// Actually, we are only going to go through EndCurrenBehavior when a new behavior is provided to the AI
		/// This way, the data of the current behavior remains relevant to the next behavior selection when a behavior is uninterrupted
	//EndCurrentBehavior();///Ensure previous Objective is ending
	/// However, since behavior has ended, we need to ignore the score by setting it to 0 so it does not affect new purpose selection
	CurrentBehavior().cachedScoreOfPurpose = 0;

	SeekNewBehavior();/// If the ability ended uninterrupted, then we need to find a new activity for the candidate, as we don't want to rely on an Occurrence happening that the candidate may ignore
}

void UPurposeAbilityComponent::SeekNewBehavior()
{
	/*if (!director.IsValid())
	{
		Global::LogError(MANAGEMENT, GetOwner()->GetName(), "SeekNewBehavior", TEXT("Director is invalid!"));
		return;
	}*/

	/// Refactor: Purpose Activities Occurrences Behaviors; We need to remove the separation of activity and purpose evaluation. 
		/// Activities should be considered alongside purposes, their criteria should be the deciding factor just the same as other purposes
		/// Otherwise we're going to have a very tough time with utilizing event targets from previous behaviors
		/// However, the idea of occurrence targets is built around an action reaction
		/// By re utilizing a previous event target we're no longer reacting to the action against a target
		/// But how else would we handle behaviors reaching a natural end?
		/// The candidate can not be the instigator as the occurrence will be ignored for that candidat
		/// What should be done:
			///	Event targets are maintained until replaced
				///	Perhaps we have activities clear targets?
			/// Activities are considered alongside Events
			///	We can then remove PreviousEventTarget
			/// But what if 10 minutes down the line event target is still the same?
				/// The criteria should still cover whether the target is relevant right?

	/// We want to utilize either the current event target, or if not contained, the previous event target
		/// Actually, if we do this we should just re-set event target, it's doing the same thing
		/// But we initially avoided that because once acquired, a target would never be lost and an activity could never occur
	ESubject subjectToSetAsPreviousEventTarget = 
		CurrentBehavior().HasSubject(ESubject::EventTarget) ? ESubject::EventTarget 
		: CurrentBehavior().HasSubject(ESubject::PreviousEventTarget) ? ESubject::PreviousEventTarget
		: ESubject::None;

	/// If we have a current behavior, then we want to utilize the target subject from that behavior
	if (CurrentBehavior().ContextIsValid() && subjectToSetAsPreviousEventTarget != ESubject::None)
	{
		FSubjectMap subjects;
		subjects.Add(ESubject::PreviousEventTarget, CurrentBehavior().Subject(subjectToSetAsPreviousEventTarget).Get());/// We use a previous Event Target, because otherwise we will never lose a target until a new occurrence replaces the current target
		
		FDataMap contextData = CurrentBehavior().contextData;

		/// For every Activity create an Occurrence with for AI to evaluate
		//PurposeSystem::Occurrence(TArray<TObjectPtr<UPurposeAbilityComponent>>({ this }), subjects, contextData, director->GetPurposeEvaluationThreads());
	}
	else/// Otherwise we should get a behavior with no ties to the previous(current)
	{
		///director->ActivityOccurrences();///Hopefully AI are getting behaviors from Occurrences of other AI regularly and this seldom has to occur.
		/// Because this recreates occurrences for every activity in the level
		/// And while the activities are now cached, that could add a huge heap of occurrences
	}
}

#pragma endregion

