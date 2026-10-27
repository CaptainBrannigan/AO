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

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, ClassGroup = "Conditions", DefaultToInstanced)
/// <summary>
///UConditions, using DataChunks, establish a criteria which ContextDatas will be evaluated against
///in order to determine the most suitable purpose for that context
/// </summary>
class AO_API UCondition : public UObject
{
	GENERATED_BODY()
public:

#pragma region BaseCondition

	UCondition();

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Condition Description", meta = (DisplayPriority = "1"))
	///Helper text for understanding a condition	
	FText description;

	///Place this within lowest derived Condition (DataVsData), so that conditionals specific to that condition can exist
	//UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	/////An Enum representing how the user wishes the two data types to be compared
	//EConditional conditional;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "8", ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	///Determines the magnitude of the condition
	float weight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "9"))
	/// Whether this purpose should score 0 if this condition score is 0
	bool isRequired = false;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Subject To Compare", meta = (DisplayPriority = "2"))
	///The primary subject to seek for comparison
	ESubject primarySubjectType = ESubject::None;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Subject To Compare Against", meta = (DisplayPriority = "6", EditCondition="bHasSecondSubject == true", EditConditionHides))
	///If desired, a subject to compare data against the primary subject
	///Leave as NONE to compare the primary subject data against a specified value
	ESubject secondarySubjectType = ESubject::None;

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

		///Retrieve data specified for the condition from context
	///Translate retrieved data into a score
	/// @param context: The aray of subjects and the data maps associated to them
	/// @param owner: The IPurposeManagementInterface owner of the context can allow access to other aspects of the PurposeSystem when evaluating
	float InitiateEvaluation(const TMap<ESubject, TArray<FDataMapEntry>>& context, TWeakObjectPtr<class UPurposeAbilityComponent> owner) const
	{
		TObjectPtr<UDataChunk> primary = nullptr;
		TObjectPtr<UDataChunk> secondary = nullptr;

		///retrieve the relevant data chunks
		if (RetrieveConditionDataChunks(context, primary, secondary))
		{
			return EvaluateConditionInternal(primary, secondary, owner.Get());
		}

		return 0.0f;
	}

	UFUNCTION(BlueprintNativeEvent)
		float EvaluateConditionInternal(const UDataChunk* primaryDataChunk, const UDataChunk* secondaryDataChunk, class UPurposeAbilityComponent* owner) const;
		virtual float EvaluateConditionInternal_Implementation(const UDataChunk* primaryDataChunk, const UDataChunk* secondaryDataChunk, class UPurposeAbilityComponent* owner) const;

	UFUNCTION(BlueprintNativeEvent)
	TSubclassOf<UDataChunk> PrimaryData() const;
	///The primary data is the constant data type to be evaluated for the condition
	///If it is not found then the score will be 0
	///It can either be compared against a second data type or a value specified on the UCondition
	///Override this method with a covarient return type to represent the data specific to the condition
	FORCEINLINE virtual TSubclassOf<UDataChunk> PrimaryData_Implementation() const PURE_VIRTUAL(UCondition::PrimaryData, return nullptr;)

	UFUNCTION(BlueprintNativeEvent)
	TSubclassOf<UDataChunk> SecondaryData() const;
	///The secondary data is a data type which the primary data type will be compared against
	///A value can not be specified for secondary data, the value is retrieved from a subject
	///Override this method with a covarient return type to represent the data specific to the condition
	FORCEINLINE virtual TSubclassOf<UDataChunk> SecondaryData_Implementation() const PURE_VIRTUAL(UCondition::SecondaryData, return nullptr;)

	template<class PrimaryType, class SecondaryType>///Templated so that casting is not required on retrieved primary and secondary
	///Retrieve the DataChunks from the ContextData via primary and secondary UDataChunk types
	///@param primary: An out parameter which will hold the primary data type, the type of which is specified in derived UConditions
	///@param secondary: An out parameter which will hold the secondary data type, the type of which is specified in derived UConditions
	///@return bool == false if a the primary subject or data of the context can not be found
	bool RetrieveConditionDataChunks(const TMap<ESubject, TArray<FDataMapEntry>>& context, TObjectPtr<PrimaryType>& primary, TObjectPtr<SecondaryType>& secondary, ESubject secondarySubject) const
	{
		////Global::Log(FULLTRACE, PurposeLog, "Condition", "RetrieveConditionDataChunk", TEXT("1"));
		if (primarySubjectType == ESubject::None || !PrimaryData())
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Data not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		if (!context.Contains(primarySubjectType))
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimarySubject not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		if (!context[primarySubjectType].Contains(PrimaryData()))
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		primary = Cast<PrimaryType>(context[primarySubjectType].FindByKey(PrimaryData())->Chunk);///Find the primary DataChunk to be evaluated

		if (!primary)
		{
			/*Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary from Context nullptr. Description: %s. Subject Type: %s, Subject: %s, Type: %s, Was Found? %s. Cast To %s %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				,  IsValid(context->Subject(primarySubjectType)) ? *context->Subject(primarySubjectType)->GetName() : TEXT("Unknown")
				, *PrimaryData()->GetName()
				, IsValid(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("True") : TEXT("False")
				, *PrimaryType::StaticClass()->GetName()
				, Cast<PrimaryType>(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("success") : TEXT("failed")
			);*/
			return false;
		}

		if (secondarySubject != ESubject::None)///Then retrieve comparison data from Secondary Subject
		{
			if (!context.Contains(secondarySubject))
			{
				Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("SecondarySubject not retrieved for Description: %s. Secondary Subject: %s.")
					, *description.ToString()
					, *Global::EnumValueOnly<ESubject>(secondarySubject)
				);
				return false;///No further logic is necessary, just return false so the score of the condition will be 0
			}

			if (SecondaryData())///Then comparison data is the Secondary Data-Type from the Secondary Subject
			{

				if (!context[secondarySubject].Contains(SecondaryData()))
				{
					Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
						, *description.ToString()
						, *Global::EnumValueOnly<ESubject>(primarySubjectType)
						, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
					);
					return false;///No further logic is necessary, just return false so the score of the condition will be 0
				}

				secondary = Cast<SecondaryType>(context[secondarySubject].FindByKey(SecondaryData())->Chunk);/// Find the secondary data of secondary type from secondary subject
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Second Subject %s + DataClassName: %s"), *UEnum::GetValueAsString(secondarySubject), IsValid(SecondaryData()) ? *SecondaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
			else///Then comparison data is Primary Data-Type from Secondary Subject
			{
				if (!context[secondarySubject].Contains(PrimaryData()))
				{
					Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
						, *description.ToString()
						, *Global::EnumValueOnly<ESubject>(primarySubjectType)
						, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
					);
					return false;///No further logic is necessary, just return false so the score of the condition will be 0
				}

				secondary = Cast<SecondaryType>(context[secondarySubject].FindByKey(PrimaryData())->Chunk);/// Find the secondary data of primary type from secondary subject
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Second Subject %s +  Primary DataClassName: %s"), *UEnum::GetValueAsString(secondarySubject), IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
		}
		else///Then retrieve comparison data from Primary Subject
		{
			if (SecondaryData())//Then comparison data is the secondary Data-Type from the Primary Subject
			{
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary Subject %s +  Secondary DataClassName: %s"), *UEnum::GetValueAsString(primarySubjectType.subject), IsValid(SecondaryData()) ? *SecondaryData()->GetName() : TEXT("GetClass() = nullptr"));
				secondary = Cast<SecondaryType>(context[primarySubjectType].FindByKey(SecondaryData())->Chunk);
			}
			else///Then comparison data is the value indicated in the condition and nothing need be done
			{
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary Subject %s +  Primary DataClassName: %s"), *UEnum::GetValueAsString(primarySubjectType.subject), IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
		}

		///Refactor: Conditions Purpose; Can we make it so that this can return false without primary and secondary?
			/// The issue is that from here, we don't know what the any optional values are, as they are specified in derived classes
			/// Without that knowledge, we can't say "if secondary is invalid, use the optional value". That has to be done in the derived class
		//return IsValid(primary) && IsValid(secondary);

		return true;
	}	
		
	///Retrieve the DataChunks from the ContextData via primary and secondary UDataChunk types
	///@param primary: An out parameter which will hold the primary data, the type of which is specified in derived UConditions
	///@param secondary: An out parameter which will hold the secondary data, the type of which is specified in derived UConditions
	///@return bool == false if a the primary subject or data of the context can not be found
	bool RetrieveConditionDataChunks(const TMap<ESubject, TArray<FDataMapEntry>>& context, TObjectPtr<UDataChunk>& primary, TObjectPtr<UDataChunk>& secondary) const
	{
		////Global::Log(FULLTRACE, PurposeLog, "Condition", "RetrieveConditionDataChunk", TEXT("1"));
		if (primarySubjectType == ESubject::None || !PrimaryData())
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Data not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		if (!context.Contains(primarySubjectType))
		{
			Global::Log(DATADEBUG, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimarySubject not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		if (!context[primarySubjectType].Contains(PrimaryData()))
		{
			Global::Log(DATADEBUG, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		primary = context[primarySubjectType].FindByKey(PrimaryData())->Chunk;///Find the primary DataChunk to be evaluated

		if (!primary)
		{
			/*Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary from Context nullptr. Description: %s. Subject Type: %s, Subject: %s, Type: %s, Was Found? %s. Cast To %s %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				,  IsValid(context->Subject(primarySubjectType)) ? *context->Subject(primarySubjectType)->GetName() : TEXT("Unknown")
				, *PrimaryData()->GetName()
				, IsValid(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("True") : TEXT("False")
				, *PrimaryType::StaticClass()->GetName()
				, Cast<PrimaryType>(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("success") : TEXT("failed")
			);*/
			return false;
		}

		if (secondarySubjectType != ESubject::None)///Then retrieve comparison data from Secondary Subject
		{
			if (!context.Contains(secondarySubjectType))
			{
				Global::Log(DATADEBUG, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("SecondarySubject not retrieved for Description: %s. Secondary Subject: %s.")
					, *description.ToString()
					, *Global::EnumValueOnly<ESubject>(secondarySubjectType)
				);
				return false;///No further logic is necessary, just return false so the score of the condition will be 0
			}

			if (SecondaryData())///Then comparison data is the Secondary Data-Type from the Secondary Subject
			{

				if (!context[secondarySubjectType].Contains(SecondaryData()))
				{
					Global::Log(DATADEBUG, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
						, *description.ToString()
						, *Global::EnumValueOnly<ESubject>(primarySubjectType)
						, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
					);
					return false;///No further logic is necessary, just return false so the score of the condition will be 0
				}

				secondary = context[secondarySubjectType].FindByKey(SecondaryData())->Chunk;/// Find the secondary data of secondary type from secondary subject
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Second Subject %s + DataClassName: %s"), *UEnum::GetValueAsString(secondarySubjectType), IsValid(SecondaryData()) ? *SecondaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
			else///Then comparison data is Primary Data-Type from Secondary Subject
			{
				if (!context[secondarySubjectType].Contains(PrimaryData()))
				{
					Global::Log(DATADEBUG, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
						, *description.ToString()
						, *Global::EnumValueOnly<ESubject>(primarySubjectType)
						, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
					);
					return false;///No further logic is necessary, just return false so the score of the condition will be 0
				}

				secondary = context[secondarySubjectType].FindByKey(PrimaryData())->Chunk;/// Find the secondary data of primary type from secondary subject
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Second Subject %s +  Primary DataClassName: %s"), *UEnum::GetValueAsString(secondarySubjectType), IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
		}
		else///Then retrieve comparison data from Primary Subject
		{
			if (SecondaryData())//Then comparison data is the secondary Data-Type from the Primary Subject
			{
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary Subject %s +  Secondary DataClassName: %s"), *UEnum::GetValueAsString(primarySubjectType.subject), IsValid(SecondaryData()) ? *SecondaryData()->GetName() : TEXT("GetClass() = nullptr"));
				secondary = context[primarySubjectType].FindByKey(SecondaryData())->Chunk;
			}
			else///Then comparison data is the value indicated in the condition and nothing need be done
			{
				////Global::Log(Debug, PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary Subject %s +  Primary DataClassName: %s"), *UEnum::GetValueAsString(primarySubjectType.subject), IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("GetClass() = nullptr"));
			}
		}

		///Refactor: Conditions Purpose; Can we make it so that this can return false without primary and secondary?
			/// The issue is that from here, we don't know what the any optional values are, as they are specified in derived classes
			/// Without that knowledge, we can't say "if secondary is invalid, use the optional value". That has to be done in the derived class
		//return IsValid(primary) && IsValid(secondary);

		return true;
	}	
	
	template<class PrimaryType>///Templated so that casting is not required on retrieved primary and secondary
	///Retrieve the DataChunks from the ContextData via primary and secondary UDataChunk types
	///@param primary: An out parameter which will hold the primary data type, the type of which is specified in derived UConditions
	///@return bool == false if a the primary subject or data of the context can not be found
	bool RetrieveConditionDataChunks(const TMap<ESubject, TArray<FDataMapEntry>>& context, TObjectPtr<PrimaryType>& primary) const
	{
		////Global::Log(FULLTRACE, PurposeLog, "Condition", "RetrieveConditionDataChunk", TEXT("1"));
		if (primarySubjectType == ESubject::None || !PrimaryData())
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}
		////Global::Log(FULLTRACE, PurposeLog, "Condition", "RetrieveConditionDataChunk", TEXT("2"));


		if (!context.Contains(primarySubjectType))
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimarySubject not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		if (!context[primarySubjectType].Contains(PrimaryData()))
		{
			Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("PrimaryData not retrieved for Description: %s. Primary Subject: %s, PrimaryData of Condition: %s.")
				, *description.ToString()
				, *Global::EnumValueOnly<ESubject>(primarySubjectType)
				, IsValid(PrimaryData()) ? *PrimaryData()->GetName() : TEXT("Invalid")
			);
			return false;///No further logic is necessary, just return false so the score of the condition will be 0
		}

		primary = Cast<PrimaryType>(context[primarySubjectType].FindByKey(PrimaryData())->Chunk);///Find the primary DataChunk to be evaluated
		////Global::Log(Debug, PurposeLog, GetName(), "RetrieveConditionDataChunks", TEXT("Primary Subject: %s, Data sought after: %s, Value found: %s"), 
		//	/**UEnum::GetValueAsString(primarySubjectType)*/ *StaticEnum<ESubject>()->GetDisplayNameTextByValue((int64)primarySubjectType).ToString(), 
		//	*PrimaryData()->GetName(), 
		//	IsValid(primary) ? *primary->GetName() : TEXT("nullptr")
		//);

		if (!primary)
		{
			//Global::LogError(PURPOSE, GetName(), "RetrieveConditionDataChunks", TEXT("Primary from Context nullptr. Description: %s. Subject Type: %s, Subject: %s, Type: %s, Was Found? %s. Cast To %s %s.")
			//	, *description.ToString()
			//	,*Global::EnumValueOnly<ESubject>(primarySubjectType)
			//	//, subjectCombination.subjects.Contains(ESubject::Candidate) ? *subjectCombination.subjects[ESubject::Candidate].GetObject()->GetOuter()->GetName() : TEXT("Invalid")
			//	, *PrimaryData()->GetName()
			//	, IsValid(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("True") : TEXT("False")
			//	, *PrimaryType::StaticClass()->GetName()
			//	, Cast<PrimaryType>(context->DataChunk(primarySubjectType, PrimaryData())) ? TEXT("was a success") : TEXT("failed")
			//);
			return false;
		}
		////Global::Log(FULLTRACE, PurposeLog, "Condition", "RetrieveConditionDataChunk", TEXT("3"));

		return true;
	}

	///Test: Condition Purpose PurposeEvaluation; ensure that user specified min-max values actually affect score correctly
	///@return float = if a curve has been setup for condition, the inValue position on curve, else inValue
	float AdjustToCurve(float inValue) const
	{
		/*if (userResponseCurve.GetNumKeys() > 0)
		{
			//Global::Log(FULLTRACE, PurposeLog, GetName(), "AdjustToCurve", TEXT("Response Curve has user defined Keys."));
			if (max != min)
			{
				FRichCurve curveCopy = userResponseCurve;
				curveCopy.ReadjustTimeRange(min, max, true, 0, 1);
				return curveCopy.Eval(inValue);
			}

			return userResponseCurve.Eval(inValue);
		}
		else*/ if (IsValid(responseCurve.Get()))
		{
			Global::Log(CALLTRACETRIVIAL, PURPOSE, GetName(), "AdjustToCurve", TEXT("Response Curve is valid."));

			//FRichCurve curveCopy = responseCurve->FloatCurve;
			/*FIndexedCurve duplicateCurve = *responseCurve->FloatCurve.Duplicate();*/
			/*FRichCurve* duplicateCurve = Cast<FRichCurve>(responseCurve->FloatCurve.Duplicate());

			if (duplicateCurve)
			{
				duplicateCurve->ReadjustTimeRange(min, max, true, 0, 1);
				return duplicateCurve->Eval(inValue);
			}*/
			//float scoreAlongCurve = curveCopy.Eval(inValue);

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
			//return Normalize(inValue);

			if ((bHasScoreFloor && inValue < min) || (bHasScoreCeiling && inValue > max))
			{
				return 0.0f;
			}

			float normalizedScore = Normalize(inValue);
			Global::Log(DATATRIVIAL, PURPOSE, description.ToString(), "AdjustToCurve", TEXT("Invalue = %f. Score normalized = %f."), inValue, normalizedScore);
			return normalizedScore;
		}

		//return inValue;
	}

#pragma endregion

#pragma region DerivedConditions
public:

	/// Use this to establish the limitation on data chunks available to this condition
	//UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Primary Data Type To Compare", meta = (DisplayPriority = "3"))
	/////If desired, an alternate data type to compare primary data against
	/////Leave as Null to compare primary data against the value specified in Primary Data
	//TSubclassOf<T> primaryDataType = nullptr;

	//UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	//	///An Enum representing how the user wishes the two data types to be compared
	//	EConditional_ conditional = EConditional_::None;

	//UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Optional Value", meta = (DisplayPriority = "5"))
	/////Specify this value in place of a secondary subject + value
	/////Primary data value of primary subject will be compared against this
	//T value;

	/// Use this to establish the limitation on data chunks available to this condition
	//UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Secondary Data Type To Compare Against", meta = (DisplayPriority = "7"))
	////If desired, an alternate data type to compare primary data against
	////Leave as Null to compare primary data against the value specified in Primary Data
	//TSubclassOf<T> secondaryDataType = nullptr;

	/////Override this method with a covarient return type to represent the data specific to the condition
	//FORCEINLINE virtual TSubclassOf<UDataChunk> PrimaryData_Implementation() const final { return primaryDataType; }
	
	/////Override this method with a covarient return type to represent the data specific to the condition
	//FORCEINLINE TSubclassOf<UDataChunk> SecondaryData_Implementation() const final { return secondaryDataType; 

	//float EvaluateConditionInternal_Implementation(const UDataChunk* primaryDataChunk, const UDataChunk* secondaryDataChunk, class UPurposeAbilityComponent* owner) const override;

#pragma endregion

};
