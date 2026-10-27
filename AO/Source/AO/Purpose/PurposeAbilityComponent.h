// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataMapInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GA_PurposeBase.h"
#include "PurposeEvaluationThread.h"
#include "GameFramework/Character.h"
#include "PurposeAbilityComponent.generated.h"

UCLASS()
/// <summary>
/// The purpose ability component is a marriage of the UGameplayAbilitySystem with the Purpose System
/// As a component it can be added to any actor and the management can keep track of that actor (for purpose system) via this component
/// </summary>
class AO_API UPurposeAbilityComponent : public UAbilitySystemComponent, public IDataMapInterface
{
	GENERATED_BODY()
public:

	UPurposeAbilityComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	///TODO: AI Purpose Director Occurrence; Disabling level director for now as it requires overhaul to remove Lyra logic
		/// FindAll for director to restore
	/// Enforces necessity for providing reqs to PurposeSystem
	///void InitializePurposeSystem(TObjectPtr<class ADirector_Level> inDirector);
	
	bool HasCurrentBehavior() { return currentBehaviorForOwner.ContextIsValid(); }

	FContextData& CurrentBehavior() { return currentBehaviorForOwner; }
	void SetCurrentBehavior(FContextData inContext) { currentBehaviorForOwner = inContext; }

	//TObjectPtr<ACharacter> GetOwnerCharacter() { return Cast<ACharacter>(GetOwner()); }
	TObjectPtr<ACharacter> GetOwnerCharacter() { return Cast<ACharacter>(Cast<AController>(GetOwner())->GetPawn()); }

	///@return Name of the object.
	FORCEINLINE FString GetName() const
	{
		return UObject::GetName() + "::" + (GetOwner() ? GetOwner()->GetName() : "Unknown Owner");
	}

protected:

	FContextData currentBehaviorForOwner;

	/// A storage of activated abilities to prevent occurrences every tick for individual ability activations
	TMap<TSubclassOf<class UGameplayAbility>, FDateTime> activatedAbilityTimeCache;

	/// Limits how often an occurrence from a single ability can trigger
	float occurenceTimerMin = 0.25f;

#pragma region Datamap Interface
public:

	const TArray<FDataMapEntry>& DataMap() final { return data; }
	TArray<FDataMapEntry> DataMapCopy() final { return data; }

	UFUNCTION(Server, Reliable)
	void AddData(UDataChunk* inData, bool overwriteValue = true) final;

	UFUNCTION(Server, Reliable)
	void AppendData(const TArray<FDataMapEntry>& inDataMap, bool overwriteValue = true) final;

	UFUNCTION(Server, Reliable)
	void RemoveData(TSubclassOf<UDataChunk> inClass) final;

private:

	UPROPERTY(Replicated)
	///This data is representative of the subject "ESubject::Candidate"
	TArray<FDataMapEntry> data;

	/// Force implementers to provide an editable datamap
	/// The only way to the change the DataMap is through the Server RPCs or server side calls
	TArray<FDataMapEntry>& DataMapInternal() final { return data; }

#pragma endregion

#pragma region Gameplay Abilities
public:
	/// @param abilityClass: UPurposeAbilityComponent will create an Initialize the abilityClass and immediately attempt to run
	void PerformAbility(FContextData& inContext, TSubclassOf<UGA_PurposeBase> abilityClass);

	/// @param ability: Existing Ability will be duplicated and performed, bypassing the need for custom initializations and allowing ability specific properties editable in editor
	void PerformAbility(FContextData& inContext, const TObjectPtr<UGA_PurposeBase> ability, uint8 priority);

	/// @param ability: Ability as a non-null reference, as these abilitys have already been initialized prior to being sent to abilitysmanager
	void PerformAbility(FContextData& inContext, UGA_PurposeBase& ability);

