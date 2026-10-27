// Copyright Jordan Cain. All Rights Reserved.


#include "AnimInstance_Base.h"

/** Play a Montage. Returns Length of Montage in seconds. Returns 0.f if failed to play. */
FAnimMontageInstance* UAnimInstance_Base::PlayMontage(TObjectPtr<UAnimMontage> MontageToPlay, float InPlayRate/*= 1.f*/, EMontagePlayReturnType ReturnValueType, float InTimeToStartMontageAt, bool bStopAllMontages /*= true*/)
{
	LLM_SCOPE(ELLMTag::Animation);

	//Refactor: AnimMontage PlayMontage; Why can't we access MontageToPlay->HasValidSlotSetup? Is it due to LogAnimMontage in the UE_LOGs?
	if (MontageToPlay && (MontageToPlay->GetPlayLength() > 0.f) /*&& MontageToPlay->HasValidSlotSetup()*/)
	{
		///TODO: Animation; apparently "IsCompattible" is now editor only, do we need to workaround that to ensure compatibility?
		//UE_LOG(LogTemp, Log, TEXT("Montage is valid. AnimInstance_Base::PlayMontage"));
		//if (CurrentSkeleton && CurrentSkeleton->IsCompatible(MontageToPlay->GetSkeleton()))/// Deprecated 5.4
		if (CurrentSkeleton /*&& CurrentSkeleton->IsCompatibleForEditor(MontageToPlay->GetSkeleton())*/)
		{
			//UE_LOG(LogTemp, Log, TEXT("Skeleton is valid. AnimInstance_Base::PlayMontage"));
			if (bStopAllMontages)
			{
				// Enforce 'a single montage at once per group' rule
				FName NewMontageGroupName = MontageToPlay->GetGroupName();

				FMontageBlendSettings blendSettings = FMontageBlendSettings();
				blendSettings.BlendProfile = MontageToPlay->BlendProfileIn;
				StopAllMontagesByGroupName(NewMontageGroupName, blendSettings);
			}

			// Enforce 'a single root motion montage at once' rule.
			if (MontageToPlay->bEnableRootMotionTranslation || MontageToPlay->bEnableRootMotionRotation)
			{
				FAnimMontageInstance* ActiveRootMotionMontageInstance = GetRootMotionMontageInstance();
				if (ActiveRootMotionMontageInstance)
				{
					ActiveRootMotionMontageInstance->Stop(MontageToPlay->BlendIn);
				}
			}

			FAnimMontageInstance* NewInstance = new FAnimMontageInstance(this);
			check(NewInstance);

			const float MontageLength = MontageToPlay->GetPlayLength();

			NewInstance->Initialize(MontageToPlay);
			NewInstance->Play(InPlayRate);
			NewInstance->SetPosition(FMath::Clamp(InTimeToStartMontageAt, 0.f, MontageLength));
			MontageInstances.Add(NewInstance);
			ActiveMontagesMap.Add(MontageToPlay, NewInstance);

			// If we are playing root motion, set this instance as the one providing root motion.
			if (MontageToPlay->HasRootMotion())
			{
				//Refactor: AnimInstance Animation RootMotion; How can we access private RootMotionMontageInstance in UAnimInstance?
				//RootMotionMontageInstance = NewInstance;
			}

			OnMontageStarted.Broadcast(MontageToPlay);

			//UE_LOG(LogAnimMontage, Verbose, TEXT("Montage_Play: AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
			//	*NewInstance->Montage->GetName(), NewInstance->GetDesiredWeight(), NewInstance->GetWeight());

			return NewInstance;
			//return (ReturnValueType == EMontagePlayReturnType::MontageLength) ? MontageLength : (MontageLength / (InPlayRate * MontageToPlay->RateScale));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Skeleton is invalid. AnimInstance_Base::PlayMontage"));
			//UE_LOG(LogAnimMontage, Warning, TEXT("Playing a Montage (%s) for the wrong Skeleton (%s) instead of (%s)."),
		//	*GetNameSafe(MontageToPlay), *GetNameSafe(CurrentSkeleton), *GetNameSafe(MontageToPlay->GetSkeleton()));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Montage is invalid. AnimInstance_Base::PlayMontage"));
		//UE_LOG(LogAnimMontage, Warning, TEXT("Playing a Montage (%s) for the wrong Skeleton (%s) instead of (%s)."),
	//	*GetNameSafe(MontageToPlay), *GetNameSafe(CurrentSkeleton), *GetNameSafe(MontageToPlay->GetSkeleton()));
	}

	return nullptr;
}

