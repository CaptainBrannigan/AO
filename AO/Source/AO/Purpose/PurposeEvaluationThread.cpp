// Copyright Jordan Cain. All Rights Reserved.


#include "PurposeEvaluationThread.h"
#include "Condition.h"
#include "Curves/CurveFloat.h"
#include "PurposeAbilityComponent.h"
#include "Purpose/Abilities/GA_PurposeBase.h"

DEFINE_LOG_CATEGORY(LogOccurrence);

FString FContextData::GetOwnerName()
{
	return purposeOwner.IsValid() ? purposeOwner->GetName() : "Invalid";
}

#pragma region PurposeEvaluationThread

bool FPurposeEvaluationThread::Init()
{
	//Global::Log(FULLTRACE, PURPOSE, "FPurposeEvaluationThread", "Init", TEXT(""));
	return true;/// Has to be true otherwise Thread won't run
}

void FPurposeEvaluationThread::Stop()
{
	//Global::Log(FULLTRACE, PURPOSE, "FPurposeEvaluationThread", "Stop", TEXT(""));
	stopThread = true;
	tickTimer = 1000.0f;/// Just in case Run() is somehow called in the middle of shutdown, it shouldn't have time to call again
}

void FPurposeEvaluationThread::Exit()
{
	//Global::Log(FULLTRACE, PURPOSE, "FPurposeEvaluationThread", "Exit", TEXT(""));
	//for (auto task : purposeSelectionTasks)
	//{
	//	if (task)
	//	{
	//		//Global::Log(FULLTRACE, PURPOSE, "FPurposeSelectionThread", "Exit", TEXT("InGameThread? %s. Attempting to cancel AsyncAbility."), IsInGameThread() ? TEXT("True") : TEXT("False"));

	//		task->Abandon();

	//		if (IsInGameThread())
	//		{
	//			//task->Cancel();
	//		}
	//	}
	//}
}

uint32 FPurposeEvaluationThread::Run()
{
	Global::Log(CALLTRACETRIVIAL, PURPOSE, "FEventThread", "Run", TEXT(""));

	while (!stopThread) ///Loop through queues until we decide to stop the thread
	{
		/// We evaluate in a backwards order, as we want each Event evaluation to be fully resolved by the time the next Event is evaluated

		/// Refactor: Purpose Evaluation Queue; Perhaps we can make the evaluation of a thread dependent instead on the order of its potentialPurposeQueues entries?
			/// Then instead of having to specify which layer in each thread, it's dictated by the order the keys are added when the map is setup
			///auto itr = potentialPurposeQueues.CreateIterator();

		FOccurrence purposeToEvaluate;
		if (DequeuePurpose(purposeToEvaluate))
		{
			SelectPurposeIfPossible(purposeToEvaluate);
		}
		FPlatformProcess::Sleep(tickTimer);///Supposedly allowing thread to sleep will help CPU optimize efficiency
		/* TODO: AI Behavior Performance; Claude suggestion:
		Fixed 50ms sleep after every single dequeue caps each thread at ~20 occurrences/sec, independent of backlog. PurposeEvaluationThread.cpp:51-72: Run() dequeues one FOccurrence, evaluates it against every candidate, then always sleeps tickTimer before looping — even if the queue is already backed up. Combine this with #1 (extra threads stop receiving new work once busy) and you have a hard throughput ceiling that gets worse exactly when "every event spawns an event" starts generating more Occurrences than that. Replace the poll-sleep with an FEvent-based wait that's triggered on Enqueue, and drain the whole backlog (or a batch) per wake instead of one-then-sleep.
		
		2. FEvent-driven wake — example. This replaces the fixed sleep with "wake immediately when work arrives, drain everything queued, then go back to sleep":

// In FPurposeEvaluationThread (header): 
FEvent* WakeEvent = nullptr;

bool FPurposeEvaluationThread::Init()
{
    WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);//(bIsManualReset=)
    return true;
}

void FPurposeEvaluationThread::Exit()
{
    if (WakeEvent) { FPlatformProcess::ReturnSynchEventToPool(WakeEvent); WakeEvent = nullptr; }
}

void FPurposeEvaluationThread::Stop()
{
    stopThread = true;
    if (WakeEvent) { WakeEvent->Trigger(); } // wake it now instead of waiting out the timeout
}

bool FPurposeEvaluationThread::QueuePurpose(FOccurrence potentialPurposesToQueue)
{
    if (potentialPurposeQueue.Enqueue(MoveTemp(potentialPurposesToQueue)))
    {
        numQueueItems++;
        if (WakeEvent) { WakeEvent->Trigger(); }
        return true;
    }
    return false;
}

uint32 FPurposeEvaluationThread::Run()
{
    while (!stopThread)
    {
        WakeEvent->Wait(FTimespan::FromMilliseconds(250)); // timeout is just a safety poll for stopThread

        FOccurrence purposeToEvaluate;
        while (!stopThread && DequeuePurpose(purposeToEvaluate))
        {
            SelectPurposeIfPossible(purposeToEvaluate);
        }
    }
    return 0;
}

The inner while (DequeuePurpose(...)) is the important part — it drains the whole backlog per wake instead of one item per tick, which is what actually removes the throughput ceiling.
		*/
	}

	return 0;/// When this point is reached, thread will shutdown
}

