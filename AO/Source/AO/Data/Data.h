// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NativeGameplayTags.h"
#include "Data.generated.h"

#pragma region Datachunk

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew)
/// <summary>
/// UDataChunk is the backbone of the data system.
/// It allows us to abstract all data types into a single recognizable form.
/// This allows data to be easily passed around and stored,
/// While also allowing derivatives to determine how best to handle themselves.
/// In order to utilize EditInlineNew to create a DataChunk in editor, use UPROPERTY(Instanced)
/// </summary>
class AO_API UDataChunk : public UObject
{
	GENERATED_BODY()
public:

	UDataChunk() {}

	/// While it can not be overloaded, this method will provide datachunks the opportunity to access their outer object and potentially initialize themselves from there
	virtual UDataChunk& NonOverloadInitialize() { return *this; }

	///Overload this to require new object arguments
	/// This is mainly to present a consistent naming convention for providing default values to individual DataChunks
	/// Not virtual because we can't return derived types in an override
	/// The point of this method is to be called off NewObject()->Initialize()
	UDataChunk& Initialize() { return *this; }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	virtual bool HasValue()
	{
		return 0;
	}

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	//const float Value() const PURE_VIRTUAL(UFloatChunk::Value, return 0.0f;);

	/// Override this method so that datachunks specified in a purpose asset may be modified when purpose is selected
	/// @param modificationContext: What purpose selection event will lead to this data modification
	/// @param inModifier: The encapsulated modifier specified on the derived DataChunks class
	virtual TObjectPtr<UDataChunk> AdjustData(const UDataChunk& inModifier) { return this; }

	virtual void CopyDataFrom(TObjectPtr<UDataChunk> ChunkToCopyFrom) {}

	/// Override in subclasses so they may return the correct enum representing modification logic
	virtual const uint8 DataModifier() { return 0; }

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	virtual FString Description() const
	{
		return TEXT("Type: ") + GetClass()->GetName() + TEXT(" ");
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }

	/// Debug: DataChunk Network; Will these be stablynamed?
		/// Does replication work just by matching exact names?
		/// If so, we need to verify DataChunks are properly replicated
		/// They actually will, if we set the AISpawners as the parents of the management spawned in, who will be the parents of the character actors
	/** IsSupportedForNetworking means an object can be referenced over the network */
	bool IsSupportedForNetworking() const final
	{
		return true;/// We shouldn't be checking for authority here. It's my responsibility to ensure if data is to be replicated
		/// Authority is assured prior to this point.
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
	Candidate UMETA(DisplayName = "Candidate") ///Represents an entity which is the current evaluee of a purpose, generally the primary subject for purpose conditions 
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
