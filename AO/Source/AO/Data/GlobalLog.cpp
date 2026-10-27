// Copyright Jordan Cain. All Rights Reserved.


#include "GlobalLog.h"

DEFINE_LOG_CATEGORY(LogManagement);
DEFINE_LOG_CATEGORY(LogPurpose);
DEFINE_LOG_CATEGORY(LogEvent);
DEFINE_LOG_CATEGORY(LogAbility);
DEFINE_LOG_CATEGORY(LogEnvQuery);
DEFINE_LOG_CATEGORY(LogPlayer);
DEFINE_LOG_CATEGORY(LogNetworking);
DEFINE_LOG_CATEGORY(LogBehavior);
DEFINE_LOG_CATEGORY(LogWeapon);
DEFINE_LOG_CATEGORY(LogWarzone);
DEFINE_LOG_CATEGORY(LogData);

#pragma region Log

UGlobalLogSettings::UGlobalLogSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion