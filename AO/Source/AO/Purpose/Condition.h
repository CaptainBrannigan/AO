// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveFloat.h"
#include "Data/GlobalLog.h"
#include "Condition.generated.h"

AO_API DECLARE_LOG_CATEGORY_EXTERN(LogCondition, Log, All);

/// <summary>
/// What conditions read: one snapshot DataMap per subject (Context included, as an ordinary subject)
/// The maps are owned by the FOccurrence being evaluated, this only points at them, so building one per candidate is free
/// Everything reachable from here is a value copy, which is what makes condition evaluation safe on the evaluation thread
/// </summary>
struct AO_API FSubjectDataView
{
	const FDataMap* Find(ESubject inSubject) const { return maps[static_cast<uint8>(inSubject)]; }
	void Set(ESubject inSubject, const FDataMap* inMap) { maps[static_cast<uint8>(inSubject)] = inMap; }

	template<typename T = FDataChunk>
	const T* FindChunk(ESubject inSubject, FGameplayTag inKey) const
	{
		const FDataMap* map = Find(inSubject);
		return map ? map->Find<T>(inKey) : nullptr;
	}

private:
	const FDataMap* maps[static_cast<uint8>(ESubject::MAX)] = {};
};

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, ClassGroup = "Conditions", DefaultToInstanced, CollapseCategories)
/// <summary>
///UConditions, using DataChunks, establish a criteria which ContextDatas will be evaluated against
///in order to determine the most suitable purpose for that context
///
/// Conditions are evaluated on the purpose evaluation thread, so scoring is a C++ virtual (Evaluate) that only reads the FSubjectDataView
/// Blueprint subclasses are fine for CONFIGURATION (preset keys, weights, curves), but can't override scoring:
/// Blueprint code assumes the game thread, and the previous BlueprintNativeEvent ran it on the evaluation thread
/// </summary>
///Design: Conditions Blueprint Iteration; If BP-authored scoring is wanted for iteration, add a UCondition_Blueprint whose BlueprintImplementableEvent
/// is evaluated on the game thread (the occurrence is queued back to the game thread for that condition, then returns to the worker)
/// It would be slower, so it's meant for prototyping a condition before writing it in C++
class AO_API UCondition : public UObject
{
	GENERATED_BODY()
public:

#pragma region BaseCondition

	UCondition();

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Condition Description", meta = (DisplayPriority = "1"))
	///Helper text for understanding a condition
	FText description;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "8", ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	///Determines the magnitude of the condition
	float weight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "9"))
	/// Whether this purpose should score 0 if this condition score is 0
	bool isRequired = false;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Subject To Compare", meta = (DisplayPriority = "2"))
	///The primary subject to seek for comparison
	ESubject primarySubjectType = ESubject::None;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Primary Data", meta = (DisplayPriority = "3", Categories = "Data"))
	///The key of the chunk to be evaluated on the primary subject (replaces PrimaryData())
	///The chunk type is decided by the condition class: a float condition only finds float chunks under this key
	///If it is not found then the score will be 0
	FGameplayTag primaryKey;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Subject To Compare Against", meta = (DisplayPriority = "6", EditCondition="bHasSecondSubject == true", EditConditionHides))
	///If desired, a subject to compare data against the primary subject
	///Leave as NONE to compare the primary subject data against a specified value
	ESubject secondarySubjectType = ESubject::None;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Secondary Data", meta = (DisplayPriority = "7", Categories = "Data", EditCondition = "bHasSecondSubject == true", EditConditionHides))
	///The key of the chunk to compare against (replaces SecondaryData())
	///With a secondary subject: leave empty to read the primary key from the secondary subject
	///Without a secondary subject: read from the primary subject, or leave empty to compare against a value on the condition
	FGameplayTag secondaryKey;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Max", meta = (DisplayPriority = "15", EditCondition = bHasRange, EditConditionHides))
	///Establishes a max of a bookend for which values are normalized within
	///Max should always be 1 for normalization purposes
	float max = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Score is clamped by Max", meta = (DisplayPriority = "16", EditCondition = bHasRange, EditConditionHides))
	/// if a condition has a score ceiling, if score is above max score becomes 0
	bool bHasScoreCeiling = true;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Min", meta = (DisplayPriority = "17", EditCondition = bHasRange, EditConditionHides))
	///Establishes a min of a bookend for which values are normalized within
	///If max == 0 || max !> min, a boolean type value of 0 or 1 will be returned
	float min = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Score is clamped by Min", meta = (DisplayPriority = "18", EditCondition = bHasRange, EditConditionHides))
	/// if a condition has a score floor, if score is below min score becomes 0
	bool bHasScoreFloor = true;

protected:
	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Response Curve", meta = (DisplayPriority = "19"))
	///By default, conditions are scored on a linear response curve
	///Here an alternate response curve may be applied to a curve
	///The min and max bookends are utilized to normalize a score between 0-1
	TObjectPtr<UCurveFloat> responseCurve = nullptr;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Unique Response Curve", meta = (DisplayPriority = "20"))
	///An alternate means of establishing a response curve unique to this condition
	///This curve will be used in place of a response curve asset if keys are present
	///The min and max bookends are utilized to normalize a score between 0-1
	FRichCurve userResponseCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Condition Data", DisplayName = "Value Can be Scored By Range", meta = (DisplayPriority = "21"))
	/// Decides whether condition has numbervalues that can  have a min/max
	bool bHasRange = false;

	UPROPERTY(EditDefaultsOnly, Category = "Condition Data", DisplayName = "Condition has a Subject to Compare Against", meta = (DisplayPriority = "22"))
	/// If we don't want a secondary subject for a condition, set this to false and the option to select a second subject will be unavailable in the property editor
	bool bHasSecondSubject = true;

