// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Misc/AssertionMacros.h"//For PrintFImpl
#include "Misc/VarArgs.h"//For PrintFImpl
#include "HAL/UnrealMemory.h"//For PrintFImpl
#include "Engine/EngineBaseTypes.h"/// For ENetMode
#include "GameFramework/Actor.h"/// For Network information about Actor
#include "Engine/GameEngine.h"
#include "GlobalLog.generated.h"

UENUM()
//A hierarchical level of verbosity for logs
//Verbosities will be log if they are less than Global::LogVerbosity
enum class EHierarchicalCalltraceVerbosity : uint8
{
	None = 0//Verbosity indicator to prevent logs
	, Core = 1//Verbosity for lightweight, only essential data logging
	, EssentialHeaderTrace = 2//Verbosity for tracing essential function calls
	, Debug = 3//Verbosity for slightly more in depth data logging than Info
	, DebugTrivial = 4//Verbosity for slightly more in depth data logging than Info
	, TrivialHeaderTrace = 5//Verbosity for tracing non essential function calls
	, TrivialData = 6//Verbosity for tracing all function calls
	, FullTrace = 7//Verbosity for tracing callstack in log
};

///So we don't have to call EHierarchicalCalltraceVerbosity:: every time

/// Logging of data only at pivotal logical moments, such as when a character performs an Action
#define DATAESSENTIAL EHierarchicalCalltraceVerbosity::Core
/// Meant to log the callstack of foundational methods, such as BeginPlay, (Insert Purpose Type)Received, etc.
#define CALLTRACEESSENTIAL EHierarchicalCalltraceVerbosity::EssentialHeaderTrace
/// Without trivial data, basic logging of debug data, such as score of a purpose, or distance to a target, etc.
#define DATADEBUG EHierarchicalCalltraceVerbosity::Debug
/// Most in depth logging of data, such as score of each condition of a purpose, etc.
#define DATADEBUGTRIVIAL EHierarchicalCalltraceVerbosity::DebugTrivial
/// Logging of less critical data that is context sensitive, such as number of targets found by behavior, or how a variable changes throughout a method (such as score being adjusted by a curve for condition)
#define CALLTRACETRIVIAL EHierarchicalCalltraceVerbosity::TrivialHeaderTrace
/// Logging of less critical data that is context sensitive, such as number of targets found by behavior, or how a variable changes throughout a method (such as score being adjusted by a curve for condition)
#define DATATRIVIAL EHierarchicalCalltraceVerbosity::TrivialData
/// All methods and all data should be logged
#define FULLTRACE EHierarchicalCalltraceVerbosity::FullTrace

//https://blog.jamie.holdings/2020/04/21/unreal-engine-4-custom-log-categories/
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogManagement, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogBehavior, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogPurpose, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogEvent, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogAbility, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogPlayer, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogEnvQuery, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogNetworking, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogWeapon, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogWarzone, Log, All);
AO_API DECLARE_LOG_CATEGORY_EXTERN(LogData, Log, All);

/// General logging of Purpose System
#define PURPOSE LogPurpose
/// Strictly for logging of management of characters
#define MANAGEMENT LogManagement
/// Logging of Ability_Behavior information
#define BEHAVIOR LogBehavior
/// Logging of world query information, such as querying distance between actors, or number of nearby actors
#define EQS LogEnvQuery
/// Logging of Event specific Purpose system information
#define EVENT LogEvent
/// Logging of Ability specific Purpose system information
#define TASK LogAbility
/// Logging of Player information
#define PLAYER LogPlayer
/// Logging of Network related logic information
#define NETWORK LogNetworking
/// Logging strictly of animation information such as keyframe data, or anim instance information
#define ANIMATION LogAnimation

/// LOGTOSCREEN(0.0f, “This is formatted %i %f”, 100, 99.999)):
/// @param displayTime: Length of time message displays to screen
/// @param x: String to display
/// @param ...: arguments of string to display
#define LOGTOSCREEN( displayTime, x, ...) if(GEngine){GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Orange, FString::Printf(TEXT(x), __VA_ARGS__));}