	///Refactor: Abilities Abilities Purpose; Consider whether we should be able to execute Abilities directly, or if only Abilities can perform AbilityAbilities
	void EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TObjectPtr<UGA_PurposeBase> abilityToExclude = nullptr);
	void EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TArray<TObjectPtr<UGA_PurposeBase>> abilitiesToExclude);
	void EndAbilitiesOf(TSubclassOf<UGA_PurposeBase> inClass, const EAbilityPurposeFeedback reasonAbilityEnded, TSubclassOf<UGA_PurposeBase> abilitiesToExclude);

	template<class T>
	T* GetAbility()
	{
		//for (TObjectPtr<UGA_PurposeBase> ability : KnownAbilities)
		//{
		//	/*	//Global::Log(FULLTRACE, AbilityLog, "UPurposeAbilityComponent", "GetAbility<T>", TEXT("Known Ability: %s. Sought Ability: %s.")
		//			, *ability->GetName()
		//			, *T::StaticClass()->GetName()
		//		);*/
		//	if (ability->IsA<T>())
		//	{
		//		return Cast<T>(ability);
		//	}
		//}

		return nullptr;
	}

protected:

	/// @Param Spec: Duplicates the ability belonging to Spec, then sets the Spec.Ability to the duplicated Ability
	/// @return FGameplayAbilitySpec: The ability spec added to ActivatableAbilites.Items, containing duplicated ability from Spec and new FGameplaySpecHandle
	FGameplayAbilitySpec GiveAbilityDuplicate(FGameplayAbilitySpec Spec);

#pragma endregion

#pragma region AbilitySystemComponent Overrides

	/// Overrides
	/*
	 * Grants an Ability.
	 * This will be ignored if the actor is not authoritative.
	 * Returns handle that can be used in TryActivateAbility, etc.
	 *
	 * @param AbilitySpec FGameplayAbilitySpec containing information about the ability class, level and input ID to bind it to.
	 */
	///FGameplayAbilitySpecHandle GiveAbility(const FGameplayAbilitySpec& AbilitySpec);

	/// Useful methods
		/**
	 * Clears all abilities bound to a given Input ID
	 * This will be ignored if the actor is not authoritative
	 *
	 * @param InputID The numeric Input ID of the abilities to remove
	 */
	///UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Gameplay Abilities")
	///	void ClearAllAbilitiesWithInputID(int32 InputID = 0);

	/**
	 * Removes the specified ability.
	 * This will be ignored if the actor is not authoritative.
	 *
	 * @param Handle Ability Spec Handle of the ability we want to remove
	 */
	///UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Gameplay Abilities")
		///void ClearAbility(const FGameplayAbilitySpecHandle& Handle);

	/// Debug info

	///static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
	///virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	///virtual void PrintDebug();
	///void AccumulateScreenPos(FAbilitySystemComponentDebugInfo& Info);
	///virtual void Debug_Internal(struct FAbilitySystemComponentDebugInfo& Info);
	///void DebugLine(struct FAbilitySystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset, int32 MinTextRowsToAdvance = 0);

	/** Notify called after GameplayTask changes state to Active (initial activation or resuming) */
	void OnGameplayTaskActivated(UGameplayTask& Task) override;

#pragma endregion

#pragma region Purpose
public:

	bool ProvidePurposeToOwner(const FContextData& purposeToStore);

	void EndCurrentBehavior();

	/// For instances when GA activation can not be routed through the manager, this method is bound to the inActor's AbilityActivationCallbacks
	/// Shouldn't need to unbind, as the existence of inActor (which owns the delegate this method is bound to) is dependent on this manager, who will outlive the component
	/// @param Ability: The GA that has been activated, for access to its context data
	void ActionPerformed(UGameplayAbility* Ability);

	/// Will end the current behavior and seek a new behavior if possible
	void AbilityHasFinished(const EAbilityPurposeFeedback reasonAbilityEnded);

	/// If an AI finishes an ability and an Occurrence has not provided them a suitable behavior, we want an explicit backup occurrence in order to find a new behavior
	void SeekNewBehavior();

private:

	//UPROPERTY()
	//TWeakObjectPtr<class ADirector_Level> director = nullptr;

#pragma endregion
};