public:
	///Normalizes a score "x" between the "min-max" bookends of the condition
	float Normalize(float x) const { return FMath::Clamp((FMath::Abs(x) - FMath::Abs(min)) / (FMath::Abs(max) - FMath::Abs(min)), 0.f, 1.f); }

	///Retrieve data specified for the condition from the subjects' snapshots
	///Translate retrieved data into a score
	/// Safe on the evaluation thread: reads only the view and this condition's own properties
	/// @param data: The snapshot DataMap of every subject of the occurrence, Context and Candidate included
	float InitiateEvaluation(const FSubjectDataView& data) const
	{
		if (primarySubjectType == ESubject::None || !primaryKey.IsValid())
		{
			Global::LogError(LogCondition, GetName(), "InitiateEvaluation", TEXT("Data not retrieved for Description: %s. Primary Subject: %s, Primary Key: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, *primaryKey.ToString()
			);
			return 0.0f;///No further logic is necessary, just return 0 so the score of the condition will be 0
		}

		return Evaluate(data);
	}

	///Test: Condition Purpose PurposeEvaluation; ensure that user specified min-max values actually affect score correctly
	///@return float = if a curve has been setup for condition, the inValue position on curve, else inValue
	float AdjustToCurve(float inValue) const
	{
		if (IsValid(responseCurve.Get()))
		{
			Global::Log(CALLTRACETRIVIAL, PURPOSE, GetName(), "AdjustToCurve", TEXT("Response Curve is valid."));

			if ((bHasScoreFloor && inValue < min) || (bHasScoreCeiling && inValue > max))
			{
				return 0.0f;
			}

			float normalizedScore = Normalize(inValue);/// we normalize between min and max so that we can easily score along curve without adjusting the curve
			float scoreAlongCurve = responseCurve->GetFloatValue(normalizedScore);

			Global::Log(DATATRIVIAL, LogCondition, description.ToString(), "AdjustToCurve", TEXT("Invalue = %f. NormalizedScore: %f. Score along curve = %f."), inValue, normalizedScore, scoreAlongCurve);
			return scoreAlongCurve;
		}
		else/// if neither curves are present, then normalize the value between min and max, which will appear as a linear response curve
		{
			Global::Log(CALLTRACETRIVIAL, PURPOSE, description.ToString(), "AdjustToCurve", TEXT("No response curves."));

			if ((bHasScoreFloor && inValue < min) || (bHasScoreCeiling && inValue > max))
			{
				return 0.0f;
			}

			float normalizedScore = Normalize(inValue);
			Global::Log(DATATRIVIAL, PURPOSE, description.ToString(), "AdjustToCurve", TEXT("Invalue = %f. Score normalized = %f."), inValue, normalizedScore);
			return normalizedScore;
		}
	}

protected:

	/// Override in derived conditions to turn the retrieved chunks into a value, which AdjustToCurve then turns into a 0-1 score
	/// Runs on the evaluation thread: only read data and this condition's properties, never live objects
	virtual float Evaluate(const FSubjectDataView& data) const PURE_VIRTUAL(UCondition::Evaluate, return 0.0f;);

	/// Replaces the RetrieveConditionDataChunks overloads
	/// @return nullptr if the subject is missing, the key is missing, or the chunk isn't a T
	template<typename T>
	const T* GetChunk(const FSubjectDataView& data, ESubject inSubject, FGameplayTag inKey) const
	{
		if (inSubject == ESubject::None || !inKey.IsValid())
		{
			return nullptr;
		}

		const FDataMap* map = data.Find(inSubject);
		if (!map)
		{
			Global::Log(DATADEBUG, LogCondition, GetName(), "GetChunk", TEXT("Subject not retrieved for Description: %s. Subject: %s, Key: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(inSubject)
				, *inKey.ToString()
			);
			return nullptr;
		}

		const FDataMapEntry* entry = map->FindEntry(inKey);
		if (!entry)
		{
			Global::Log(DATADEBUG, LogCondition, GetName(), "GetChunk", TEXT("Data not retrieved for Description: %s. Subject: %s, Key: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(inSubject)
				, *inKey.ToString()
			);
			return nullptr;
		}

		const T* chunk = entry->Get<T>();
		if (!chunk)
		{
			Global::LogError(LogCondition, GetName(), "GetChunk", TEXT("Description: %s. Key %s on %s is a %s, but the condition reads %s!")
				, *description.ToString()
				, *inKey.ToString()
				, *Global::EnumValueOnly<ESubject>(inSubject)
				, entry->Chunk.GetScriptStruct() ? *entry->Chunk.GetScriptStruct()->GetName() : TEXT("invalid chunk")
				, *T::StaticStruct()->GetName()
			);
		}
		return chunk;
	}

	/// The chunk of primaryKey on the primary subject
	template<typename T>
	const T* Primary(const FSubjectDataView& data) const
	{
		return GetChunk<T>(data, primarySubjectType, primaryKey);
	}

	/// With a secondary subject: secondaryKey (or primaryKey if empty) on the secondary subject
	/// Without one: secondaryKey on the primary subject, or nullptr if empty so the derived condition uses its own value
	template<typename T>
	const T* Secondary(const FSubjectDataView& data) const
	{
		if (secondarySubjectType != ESubject::None)
		{
			return GetChunk<T>(data, secondarySubjectType, secondaryKey.IsValid() ? secondaryKey : primaryKey);
		}

		return secondaryKey.IsValid() ? GetChunk<T>(data, primarySubjectType, secondaryKey) : nullptr;
	}

#pragma endregion

};
