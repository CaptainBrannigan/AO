// Copyright Jordan Cain. All Rights Reserved.


#include "PurposeEvaluationThread.h"
#include "Condition.h"
#include "Curves/CurveFloat.h"
#include "HAL/IConsoleManager.h"
#include "PurposeAbilityComponent.h"
#include "Purpose/Abilities/GA_PurposeBase.h"
#include "Purpose/Abilities/Behavior_AI.h"

DEFINE_LOG_CATEGORY(LogOccurrence);

/// Data age test: how old can an occurrence's snapshot be by the time it's evaluated (and by the time the result reaches the game thread)
/// Every evaluation logs its age at DATADEBUG; anything older than this is logged as an error regardless of verbosity
static TAutoConsoleVariable<float> CVarPurposeSnapshotAgeWarning(
	TEXT("AO.Purpose.SnapshotAgeWarningSeconds"),
	2.0f,
	TEXT("Occurrence snapshots older than this (in seconds) when evaluated, or when the selected purpose reaches the game thread, are logged as errors."),
	ECVF_Default);

FString FContextData::GetOwnerName() const
{
	return purposeOwner.IsValid() ? purposeOwner->GetName() : "Invalid";
}

FString FContextData::Description() const
{
	FString name = GetName();
	for (const TPair<ESubject, TWeakObjectPtr<UObject>>& subject : subjectMap.subjects)
	{
		IDataMapInterface* dataMapInterface = Cast<IDataMapInterface>(subject.Value.Get());
		name += LINE_TERMINATOR;
		if (!dataMapInterface)
		{
			name += FString::Printf(TEXT("Subject invalid! Subject: %s."), *Global::EnumValueOnly<ESubject>(subject.Key));
			continue;
		}

		name += FString::Printf(TEXT("Subject: %s. Object: %s. Data: %s")
			, *Global::EnumValueOnly<ESubject>(subject.Key)
			, *subject.Value->GetName()
			, *dataMapInterface->DataMap().Description()
		);
	}

	name += LINE_TERMINATOR;
	name += TEXT("Context: ") + contextData.Description();

	return name;
}

bool FContextData::AdjustData(ESubject target, FGameplayTag inKey, const FDataModifier* modifier)
{
	if (!modifier || !inKey.IsValid())
	{
		Global::LogError(PURPOSE, GetName(), "AdjustData", TEXT("DataAdjustment for subject %s has %s!")
			, *Global::EnumValueOnly<ESubject>(target)
			, modifier ? TEXT("no key") : TEXT("no modifier")
		);
		return false;
	}

	if (target == ESubject::Context)/// The context isn't an object, its data is held directly
	{
		Global::Log(DATATRIVIAL, PURPOSE, GetName(), "AdjustData", TEXT("Adjusting context %s: %s."), *inKey.ToString(), *modifier->Description());
		return modifier->Apply(contextData, inKey);
	}

	if (TScriptInterface<IDataMapInterface> subject = DataMapInterfaceForSubject(target))
	{
		Global::Log(DATATRIVIAL, PURPOSE, GetName(), "AdjustData", TEXT("Adjusting %s of %s: %s."), *inKey.ToString(), *Global::EnumValueOnly<ESubject>(target), *modifier->Description());
		return modifier->Apply(subject->EditDataMap(), inKey);
	}

	Global::LogError(PURPOSE, GetName(), "AdjustData", TEXT("DataAdjustment for %s does not contain subject %s!")
		, *GetName()
		, *Global::EnumValueOnly<ESubject>(target)
	);
	return false;
}

void FContextData::AdjustDataIfPossible(EPurposeSelectionEvent inEventTypeToAdjust, const FLogCategoryBase& inLogCat, FString callingMethodName)
{
	for (const FPurposeModificationEntry& adjustment : purpose.DataAdjustments())/// For every piece of data we want to adjust based on the selected purpose
	{
		if (adjustment.selectionEvent != inEventTypeToAdjust)
		{
			continue;
		}

		const FDataModifier* modifier = adjustment.dataAdjustment.GetPtr<FDataModifier>();
		if (!modifier)
		{
			/// Only log if the DataAdjustment was invalid
			Global::LogError(inLogCat, GetName(), callingMethodName, TEXT("DataAdjustment %s has no modifier selected!"), *adjustment.description.ToString());
			continue;
		}

		AdjustData(adjustment.subjectToAdjust, adjustment.dataKey, modifier);
	}
}

#pragma region Snapshot

namespace PurposeSystem::Private
{
	bool AttemptToPassSelectedPurposeToOwner(const FContextData& selectedPurpose);

