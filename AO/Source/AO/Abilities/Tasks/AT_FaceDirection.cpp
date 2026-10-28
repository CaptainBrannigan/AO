// Copyright Jordan Cain. All Rights Reserved.


#include "AT_FaceDirection.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

void UAT_FaceDirection::Activate()
{
	Super::Activate();

	actorCache = Cast<AAIController>(GetOwnerActor());

	if (!actorCache)
	{
		EndTask();
		taskFinished.Broadcast();
	}

	Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("OwnerActor: %s.")
		, *actorCache->GetName()
	);
	
	pawnCache = actorCache->GetPawn();

	if (!pawnCache)
	{
		EndTask();
		taskFinished.Broadcast();
	}

	const FActorVectorChunk* locationToFace = DataChunk<FActorVectorChunk>(TAG_Data_Task_FaceDirection_Location);
	targetActorCache = locationToFace ? locationToFace->GetActor() : nullptr;

	rotationToFaceLocation = ((locationToFace ? locationToFace->GetValue() : pawnCache->GetActorLocation() + pawnCache->GetActorForwardVector()) - pawnCache->GetActorLocation()).GetSafeNormal().Rotation();

	//actorCache->SetFocalPoint(DataChunk<ULocationToFace>()->Value());

	//if (!actorCache->GetPawn())
	//{
	//	return;
	//}

	//TObjectPtr<UPawnMovementComponent> pawnMovementComp = actorCache->GetPawn()->GetMovementComponent();
	//if (!pawnMovementComp)
	//{
	//	return;
	//}

	//pawnMovementComp;
	
}

void UAT_FaceDirection::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	float faceDirectionDifference = pawnCache->GetActorRotation().Yaw - rotationToFaceLocation.Yaw;

	/// If we explictly state this task should end regardless of other task states for current action, then 
	if (taskDuration != ETaskDurationType::EndOnActionFinished && FMath::Abs(faceDirectionDifference) < rotationToLookDirectionTolerance)
	{
		EndTask();
		taskFinished.Broadcast();
	}

	if (targetActorCache)
	{
		rotationToFaceLocation = (targetActorCache->GetActorLocation() - pawnCache->GetActorLocation()).GetSafeNormal().Rotation();
	}

	/// If we have activated, then we have our actor and a location to face
	FRotator newRotationTowardsFaceLocation = UKismetMathLibrary::RInterpTo(pawnCache->GetViewRotation(), rotationToFaceLocation, DeltaTime, rotateToDirectionSpeed);

	Global::Log(DATATRIVIAL, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Difference between where actor is looking and meant to look: %f. Current Rotation: %s. New Rotation: %s.")
		, faceDirectionDifference
		, *pawnCache->GetActorRotation().ToString()
		, *newRotationTowardsFaceLocation.ToString()
	);

	pawnCache->FaceRotation(newRotationTowardsFaceLocation, DeltaTime);
	//pawnCache->SetActorRotation(newRotationTowardsFaceLocation);
}

void UAT_FaceDirection::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);


}
