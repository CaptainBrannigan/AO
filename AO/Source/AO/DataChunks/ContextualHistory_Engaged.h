// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/ContextualHistory.h"
#include "Data/GlobalLog.h"
#include "ContextualHistory_Engaged.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_1);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_2);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_3);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_4);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_5);

UCLASS(MinimalAPI)
/// <summary>
/// ContextualHistory_Engaged is meant to be a datamodifier only, which can automatically track the number of engagements on a subject
/// </summary>
class UContextualHistory_Engaged : public UContextualHistory
{
	GENERATED_BODY()
public:

	UContextualHistory_Engaged() 
	{
		tagForAdjustment = TAG_Contextual_Combat_Engaged;
	}

	/// Override this in derived datachunk classes to allow evaluation of potential child tags from a parent
	virtual FGameplayTag GetTagForAdjustment(const TArray<FContextHistoryEntry>& currentContextualHistoryForCandidate) const
	{
		/// Here we're trying to determine what the highest count of engagements is and ensure it's time of updated
			/// However, as we don't have any unique identifiers, engagements can come from the same candidate
			/// So think of this as "times engaged" rather than "number of engaged combatants"
			
		FGameplayTag outTag = TAG_Contextual_Combat_Engaged_1;
		int highestEngagementCount = 0;

		for (const FContextHistoryEntry& entry : currentContextualHistoryForCandidate)
		{
			Global::Log(DATATRIVIAL, LogData, "UContextualHistory_Engaged", "GetTagForAdjustment", TEXT("Tag: %s. Time since: %f")
				, *entry.contextualTag.ToString()
				, (FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds()
			);

			if (entry.contextualTag.MatchesTag(TAG_Contextual_Combat_Engaged) )
			{
				TCHAR endChar = entry.contextualTag.ToString()[entry.contextualTag.ToString().Len() - 1];
				
				int engagedCount = FCString::Atoi(&endChar);

				/// By detracting the highest engaged count from the relevancy test
				/// Higher engaged counts will become irrelevant more quickly while lower engaged counts remain relevant longer
				/// This relates best to how often the subject is being engaged
				/// We firstly do engagedCount*2 to create a bigger discrepancy between the engagedCount timeSince values
				/// Currently max engaged count is 5, so 5*2 with the addition of 1 provides a flat 1 second buffer for max engagedCounts
				/// And with the higher discrepancy for lower values, that gives them a longer time to be considered relevant the lower engagedCount is
				if ((FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds() < (11 - engagedCount*2) && engagedCount > highestEngagementCount)
				{
					Global::Log(DATATRIVIAL, LogData, "UContextualHistory_Engaged", "GetTagForAdjustment", TEXT("Engaged count is %d."), engagedCount);
					highestEngagementCount = engagedCount;
				}
			}
		}

		/// We have the last relevant engagement count
		/// We increase because this modification represents an additional engagement
		highestEngagementCount++;

		highestEngagementCount = FMath::Clamp(highestEngagementCount, 0, 5);

		/// We could just append highestEngagementCount to the string of the combat_engaged tag, but that would take unnecessary processing power
		/// We have to define the tags for them to exist anyways, so we may as well just use an cheap switch statement
		switch (highestEngagementCount)
		{
			default: case 0: case 1: outTag = TAG_Contextual_Combat_Engaged_1; break;
			case 2: outTag = TAG_Contextual_Combat_Engaged_2; break;
			case 3: outTag = TAG_Contextual_Combat_Engaged_3; break;
			case 4: outTag = TAG_Contextual_Combat_Engaged_4; break;
			case 5: outTag = TAG_Contextual_Combat_Engaged_5; break;
		}

		return outTag;

		/// We can't use an int for each tag, as once a child is returned, the former will never be updated until child is out of time zone
		/*		
		int engagements = 0;

		for (const FContextHistoryEntry& entry : currentContextualHistoryForCandidate)
		{
			Global::Log(DATATRIVIAL, LogData, "UContextualHistory_Engaged", "GetTagForAdjustment", TEXT("Tag: %s. Time since: %f")
				, *entry.contextualTag.ToString()
				, (FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds()
			);
			if (entry.contextualTag.MatchesTag(TAG_Contextual_Combat_Engaged) && (FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds() < 5)
			{
				Global::Log(DATATRIVIAL, LogData, "UContextualHistory_Engaged", "GetTagForAdjustment", TEXT("Increasing engaged targets.")
				);
				engagements++;
			}
		}

		Global::Log(DATATRIVIAL, LogData, "UContextualHistory_Engaged", "GetTagForAdjustment", TEXT("Engaged targets: %d")
			, engagements
		);
		/// We could simply append engagements as a string to the gameplay tag, but that's unecessary processing
		/// This automatically handles how many actors are engaging the subject who is being adjusted so we don't have to check for ourselves
		/// Remember engagements represents the number of unique tags of Combat_Engaged
		switch (engagements)
		{
			default: case 0: return TAG_Contextual_Combat_Engaged; break;
			case 1: return TAG_Contextual_Combat_Engaged_2; break;
			case 2: return TAG_Contextual_Combat_Engaged_3; break;
			case 3: return TAG_Contextual_Combat_Engaged_4; break;
			case 4: return TAG_Contextual_Combat_Engaged_5; break;
		}*/
	}

};