	/// The subject's data, with its current behavior's context data appended when it is a purpose component
	/// So conditions can be designed with only the subject in mind, while still reaching the data of the behavior it is performing
	/// The subject's own data takes priority over its behavior's data for the same key
	FDataMap SnapshotSubject(UObject& subject)
	{
		FDataMap snapshot;
		if (IDataMapInterface* dataMapInterface = Cast<IDataMapInterface>(&subject))
		{
			snapshot = dataMapInterface->Snapshot();
		}

		if (UPurposeAbilityComponent* component = Cast<UPurposeAbilityComponent>(&subject))
		{
			if (component->CurrentBehavior().ContextIsValid())
			{
				snapshot.AppendMissing(component->CurrentBehavior().contextData.Snapshot());
			}
		}

		return snapshot;
	}

	FOccurrence MakeOccurrence(const TArray<TObjectPtr<UPurposeAbilityComponent>>& inCandidates, const FSubjectMap& subjectsOfContext, const FDataMap& context)
	{
		check(IsInGameThread());

		FOccurrence occurrence;
		occurrence.snapshotTimeSeconds = FPlatformTime::Seconds();
		occurrence.subjectMap = subjectsOfContext;
		occurrence.contextData = context;

		for (const TPair<ESubject, TWeakObjectPtr<UObject>>& subject : subjectsOfContext.subjects)
		{
			UObject* subjectObject = subject.Value.Get();
			if (!subjectObject)
			{
				Global::LogError(PURPOSE, "PurposeSystem", "MakeOccurrence", TEXT("Subject %s value is invalid!."), *Global::EnumValueOnly<ESubject>(subject.Key));
				continue;
			}

			/// Only Instigator and EventTarget get their current behavior appended, as before. Candidate is handled per candidate below
			if (subject.Key == ESubject::Instigator || subject.Key == ESubject::EventTarget)
			{
				occurrence.subjectSnapshots.Add(subject.Key, SnapshotSubject(*subjectObject));
			}
			else if (IDataMapInterface* dataMapInterface = Cast<IDataMapInterface>(subjectObject))
			{
				occurrence.subjectSnapshots.Add(subject.Key, dataMapInterface->Snapshot());
			}
		}

		occurrence.subjectSnapshots.Add(ESubject::Context, context.Snapshot());/// Context becomes an ordinary subject for conditions

		if (UObject* instigator = subjectsOfContext.Get(ESubject::Instigator))
		{
			occurrence.instigatorKey = instigator;
			occurrence.instigatorName = instigator->GetPathName(instigator->GetOuter());
		}
		else
		{
			occurrence.instigatorName = TEXT("No Instigator");
		}

		if (UObject* eventTarget = subjectsOfContext.Get(ESubject::EventTarget))
		{
			occurrence.eventTargetKey = eventTarget;
		}

		/// An explicit Candidate subject (SeekNewBehavior) is snapshot as a candidate rather than a subject
		TArray<UPurposeAbilityComponent*, TInlineAllocator<1>> explicitCandidate;
		if (UPurposeAbilityComponent* candidateSubject = Cast<UPurposeAbilityComponent>(subjectsOfContext.Get(ESubject::Candidate)))
		{
			explicitCandidate.Add(candidateSubject);
			occurrence.subjectSnapshots.Remove(ESubject::Candidate);
		}

		auto addCandidate = [&occurrence](UPurposeAbilityComponent* candidate)
		{
			if (!IsValid(candidate))
			{
				return;
			}

			FCandidateSnapshot& snapshot = occurrence.candidates.AddDefaulted_GetRef();
			snapshot.component = candidate;
			snapshot.componentKey = candidate;
			snapshot.data = SnapshotSubject(*candidate);
			snapshot.currentScore = candidate->CurrentBehavior().cachedScoreOfPurpose;
			snapshot.currentBehaviorAbility = candidate->CurrentBehavior().purpose.behaviorAbility.Get();
			if (UObject* currentTarget = candidate->CurrentBehavior().subjectMap.Get(ESubject::EventTarget))
			{
				snapshot.currentEventTarget = currentTarget;
			}
			snapshot.debugName = candidate->GetOwner() ? candidate->GetOwner()->GetName() : candidate->GetName();
		};

		if (explicitCandidate.Num() > 0 && inCandidates.Num() < 1)
		{
			addCandidate(explicitCandidate[0]);
		}

		for (const TObjectPtr<UPurposeAbilityComponent>& candidate : inCandidates)
		{
			addCandidate(candidate.Get());
		}

		return occurrence;
	}