/*

https://forums.unrealengine.com/t/ue_log-with-format-as-variable/157314/8
#define LOG(x, ...) UE_LOG(MBT, Log, TEXT(x), __VA_ARGS__)
*/

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Log Settings"))
class AO_API UGlobalLogSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGlobalLogSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category = "Log")
		/// <summary>
		/// Determines the max verbosity of Global Logs allowed to print to log
		/// </summary>
		EHierarchicalCalltraceVerbosity GlobalLogVerbosity = EHierarchicalCalltraceVerbosity::Core;

};

namespace Global
{

#pragma region Log

	/*Printf Formatting
	template <typename FmtType, typename... Types>
	UE_NODISCARD static FString Printf(const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FString::Printf");
	
		return PrintfImpl(Fmt, Args...);
	}
	*/

	/*Log Formating
	/// Internal version of log function. Should be used only in logging macros, as it relies on caller to call assert on fatal error
	template <typename FmtType, typename... Types>
	static void Logf_Internal(const ANSICHAR* File, int32 Line, const FLogCategoryName& Category, ELogVerbosity::Type Verbosity, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FMsg::Logf_Internal");

		Logf_InternalImpl(File, Line, Category, Verbosity, Fmt, Args...);
	}
	*/

	///// @param VerbosityCache: Out parameter, set to current Global::HierarchicalVerbosity
	//static void CacheAndSetLogVerbosity(EHierarchicalCalltraceVerbosity& VerbosityCache, EHierarchicalCalltraceVerbosity inVerbosity)
	//{

	//	const UGlobalLogSettings* LogSettings = GetDefault<UGlobalLogSettings>();

	//	if (LogSettings)
	//	{
	//		VerbosityCache = LogSettings->GlobalLogVerbosity;
	//		LogSettings->GlobalLogVerbosity = inVerbosity;
	//	}
	//}

	//static void ResetLogVerbosity(EHierarchicalCalltraceVerbosity VerbosityCache) { Global::HierarchicalVerbosity = VerbosityCache; }
	template<typename T>
	static FString EnumWithValue(T inValue)
	{
		return UEnum::GetValueAsString(inValue);
	}

	template<typename T>
	static FString EnumValueOnly(T inValue)
	{
		return StaticEnum<T>()->GetNameStringByValue((int64)inValue);
	}

	template<typename T>
	static FString EnumValueOnly(int64 inValue)
	{
		return StaticEnum<T>()->GetNameStringByValue(inValue);
	}

	/// As ENetMode is not a UENUM, it cannot be used for StaticEnum<>
	static FString NetModeAsString(ENetMode inMode)
	{
		switch (inMode)
		{
		case ENetMode::NM_ListenServer:
			return "ListenServer";
			break;
		case ENetMode::NM_Client:
			return "Client";
			break;
		case ENetMode::NM_DedicatedServer:
			return "DedicatedServer";
			break;
		case ENetMode::NM_Standalone:
			return "Standalone";
			break;
		}

		return "Unknown";
	}

	template <typename FmtType, typename... Types>
	///@param verbosity: Custom verbosity with priorities. Verbosities with higher priority than one passed in will not print
	///Verbosity level is set in Global.cpp (EHierarchicalCalltraceVerbosity HierarchicalVerbosity) 
	static void Log(EHierarchicalCalltraceVerbosity verbosity, const FLogCategoryBase& logCat, UObject& caller, FString methodName, const FmtType& Fmt, Types... Args)
	{
		const UGlobalLogSettings* LogSettings = GetDefault<UGlobalLogSettings>();

		if (!LogSettings)
		{
			return;
		}

		if (static_cast<int>(LogSettings->GlobalLogVerbosity) < static_cast<int>(verbosity))
		{
			//		UE_LOG(LogTemp, Log, TEXT("HV: %d ; V: %d"), static_cast<int>(HierarchicalVerbosity), static_cast<int>(verbosity));
			return;
		}///Verbosity is too high, so we don't want to log

		//static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		//static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to Global::Log");

		//FString inString = FString::Printf(Fmt, Args...);
		////FString inString = CustomPrintfImpl(Fmt, Args...);

		//FMsg::Logf_Internal(nullptr, 0, logCat.GetCategoryName(), ELogVerbosity::Log, TEXT("%s: %s %s::%s"), *Global::EnumValueOnly<EHierarchicalCalltraceVerbosity>(verbosity), *inString, *caller.GetName(), *methodName);
	}	
	
