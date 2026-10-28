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
	///Design: GAS Cooldowns Occurrences; This is a per-ability cooldown on raising occurrences
	/// A GameplayEffect cooldown can't gate it directly (it gates activation, not the occurrence), but a tag granted for occurenceTimerMin by an
	/// instant-applied duration effect (Cooldown.Occurrence.<ability tag>) would make it visible in GAS debugging and tunable per ability
	TMap<TSubclassOf<class UGameplayAbility>, FDateTime> activatedAbilityTimeCache;

	/// Limits how often an occurrence from a single ability can trigger
	float occurenceTimerMin = 0.25f;

#pragma region Datamap Interface
public:

	const FDataMap& DataMap() const final { return data; }

	/// Marks data dirty for push model replication (Iris and generic replication both use it)
	void OnDataMapChanged() final;

	/// Binds actor/attribute chunks to this component's avatar and ASC (see FDataChunk::InitializeForOwner)
	/// Call after the avatar is set, and after any data is appended from a spawner
	void InitializeDataForOwner();

private:

	UPROPERTY(Replicated, EditAnywhere, Category = "Data")
	///This data is representative of the subject "ESubject::Candidate"
	///Design: Network Replication FastArray; The whole array is compared and sent when any chunk changes
	/// If per-AI data changes often, FDataMap can become an FFastArraySerializer (FDataMapEntry : FFastArraySerializerItem) so only changed entries are sent
	/// Lyra's FGameplayTagStackContainer (LyraGame/System/GameplayTagStack.h) is the pattern
	/* TODO: Data Defaults DataAsset Characters; Default chunks are authored per component/spawner instance, so subjects miss chunks conditions expect (Data.ViewPoint, Data.Actor)
	Make a UPrimaryDataAsset (UCharacterDataDefaults) holding an FDataMap of default chunks plus an optional parent asset, with a shared base and AI/Player children
	Base: Location, ViewPoint, Actor, Velocity, ContextualHistory. AI: Role AI, Morale, LevelOfAlarm. Player: Role Player
	The component takes the asset and AppendMissing's it (parent first) before InitializeDataForOwner, so per-instance chunks still override
	AISpawn's FAISpawnParameters could reference one too, replacing the location chunk it sets in its constructor */
	FDataMap data;

	/// Force implementers to provide an editable datamap
	/// Only reachable through IDataMapInterface, so every change marks data dirty
	FDataMap& MutableDataMap() final { return data; }

#pragma endregion

#pragma region Gameplay Abilities
public:
	/// @param abilityClass: UPurposeAbilityComponent will create an Initialize the abilityClass and immediately attempt to run
	void PerformAbility(FContextData& inContext, TSubclassOf<UGA_PurposeBase> abilityClass);

	/// @param ability: Existing Ability will be duplicated and performed, bypassing the need for custom initializations and allowing ability specific properties editable in editor
	///Design: GAS AbilitySpec SourceObject; Duplicating the inline ability template is what gives per-purpose ability settings
	/// GAS's intended route is a class + FGameplayAbilitySpec::SourceObject (the purpose asset) or SetByCaller/GameplayEventData for per-use parameters,
	/// which keeps instancing standard and avoids GiveAbilityDuplicate reaching into ActivatableAbilities (and the unexported AddReplicatedInstancedAbility)
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

	/** Binds data chunks to the new avatar (InitializeDataForOwner) */
	void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

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