	FPurposeEvaluationThread* SelectThread(const TArray<FPurposeEvaluationThread*>& backgroundThreads, bool bRequireCandidateCache)
	{
		FPurposeEvaluationThread* selectedThread = nullptr;
		int leastNumQueuedPurposes = 0;
		for (FPurposeEvaluationThread* thread : backgroundThreads)
		{
			if (!thread)
			{
				continue;
			}
			if (thread->GetPurposeCacheForBackgroundThread().Num() < 1)
			{
				Global::Log(DATADEBUGTRIVIAL, EVENT, "PurposeSystem", "Occurrence", TEXT("No Purposes stored on background thread!"));
				continue;
			}

			if (bRequireCandidateCache && thread->GetCandidateCache().Num() < 1)
			{
				Global::Log(DATADEBUGTRIVIAL, EVENT, "PurposeSystem", "Occurrence", TEXT("No candidates for Occurrence!"));
				continue;
			}

			if (leastNumQueuedPurposes == 0)
			{
				leastNumQueuedPurposes = thread->GetNumQueuedPurposes();
				selectedThread = thread;
				continue;
			}

			if (thread->GetNumQueuedPurposes() <= leastNumQueuedPurposes)
			{
				leastNumQueuedPurposes = thread->GetNumQueuedPurposes();
				selectedThread = thread;
			}
		}

		if (!selectedThread)
		{
			Global::Log(DATADEBUG, EVENT, "PurposeSystem", "Occurrence", TEXT("No valid background thread was selected!"));
		}

		return selectedThread;
	}
}

bool PurposeSystem::Occurrence(const FSubjectMap& subjectsOfContext, const FDataMap& context, const TArray<FPurposeEvaluationThread*>& backgroundThreads)
{
	const bool bHasExplicitCandidate = subjectsOfContext.Contains(ESubject::Candidate);
	FPurposeEvaluationThread* selectedThread = Private::SelectThread(backgroundThreads, !bHasExplicitCandidate);
	if (!selectedThread)
	{
		return false;
	}

	FOccurrence occurrence = Private::MakeOccurrence(bHasExplicitCandidate ? TArray<TObjectPtr<UPurposeAbilityComponent>>() : selectedThread->GetCandidateCache(), subjectsOfContext, context);
	Global::Log(DATADEBUGTRIVIAL, LogOccurrence, "PurposeSystem", "Occurrence", TEXT("Ocurrence caused by %s"), *occurrence.instigatorName);

	/// Queue the subjects, context, and potential purposes to background thread
	/// At this time, we don't bother with UniqueSubjects for Occurrences, as Conditions for Events are revolving strictly around the context of the Occurrence
	return selectedThread->QueuePurpose(MoveTemp(occurrence));
}

bool PurposeSystem::Occurrence(const TArray<TObjectPtr<UPurposeAbilityComponent>>& inCandidates, const FSubjectMap& subjectsOfContext, const FDataMap& context, const TArray<FPurposeEvaluationThread*>& backgroundThreads)
{
	FPurposeEvaluationThread* selectedThread = Private::SelectThread(backgroundThreads, false);
	if (!selectedThread)
	{
		return false;
	}

	FOccurrence occurrence = Private::MakeOccurrence(inCandidates, subjectsOfContext, context);

	return selectedThread->QueuePurpose(MoveTemp(occurrence));
}

void PurposeSystem::PurposeSelected(FContextData contextOfSelectedPurpose)
{
	if (!contextOfSelectedPurpose.purposeOwner.IsValid())
	{
		Global::LogError(EVENT, "PurposeSystem", "PurposeSelected", TEXT("Provided an invalid PurposeOwner!"));
		return;
	}

	/// Data age test, second half: snapshot -> evaluated -> back on the game thread
	if (contextOfSelectedPurpose.snapshotTimeSeconds > 0.0)
	{
		const double ageSeconds = FPlatformTime::Seconds() - contextOfSelectedPurpose.snapshotTimeSeconds;
		if (ageSeconds > CVarPurposeSnapshotAgeWarning.GetValueOnGameThread())
		{
			Global::LogError(LogOccurrence, "PurposeSystem", "PurposeSelected", TEXT("%s selected from data %.3fs old!"), *contextOfSelectedPurpose.GetName(), ageSeconds);
		}
		else
		{
			Global::Log(DATADEBUG, LogOccurrence, "PurposeSystem", "PurposeSelected", TEXT("%s selected from data %.3fs old."), *contextOfSelectedPurpose.GetName(), ageSeconds);
		}
	}

	bool purposeAccepted = PurposeSystem::Private::AttemptToPassSelectedPurposeToOwner(contextOfSelectedPurpose);

	if (!purposeAccepted)
	{
		Global::Log(DATADEBUG, PURPOSE, "PurposeSystem", "PurposeSelected", TEXT("Purpose Owner %s did not accept the provided purpose: %s!")
			, *contextOfSelectedPurpose.GetOwnerName()
			, *contextOfSelectedPurpose.GetName()
		);

		return;
	}

	/// Adjust the owner's stored behavior rather than this copy, so adjustments to ESubject::Context persist with the behavior
	contextOfSelectedPurpose.purposeOwner->CurrentBehavior().AdjustDataIfPossible(EPurposeSelectionEvent::OnSelected, PURPOSE, "PurposeSelected");
}