	template <typename FmtType, typename... Types>
	static void LogError(const FLogCategoryBase& logCat, UObject& caller, FString methodName, const FmtType& Fmt, Types... Args)
	{
		//static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		//static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to Global::Log");

		//FString inString = FString::Printf(Fmt, Args...);
		//FMsg::Logf_Internal(nullptr, 0, logCat.GetCategoryName(), ELogVerbosity::Error, TEXT("%s %s::%s"), *inString, *caller.GetName(), *methodName);
	}

	template <typename FmtType, typename... Types>
	///@param verbosity: Custom verbosity with priorities. Verbosities with higher priority than one passed in will not print
	///Verbosity level is set in Global.cpp (EHierarchicalCalltraceVerbosity HierarchicalVerbosity) 
	static void Log(EHierarchicalCalltraceVerbosity verbosity, const FLogCategoryBase& logCat, FString caller, FString methodName, const FmtType& Fmt, Types... Args)
	{
		const UGlobalLogSettings* LogSettings = GetDefault<UGlobalLogSettings>();

		if (!LogSettings)
		{
			return;
		}

		if (static_cast<int>(LogSettings->GlobalLogVerbosity) < static_cast<int>(verbosity))
		{
			//		UE_LOG(LogTemp, Log, TEXT("HV: %d ; V: %d"), static_cast<int>(HierarchicalVerbosity), static_cast<int>(verbosity));
			return;
		}///Verbosity is too high, so we don't want to log

		//static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		//static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to Global::Log");

		//FString inString = FString::Printf(Fmt, Args...);
		//FMsg::Logf_Internal(nullptr, 0, logCat.GetCategoryName(), ELogVerbosity::Log, TEXT("%s: %s %s::%s"), *Global::EnumValueOnly<EHierarchicalCalltraceVerbosity>(verbosity), *inString, *caller, *methodName);
	}	
	
	template <typename FmtType, typename... Types>
	///
	static void LogError(const FLogCategoryBase& logCat, FString caller, FString methodName, const FmtType& Fmt, Types... Args)
	{
		//static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
		//static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to Global::Log");

		//FString inString = FString::Printf(Fmt, Args...);
		//FMsg::Logf_Internal(nullptr, 0, logCat.GetCategoryName(), ELogVerbosity::Error, TEXT("%s %s::%s"), *inString, *caller, *methodName);
	}

	//Receiving an error when attempting to pass in a custom log category
	/*template <typename FmtType, typename... Types>
	void Log(const FLogCategoryName& category, UObject& caller, FString methodName, const FmtType& Fmt, Types... Args);*/

	

	/// Return details relevant to network identification of inActor
	/// @param includeAdvancedDetails: == true will result in additional information such as roles
	static FString NetworkInfoOnActor(TObjectPtr<AActor> inActor, bool includeAdvancedDetails = false)
	{

		FString string = "";

		string.Append(FString::Printf(TEXT("NetMode: %s. "), *Global::NetModeAsString(inActor->GetNetMode())));
		string.Append(FString::Printf(TEXT("FullName: %s. "), *inActor->GetFullName()));
		string.Append(FString::Printf(TEXT("NetOwner: %s. "), IsValid(inActor->GetNetOwner()) ? *inActor->GetNetOwner()->GetName() : TEXT("Invalid")));

		if (includeAdvancedDetails)
		{
			string.Append(FString::Printf(TEXT("%s. "), inActor->IsNameStableForNetworking() ? TEXT("Name is stable for networking") : TEXT("Name is not stable for networking")));
			string.Append(FString::Printf(TEXT("%s. "), inActor->IsFullNameStableForNetworking() ? TEXT("Full name is stable for networking") : TEXT("Full name is not stable for networking")));
			string.Append(FString::Printf(TEXT("Local Owner: %s. "), inActor->GetOwner() ? *inActor->GetOwner()->GetName() : TEXT("Invalid")));
			string.Append(FString::Printf(TEXT("Remote Role: %s. "), *Global::EnumValueOnly<ENetRole>(inActor->GetRemoteRole())));
			string.Append(FString::Printf(TEXT("Local Role: %s. "), *Global::EnumValueOnly<ENetRole>(inActor->GetLocalRole())));
		}

		return string;
	}


#pragma endregion

}

