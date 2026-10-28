// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "NativeGameplayTags.h"
#include "Data.generated.h"

class UAbilitySystemComponent;

#pragma region Datachunk

USTRUCT(BlueprintType)
/// <summary>
/// FDataChunk is the backbone of the data system.
/// It allows us to abstract all data types into a single recognizable form.
/// This allows data to be easily passed around and stored,
/// While also allowing derivatives to determine how best to handle themselves.
///
/// Chunks are values, not objects. They live inside an FInstancedStruct (see FDataMapEntry),
/// which gives the same editor workflow as EditInlineNew: pick a chunk type from a dropdown and its fields appear.
/// Copying a chunk copies its data, so a snapshot handed to the evaluation thread can never be changed by the game thread.
///
/// A chunk's NAME is its Key (a gameplay tag), a chunk's LOGIC is its struct type.
/// Conditions target a base type (FFloatChunk) and work for any struct derived from it.
/// Named chunk types (FMoraleChunk) set a default Key in their constructor, so selecting the type fills in its name.
/// </summary>
struct AO_API FDataChunk
{
	GENERATED_BODY()

	FDataChunk() {}
	explicit FDataChunk(FGameplayTag inKey) : Key(inKey) {}
	virtual ~FDataChunk() {}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data", meta = (Categories = "Data", DisplayPriority = "0"))
	/// The precise name of this chunk. Conditions, modifiers and tasks find chunks by this key
	/// Only one chunk per key may exist in a DataMap
	///Design: Data Keys Validation Registry; Nothing stops Data.Morale being authored as a vector chunk, or a float condition pointing at a vector key
	/// Lookups fail safely (typed Find returns nullptr and logs), but a UDeveloperSettings table of key -> expected chunk type would let
	/// IsDataValid on purpose assets/spawners flag mismatches in editor, and could filter each condition's key picker to compatible keys
	FGameplayTag Key;

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	/// Must be safe on the evaluation thread: only read this chunk's own fields
	virtual bool HasValue() const { return false; }

	/// Game thread only. Called on the copy FDataMap::Snapshot() makes for the evaluation thread
	/// Chunks whose value comes from the world (actor location, velocity) copy it into plain data here,
	/// so evaluation never reads live actors off the game thread. Most chunks do nothing
	virtual void ResolveForSnapshot() {}

	/// Game thread only. Replaces UDataChunk::NonOverloadInitialize
	/// Called when a DataMap is handed to an owner, so chunks authored in editor (which can't reference a spawned actor) can bind to that owner
	/// @param inAvatar: the actor the owner represents (the pawn for a PurposeAbilityComponent, the actor itself otherwise)
	/// @param inAbilitySystem: the owner's AbilitySystemComponent, if it has one
	virtual void InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem) {}

	/// Copy the value of a chunk of a DIFFERENT type into this one, keeping this chunk's Key
	/// Same-type copies are handled by FDataMapEntry::CopyValueFrom, so only override this for conversions (actor reference -> actor location, etc.)
	/// @return bool: false when the source can't convert into this chunk
	virtual bool CopyValueFrom(const FInstancedStruct& source) { return false; }

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include the key
	virtual FString Description() const
	{
		return TEXT("Key: ") + Key.ToString() + TEXT(" ");
	}
};

#pragma endregion DataChunk

#pragma region Enums

UENUM(BlueprintType)//"BlueprintType" is essential to include
///Subjects are a way linking an identifier to a DataMap object for use with ContextData
enum class ESubject : uint8
{
	None UMETA(DisplayName = "None"), ///Default to ensure data is set with HasData()
	Instigator UMETA(DisplayName = "Event Instigator"), ///The owner/initiator of a context which will be Instigator/owner of this context
	EventTarget UMETA(DisplayName = "Target of Event"), ///The target of the instigator at time of occurrence
	PreviousEventTarget UMETA(DisplayName = "Previous Event Target"), ///The target of the instigator of the previous Event Candidate engaged in
	Context UMETA(DisplayName = "Context"), ///Signifies data specific to the context, rather than a character, used for occurrences
	Candidate UMETA(DisplayName = "Candidate"), ///Represents an entity which is the current evaluee of a purpose, generally the primary subject for purpose conditions
	MAX UMETA(Hidden) /// Count of subjects, used to size FSubjectDataView. Keep last
};

UENUM(BlueprintType)//"BlueprintType" is essential to include
///Conditionals are utilized by UConditions to specify how to evaluate one ESubject against another in ContextData
enum class EConditional : uint8
{
	None UMETA(DisplayName = "None") ///Default to ensure data is set with HasData()
};

#pragma endregion Enums

#pragma region Global

#define ECC_Grippable ECollisionChannel::ECC_GameTraceChannel6
#define ECC_GrippableTrace UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_GameTraceChannel6)

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Primary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Secondary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Primary_Grip);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Secondary_Grip);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Primary_Release);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Secondary_Release);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Primary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Primary_Deny);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Secondary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Secondary_Deny);
/// Basically attach object relative to current position to hand
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Free);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GripType_Free_Deny);
/// If primary grip is released, then release all grips no matter what
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DropType_OnPrimaryGripRelease_ReleaseAllGrips);
/// If primary grip is released, then release only that grip in case there is still another grip
/// Should switch other grip to primary?
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DropType_OnPrimaryGripRelease_ReleaseOnlyThisGrip);
/// If secondary grip is released, then release all grips no matter what (Like if an object requires two grips at all times)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DropType_OnSecondaryGripRelease_ReleaseAllGrips);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DropType_OnSecondaryGripRelease_ReleaseOnlyThisGrip);

#pragma endregion