#pragma endregion

#pragma region PurposeEvaluationThread

bool FPurposeEvaluationThread::Init()
{
	return true;/// Has to be true otherwise Thread won't run
}

void FPurposeEvaluationThread::Stop()
{
	stopThread = true;
	tickTimer = 1000.0f;/// Just in case Run() is somehow called in the middle of shutdown, it shouldn't have time to call again
}

void FPurposeEvaluationThread::Exit()
{
}

uint32 FPurposeEvaluationThread::Run()
{
	Global::Log(CALLTRACETRIVIAL, PURPOSE, "FEventThread", "Run", TEXT(""));

	while (!stopThread) ///Loop through queues until we decide to stop the thread
	{
		FOccurrence purposeToEvaluate;
		if (DequeuePurpose(purposeToEvaluate))
		{
			SelectPurposeIfPossible(purposeToEvaluate);
		}
		FPlatformProcess::Sleep(tickTimer);///Supposedly allowing thread to sleep will help CPU optimize efficiency
		/* TODO: AI Behavior Performance; Claude suggestion:
		Fixed 50ms sleep after every single dequeue caps each thread at ~20 occurrences/sec, independent of backlog. Run() dequeues one FOccurrence, evaluates it against every candidate, then always sleeps tickTimer before looping — even if the queue is already backed up.
		Replace the poll-sleep with an FEvent-based wait that's triggered on Enqueue, and drain the whole backlog (or a batch) per wake instead of one-then-sleep.
		This sleep also adds up to 50ms to every occurrence's data age (see AO.Purpose.SnapshotAgeWarningSeconds)

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

bool FPurposeEvaluationThread::QueuePurpose(FOccurrence&& potentialPurposesToQueue)
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

bool FPurposeEvaluationThread::SelectPurposeIfPossible(const FOccurrence& occurrenceToEvaluate)
{
	/// Data age test, first half: how long the occurrence waited in the queue
	const double snapshotAgeAtStart = FPlatformTime::Seconds() - occurrenceToEvaluate.snapshotTimeSeconds;
	maxSnapshotAgeSeconds = FMath::Max(maxSnapshotAgeSeconds, snapshotAgeAtStart);
	if (snapshotAgeAtStart > CVarPurposeSnapshotAgeWarning.GetValueOnAnyThread())
	{
		Global::LogError(LogOccurrence, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Occurrence caused by %s evaluated with data %.3fs old! (max seen %.3fs)"), *occurrenceToEvaluate.instigatorName, snapshotAgeAtStart, maxSnapshotAgeSeconds);
	}
	else
	{
		Global::Log(DATADEBUG, LogOccurrence, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Occurrence caused by %s evaluated with data %.3fs old (max seen %.3fs). %d candidates."), *occurrenceToEvaluate.instigatorName, snapshotAgeAtStart, maxSnapshotAgeSeconds, occurrenceToEvaluate.candidates.Num());
	}

	/// Every subject's snapshot except the candidate is shared by every candidate
	FSubjectDataView subjectData;
	for (const TPair<ESubject, FDataMap>& subject : occurrenceToEvaluate.subjectSnapshots)
	{
		subjectData.Set(subject.Key, &subject.Value);
	}

	/// Since we have a context to evaluate against, we evaluate every candidate for a new purpose in response to the context
	for (const FCandidateSnapshot& candidate : occurrenceToEvaluate.candidates)
	{
		/// We do not want instigators reacting to their own occurrences
		if (occurrenceToEvaluate.instigatorKey == candidate.componentKey)
		{
			continue;
		}

		/// Refactor: Current Behavior; Both AI are heading on the patrol task (only 1 is supposed to but it seems activity data isn't updating quickly enough).
			/// But when AI2 is reacting to occurrence of AI1, their currenth patrol behavior has a higher conditionValue than what it would after the data updates
				///	I think the data isn't being updated quickly enough because each occurrence loops through each AI now. So while the purpose is being selected and passed back, we're still evaluating the same occurrence but with the next candidate
				/// With snapshots this is now explicit: every candidate of an occurrence is scored against the data as it was when the occurrence was raised

		/// We establish the current high conditionValue as the high conditionValue of the current objective to pre-filter any lesser purposes
		float highScore = candidate.currentScore;
		const FPurpose* highScorePurpose = nullptr;/// Pointer into the purpose cache, the purpose is copied once for the winner

		/// Firstly we need to add the candidate to the context subjects
		subjectData.Set(ESubject::Candidate, &candidate.data);

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
				, *candidate.debugName
				, totalConditions
			);

			for (const TObjectPtr<UCondition>& condition : potentialPurpose.GetConditions())
			{
				if (!condition)
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

				float conditionValue = condition->InitiateEvaluation(subjectData);///Get a baseline conditionValue for condition
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

			Global::Log(DATAESSENTIAL, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Candidate %s. Score of %s is %f. High score: %f. Instigator %s.")
				, *candidate.debugName
				, *potentialPurpose.descriptionOfPurpose
				, finalScore
				, highScore
				, *occurrenceToEvaluate.instigatorName
			);

			if (finalScore > highScore)
			{
				Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Candidate %s. %s has high conditionValue of %f.")
					, *candidate.debugName
					, *potentialPurpose.descriptionOfPurpose
					, finalScore
				);

				/// Ensure new high conditionValue is reflected
				highScore = finalScore;
				highScorePurpose = &potentialPurpose;
			}
		}

		/// if highScorePurpose was set, a purpose was found
		if (highScorePurpose)
		{
			if (!highScorePurpose->behaviorAbility) {
				continue; }/// If the behavior is invalid, we shouldn't be receiving calls to this

			/// Refactor: Identical Behaviors Time-Gated Occurrences; This skip intentionally ignores the occurrence's instigator, since conditions couldn't yet tell "cover from P" apart from "cover from Q"
				/// So a second shooter hitting the same target with the same winning behavior is skipped rather than starting a new behavior against the new instigator
				/// With the cover conditions this can be told apart: compare intent + reference instead, and remove this skip when identical behaviors and time-gated occurrences are refactored
			if (occurrenceToEvaluate.eventTargetKey != TObjectKey<UObject>() && occurrenceToEvaluate.eventTargetKey == candidate.currentEventTarget /// If the target
				&& highScorePurpose->behaviorAbility.Get() == candidate.currentBehaviorAbility)/// And the ability are both the same
				///Then we don't want to restart behavior, and we don't want it passed back to the Game Thread at all
			{
				Global::Log(DATADEBUG, PURPOSE, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Incoming purpose %s already being performed by %s.")
					, *highScorePurpose->descriptionOfPurpose
					, *candidate.debugName
				);
				continue;
			}

			/// So now we want to pass the purpose back to the owner and game thread
			FSubjectMap subjects = occurrenceToEvaluate.subjectMap;
			subjects.subjects.Add(ESubject::Candidate, TWeakObjectPtr<UObject>(candidate.component));

			FContextData context(
				*highScorePurpose
				, subjects
				, occurrenceToEvaluate.contextData
				, candidate.component
				, candidate.debugName
			);

			context.cachedScoreOfPurpose = highScore;
			context.snapshotTimeSeconds = occurrenceToEvaluate.snapshotTimeSeconds;

			CreateAsyncTask_PurposeSelected(MoveTemp(context));
		}
		else
		{
			Global::Log(DATADEBUGTRIVIAL, LogOccurrence, "FPurposeEvaluationThread", "SelectPurposeIfPossible", TEXT("Purpose not found for %s.")
				, *candidate.debugName
			);
		}
	}

	Global::Log(DATADEBUGTRIVIAL, LogOccurrence, "PurposeSystem", "Occurrence", TEXT("End of Occurrence caused by %s. Evaluation took %.3fs."), *occurrenceToEvaluate.instigatorName, FPlatformTime::Seconds() - occurrenceToEvaluate.snapshotTimeSeconds - snapshotAgeAtStart);

	return false;
}

bool FPurposeEvaluationThread::CreateAsyncTask_PurposeSelected(FContextData context)
{
	TGraphTask<FAsyncGraphTask_PurposeSelected>::CreateTask().ConstructAndDispatchWhenReady(MoveTemp(context));
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
}

#pragma endregion

#pragma region Companion Thread

FCompanionThread::~FCompanionThread()
{
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