bool FPurposeEvaluationThread::SelectPurposeIfPossible(FOccurrence& occurrenceToEvaluate)
{
	/// Since we have a context to evaluate against, we evaluate every candidate for a new purpose in response to the context
	for (const TObjectPtr<UPurposeAbilityComponent>& candidate : occurrenceToEvaluate.candidatesForNewPurpose)
	{
		if (!candidate)
		{
			Global::LogError(PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Candidate is invalid!"));
			continue;
		}

		/// We do not want instigators reacting to their own occurrences
		if (occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Contains(ESubject::Instigator) && occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[ESubject::Instigator] == candidate.Get())
		{
			continue;
		}

		/// Refactor: Current Behavior; Both AI are heading on the patrol task (only 1 is supposed to but it seems activity data isn't updating quickly enough).
			/// But when AI2 is reacting to occurrence of AI1, their currenth patrol behavior has a higher conditionValue than what it would after the data updates
				///	I think the data isn't being updated quickly enough because each occurrence loops through each AI now. So while the purpose is being selected and passed back, we're still evaluating the same occurrence but with the next candidate
				/// 

		/// We establish the current high conditionValue as the high conditionValue of the current objective to pre-filter any lesser purposes
		/// Because the CurrentBehavior is stored as a copy it's safe to access
		float highScore = candidate->CurrentBehavior().cachedScoreOfPurpose;
		int highScorePurposeIndex = -1;
		FPurpose highScorePurpose;

		/// Firstly we need to add the candidate to the context subjects
		occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Add(ESubject::Candidate, TScriptInterface<IDataMapInterface>(candidate.Get()));

		/// Now for each candidate, evaluate the context against every potential purpose
		for (const FPurpose& potentialPurpose : purposeCacheForBackgroundThread)
		{
			if (!potentialPurpose.behaviorAbility)
			{
				Global::LogError(PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Behavior for purpose %s not selected!")
					, *potentialPurpose.descriptionOfPurpose
				);
				continue;
			}

			/// Potential conditionValue is used to determine whether this purpose will remain above the minimum conditionValue of previous purposes
			/// Potential conditionValue equals +1 for each condition + an exponential decay additional
			/// More conditions then give purposes a slight advantage that decays so that it doesn't stifle competition against other purposes with less conditions
			float potentialScore = 0.0f;
			///Total weight is used to adjust a condition's conditionValue by condition->weight / totalWeight
			///This is so that conditions can be given a user selected weight without having to recalculate other condition->weight for each adjustment
			float totalWeight = 0.0f;

			potentialPurpose.Potential(potentialScore, totalWeight);

			const int totalConditions = potentialPurpose.GetConditions().Num();

			/// the potential conditionValue for each condition increases with the number of conditions
			/// when we divide that total potential conditionValue by the total number of conditions we get a potential conditionValue for each condition
			/// So with 3 conditions, the potential conditionValue of each individual is higher than when just 1 condition
			const float totalPotentialAdjustedForNumConditions = potentialScore / totalConditions;

			/// ConditionDetractor is the difference between how much a condition could conditionValue and how much it actually scores
			/// By continually adding that difference to a single variable, we can test whether potentialScore - conditionDetractor < min (or the current highest conditionValue)
			float conditionDetractor = 0.0f;

			float finalScore = 0.0f;
			Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Scoring: %s For Candidate: %s. Number Conditions: %d.")
				, *potentialPurpose.descriptionOfPurpose
				, *Cast<UObject>(candidate.Get())->GetOuter()->GetName()
				, totalConditions
			);

			/// Now that we are ready to evaluate for the conditions, we will need to combine the data of the context with the data of the subjects
			/// While this will make each data chunk a copy rather than the exact current data from a pointer, the differences in time between queuing and evaluation should be milliseconds
			/// It's a minimal price to pay for the new structure of purpose, where we no longer have to manually root/unroot object pointers for background threads
			TMap<ESubject, TArray<FDataMapEntry>> subjectMapForCondition = occurrenceToEvaluate.subjectMapForPotentialPurposes.GetSubjectsAsDataMaps();
			subjectMapForCondition.Add(ESubject::Context, occurrenceToEvaluate.ContextDataForPotentialPurposes);

			/// If a subject is valid and has a current behavior, we're going to add the datamap of it's current behavior to the data of that subject
			/// I really don't care for this technical design as it is not replicated anywhere else so if EvaluateCondition were attempted elsehwere it could produce a totally different result
			/// However, adding the current behavior data to a subject only for evaluation combines the simplicity of designing conditions with only the subject in mind
			/// And the technical versatility of accessing the data of a current behavior from that current behavior, essentially keeping behavior data encapsulated yet able to be accessed for conditions
			TArray<ESubject> subjectTypesToCheckForCurrentBehavior({ ESubject::Instigator, ESubject::EventTarget, ESubject::Candidate });
			for (ESubject subject : subjectTypesToCheckForCurrentBehavior)
			{
				if (
					occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Contains(subject) 
					&& IsValid(occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[subject].GetObject())
					&& occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[subject].GetObject()->IsA<UPurposeAbilityComponent>()
					)
				{
					FContextData& contextForSubjectCurrentBehavior = Cast<UPurposeAbilityComponent>(occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[subject].GetObject())->CurrentBehavior();
					if (contextForSubjectCurrentBehavior.ContextIsValid())
					{
						/// We know the subject is contained because it was contained in the subjectMapForPotentialPurposes, which was use to create subjectMapForCondition
						subjectMapForCondition[subject].Append(contextForSubjectCurrentBehavior.contextData);
					}
				}
			}

			for (const TObjectPtr<UCondition> condition : potentialPurpose.GetConditions())
			{
				if (!IsValid(condition))
				{
					Global::Log(DATATRIVIAL, LogCondition, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Purpose->conditions returned an invalid object."));
					conditionDetractor += totalPotentialAdjustedForNumConditions;///Ensure that if this condition can't evaluate it counts against purpose
					continue;
				}

				/// Refactor: Potential Score Evaluation; need to actually change potential score
					/// max potential of each condition is individualpotentialscore (which is where the adjustment based on number of conditions comes in)
					/// However potential score does not account for the number of conditions adjustment, only weight. So we get a number that's close to equal the number of conditions, rather than near 1
					/// In order to correctly use the conditionDetractor, we have to utilize all individual potential scores
				if ((totalPotentialAdjustedForNumConditions - conditionDetractor) < highScore)///Potential conditionValue adjusted by actual condition scores must remain above min
				{
					Global::Log(DATADEBUGTRIVIAL, LogCondition, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Potential Score of %s, %f, less than min of %f."), *potentialPurpose.descriptionOfPurpose, totalPotentialAdjustedForNumConditions - conditionDetractor, highScore);
					finalScore = 0.0f;
					break;
				}

				float conditionValue = condition->InitiateEvaluation(subjectMapForCondition, candidate);///Get a baseline conditionValue for condition
				/// Important to note, the conditionValue is not an actual score. It is just the value returned by the condition
				/// The curve score is that value translated into a 0-1 value which represents the score
				float curveScore = condition->AdjustToCurve(conditionValue);///Adjust conditionValue to fit along a curve if present

				if (curveScore <= 0 && condition->isRequired)
				{
					Global::Log(DATADEBUG, LogCondition, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Required condition %s is 0.")
						, *condition->description.ToString()
					);
					finalScore = 0.0f;
					break;
				}

				/// If we multiply the conditionValue adjusted to the curve by the totalPotentialAdjustedForNumConditions
				/// We provide an adjustment to conditionValue that results in purposes with more conditions having a slightly higher conditionValue potential
				/// This is to mitigate the higher risk of low value conditions and reward complexity of purpose scoring
				float curveScoreAdjustedPotentialScore = curveScore * totalPotentialAdjustedForNumConditions;

				/// When we divide the current weight of the condition by the total weight and multiply the conditionValue by that, 
				/// We are actually normalizing the the entire purpose's conditionValue to its maxpotentialscore / totalweight
				/// While allowing each condition to make up a larger bulk of that conditionValue
				float adjustedCurveScoreForNumConditions = curveScoreAdjustedPotentialScore * (condition->weight / totalWeight);

				/// Get the difference between it's potential conditionValue by its curve adjusted conditionValue (both including weight of condition)
				conditionDetractor += (totalPotentialAdjustedForNumConditions * (condition->weight / totalWeight)) - adjustedCurveScoreForNumConditions;///if curveScore is < 1, then conditionDetractor will increase

				/// Scores are normalized to their max, so we just add them up for the final conditionValue
				finalScore += adjustedCurveScoreForNumConditions;
				Global::Log(DATATRIVIAL, LogCondition, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Original Score for %s: %f; CurveScore: %f. Potential based on Conditions: %f. TotalPotential = %f. CurveScoreAdjustedByPotential: %f. Condition->Weight: %f. TotalWeight: %f. TotalDeductionFromPurposeScore: %f. Score normalized against total num Conditions: %f. Final Score: %f")
					, *condition->description.ToString()
					, conditionValue
					, curveScore
					, totalPotentialAdjustedForNumConditions
					, potentialScore
					, curveScoreAdjustedPotentialScore
					, condition->weight
					, totalWeight
					, conditionDetractor
					, adjustedCurveScoreForNumConditions
					, finalScore
				);

				Global::Log(DATADEBUGTRIVIAL, LogCondition, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Score for Condition: %s = %f; Potential Score = %f; Score Normalized against all Conditions: %f."), *condition->description.ToString(), curveScore, totalPotentialAdjustedForNumConditions, adjustedCurveScoreForNumConditions);
			}

			Global::Log(DATAESSENTIAL, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Candidate %s. Score of %s is %f. High score: %f. Instigator %s. %s.")
				, *Cast<UObject>(candidate)->GetOuter()->GetName()
				, *potentialPurpose.descriptionOfPurpose
				, finalScore
				, highScore
				, occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Contains(ESubject::Instigator) ? *occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[ESubject::Instigator].GetObject()->GetOuter()->GetName() : TEXT("Unknown")
				, occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Contains(ESubject::EventTarget) ? *FString::Printf(TEXT("ObjectiveTarget %s"), *occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[ESubject::EventTarget].GetObject()->GetOuter()->GetName())
				: TEXT("Unknown Target")
			);

			//contextData->ScoreCache = finalScore;/// Ensure the context has a the conditionValue stored for future use
			//return finalScore;

			if (finalScore > highScore)
			{
				Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Candidate %s. %s has high conditionValue of %f.")
					, *Cast<UObject>(candidate)->GetOuter()->GetName()
					, *potentialPurpose.descriptionOfPurpose
					, finalScore
				);

					
				/* Todo: Performance Purpose;
				Full FPurpose struct copies on every score improvement, not just the final winner (PurposeEvaluationThread.cpp:261) — cheap to fix by tracking the winning index/pointer and copying once at the end.
				*/
				/// Ensure new high conditionValue is reflected
				highScore = finalScore;
				highScorePurpose = potentialPurpose;
			}
		}

		/// if highScore was set, a purpose was found
		if (highScore > 0)
		{
			/// So now we want to pass the purpose back to the owner and game thread
			FContextData context(
				highScorePurpose
				, occurrenceToEvaluate.subjectMapForPotentialPurposes
				, occurrenceToEvaluate.ContextDataForPotentialPurposes
				, candidate
			);

			context.cachedScoreOfPurpose = highScore;

			if (!context.purpose.behaviorAbility) { 
				continue; }/// If the behavior is invalid, we shouldn't be receiving calls to this

			if ( context.HasSubject(ESubject::EventTarget) && candidate->CurrentBehavior().HasSubject(ESubject::EventTarget)
				&& context.Subject(ESubject::EventTarget) == candidate->CurrentBehavior().Subject(ESubject::EventTarget) /// If the target
				&& context.purpose.behaviorAbility == candidate->CurrentBehavior().purpose.behaviorAbility)/// And the ability are both the same
				///Then we don't want to restart behavior, and we don't want it passed back to the Game Thread at all
			{
				Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Incoming purpose %s already being performed by %s.")
					, *context.purpose.descriptionOfPurpose
					, *candidate->GetOwner()->GetName()
				);
				continue;
			}

			CreateAsyncTask_PurposeSelected(context);
		}
		else
		{

			Global::Log(DATADEBUGTRIVIAL, LogOccurrence, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Purpose not found for %s.")
				, *candidate->GetOwner()->GetName()
			);
		}
	}
	
	FString instigatorName = occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects.Contains(ESubject::Instigator) ? *occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[ESubject::Instigator].GetObject()->GetPathName(occurrenceToEvaluate.subjectMapForPotentialPurposes.subjects[ESubject::Instigator].GetObject()->GetOuter()) : TEXT("No Instigator");
	Global::Log(DATADEBUGTRIVIAL, LogOccurrence, "PurposeSystem", "Occurrence", TEXT("End of Occurrence caused by %s"), *instigatorName);

	return false;
}

bool FPurposeEvaluationThread::CreateAsyncTask_PurposeSelected(FContextData context)
{
	TGraphTask<FAsyncGraphTask_PurposeSelected>::CreateTask().ConstructAndDispatchWhenReady(context);
	return true;
}

#pragma endregion

#pragma region Event Thread

FEventThread::~FEventThread()
{
	Global::Log(FULLTRACE, PURPOSE, "FEventThread", "~FEventThread", TEXT(""));
}

#pragma endregion

#pragma region Actor Thread

FActorThread::~FActorThread()
{
	//Global::Log(FULLTRACE, PURPOSE, "FActorThread", "~FActorThread", TEXT(""));
}

#pragma endregion

#pragma region Companion Thread

FCompanionThread::~FCompanionThread()
{
	//Global::Log(FULLTRACE, PURPOSE, "CompanionThread", "~CompanionThread", TEXT(""));
}

#pragma endregion

#pragma region TAsyncGraphTasks

void FAsyncGraphTask_PurposeSelected::PurposeSelected()
{
	Global::Log(FULLTRACE, PURPOSE, "FAsyncGraphTask_PurposeSelected", "PurposeSelected", TEXT("Purpose: %s, IsInGameThread: %s")
		, *contextData.GetName()
		, IsInGameThread() ? TEXT("True") : TEXT("False")
	);

	PurposeSystem::PurposeSelected(contextData);
}

#pragma endregion

bool PurposeSystem::Private::AttemptToPassSelectedPurposeToOwner(const FContextData& selectedPurpose)
{
	return selectedPurpose.purposeOwner->ProvidePurposeToOwner(selectedPurpose);
}


