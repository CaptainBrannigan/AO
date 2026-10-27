// Copyright Jordan Cain. All Rights Reserved.


#include "AT_RunEQSQuery.h"
#include "Data/GlobalLog.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "Abilities/AI/AI_SequentialActions.h"
#include "NativeGameplayTags.h"
#include "Purpose/DataChunks/ContextualHistory.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Seeking, "Seeking");

UAT_RunEQSQuery::UAT_RunEQSQuery(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	AddData(CreateDefaultSubobject<UResult>("Result"));
	NotificationDelegate = FQueryFinishedSignature::CreateUObject(this, &UAT_RunEQSQuery::ResultFound);
	contextualTagForTask.AddTag(TAG_Seeking);
}

UAT_RunEQSQuery* UAT_RunEQSQuery::RunEQSQuery(AAIController* Controller, UEnvQuery* QueryTemplate, TEnumAsByte<EEnvQueryRunMode::Type> inRunMode)
{
	if (QueryTemplate == nullptr || Controller == nullptr)
	{
		return nullptr;
	}

	UAT_RunEQSQuery* MyTask = UAITask::NewAITask<UAT_RunEQSQuery>(*Controller, EAITaskPriority::High);
	if (MyTask)
	{
		MyTask->EQSRequest.QueryTemplate = QueryTemplate;
		MyTask->EQSRequest.RunMode = inRunMode;
	}

	return MyTask;
}

void UAT_RunEQSQuery::Activate()
{
	if (EQSRequest.QueryTemplate && OwnerController
		&& OwnerController->GetBlackboardComponent()
		&& OwnerController->GetPawn())
	{

		UE_VLOG(Cast<UObject>(GetGameplayTasksComponent()), LogGameplayTasks, Verbose
			, TEXT("%s Activate called, current State: %s")
			, *GetName(), *GetTaskStateName());

		/// We are going to attempt to avoid using the pawn, and we already don't use the blackboard, so we should be able to skip calling AITask::Activate super
		//Super::Activate();
		EQSRequest.Execute(*Cast<UObject>(GetGameplayTasksComponent()), nullptr, EQSFinishedDelegate);
	}
}

bool UAT_RunEQSQuery::PreActivationInitialization(const FActionSequenceEntry& entryData, UAI_SequentialActions& source)
{
	if (!IsValid(query))
	{
		Global::Log(DATADEBUG, TASK, "UAT_RunEQSQuery", "PreActivationInitialization", TEXT("Query asset invalid!"));
		return false;
	}

	EQSRequest.QueryTemplate = query->query;
	EQSRequest.RunMode = runMode;

	FAIDynamicParam param;
	param.ParamType = EAIParamType::Float;
	//AActor* actor = GetOwnerActor();
	//param.Value = *actor;
	//EQSRequest.QueryConfig.Add();/// Check to see if we have a tag that indicates a found position, and modify that tag to indicate we are moving to that position
	

	/// Check to see if we have a tag that indicates a found position, and modify that tag to indicate we are moving to that position
	if (source.Vessel() && source.Vessel()->HasData(UContextualHistory::StaticClass()))
	{
		TObjectPtr<UContextualHistory> contextualTagData = source.Vessel()->DataChunk<UContextualHistory>();

		for (int i = 0; i < query->contextualTagsForQuery.Num(); ++i)
		{
			FGameplayTag tag = query->contextualTagsForQuery.GetByIndex(i);
			Global::Log(DATATRIVIAL, TASK, "UAT_RunEQSQuery", "PreActivationInitialization", TEXT("Tag: %s."), *tag.ToString());

			/// If we have a position tag
			if (!tag.MatchesTag(TAG_Contextual_Position))
			{
				continue;
			}

			/// We want to append the moving context to that tag
			FString tagName = tag.GetTagName().ToString();
			FString stringToAppend = "Seeking";

			FString appendedTagName = tagName + "." + stringToAppend;

			FGameplayTag newTag = FGameplayTag::RequestGameplayTag(FName(appendedTagName), false);

			Global::Log(DATATRIVIAL, TASK, "UAT_RunEQSQuery", "PreActivationInitialization", TEXT("Appended tag with movement tag: %s. StringName: %s."), *newTag.ToString(), *appendedTagName);

			if (!newTag.IsValid())
			{
				continue;
			}

			contextualTagData->AddToValue(newTag);
		}
	}


	return EQSRequest.QueryTemplate != nullptr;
}

void UAT_RunEQSQuery::ResultFound(TSharedPtr<FEnvQueryResult> Result)
{
	Global::Log(FULLTRACE, TASK, GetName(), "QueryFinished", TEXT(""));
	if (!IsValid(this)) /// In case a delegate attempts to callback and object is being destroyed
	{
		return;
	}

	if (!Result.IsValid())
	{
		Global::LogError(TASK, GetName(), "QueryFinished", TEXT("Query Result is invalid!"));
		return;
	}

	float highItemScore = 0.0f;
	TObjectPtr<AActor> chosenActor = nullptr;
	FVector chosenLocation;
	Global::Log(DATADEBUG, TASK, GetName(), "QueryFinished", TEXT("Results.Num(): %d."), Result->Items.Num());

	for (int i = 0; i < Result->Items.Num(); ++i)
	{
		if (Result->GetItemScore(i) < highItemScore)
		{
			continue;/// ignore the result
		}

		highItemScore = Result->GetItemScore(i);

		if ((chosenActor = Result->GetItemAsActor(i)))
		{
			Global::Log(DATATRIVIAL, TASK, GetName(), "ResultFound", TEXT("Actor: %s. Item Score: %f; High Item Score: %f."), *chosenActor.GetName(), Result->GetItemScore(i), highItemScore);
		}
		else ///if (Result->GetItemAsLocation(i) != FVector::ZeroVector)
		{
			chosenLocation = Result->GetItemAsLocation(i);
			Global::Log(DATATRIVIAL, TASK, GetName(), "ResultFound", TEXT("Location: %s. Item Score: %f; High Item Score: %f."), *chosenLocation.ToString(), Result->GetItemScore(i), highItemScore);
		}
	}

	//UBlackboardComponent* MyBlackboard = GetAIController()->GetBlackboardComponent();

	//if (!MyBlackboard)
	//{
	//	Global::LogError(BEHAVIOR, GetName(), "ResultFound", TEXT("Blackboard not valid!"));
	//	return;
	//}

	//MyBlackboard->ClearValue(keyNameForSelectedItem);
	////MyBlackboard->GetValueAsBool(targetKey.IsSet() ? targetKey.SelectedKeyName : targetKeyName);

	//if (chosenActor)
	//{
	//	MyBlackboard->SetValueAsObject(keyNameForSelectedItem, chosenActor);
	//}
	//else if (chosenLocation != FVector::ZeroVector)
	//{
	//	MyBlackboard->SetValueAsVector(keyNameForSelectedItem, chosenLocation);
	//}
	//else
	//{
	//	Global::LogError(BEHAVIOR, GetName(), "ResultFound", TEXT("No value sent to Blackboard!"));
	//}

	if (chosenActor)
	{
		DataChunk<UResult>()->SetValueWithActor(chosenActor);
	}
	else if (chosenLocation != FVector::ZeroVector)
	{
		DataChunk<UResult>()->SetValue(chosenLocation);
	}
	else
	{
		Global::LogError(BEHAVIOR, GetName(), "ResultFound", TEXT("No value sent to Blackboard!"));
	}

	EndTask();
	taskFinished.Broadcast();
}
