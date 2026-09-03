#include "HansaAutomationModule.h"

#include "Endpoint/HansaAutomationNamedPipeEndpoint.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Session/HansaAutomationSessionService.h"

#if UE_BUILD_SHIPPING
#error HansaAutomation must never be compiled into a Shipping target.
#endif

#if !WITH_HANSA_AUTOMATION
#error HansaAutomation requires WITH_HANSA_AUTOMATION=1 in approved development targets.
#endif

DEFINE_LOG_CATEGORY(LogHansaAutomation);

namespace Hansa::Automation
{
	constexpr TCHAR ConfigSection[] = TEXT("Hansa.Automation");
	constexpr TCHAR EnableTransportKey[] = TEXT("bEnableTransport");
	constexpr TCHAR EnableCommandLineFlag[] = TEXT("HansaAutomation");
	constexpr TCHAR PermissionKey[] = TEXT("MaximumPermission");
	constexpr TCHAR PermissionCommandLineValue[] = TEXT("HansaAutomationPermission=");
	constexpr TCHAR AuthenticationTokenEnvironmentVariable[] = TEXT("HANSA_AUTOMATION_TOKEN");
	constexpr TCHAR PipeNameEnvironmentVariable[] = TEXT("HANSA_AUTOMATION_PIPE");

	bool IsValidStartupToken(const FString& Token)
	{
		if (Token.Len() < 16 || Token.Len() > 128)
		{
			return false;
		}
		for (const TCHAR Character : Token)
		{
			if (FChar::IsWhitespace(Character))
			{
				return false;
			}
		}
		return true;
	}

	bool IsValidPipeName(const FString& PipeName)
	{
		if (PipeName.IsEmpty() || PipeName.Len() > 64)
		{
			return false;
		}
		for (const TCHAR Character : PipeName)
		{
			if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('_') || Character == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}
}

void FHansaAutomationModule::StartupModule()
{
	bool bConfigEnabled = false;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(
			Hansa::Automation::ConfigSection,
			Hansa::Automation::EnableTransportKey,
			bConfigEnabled,
			GGameIni);
	}

	const bool bCommandLineEnabled = FParse::Param(
		FCommandLine::Get(),
		Hansa::Automation::EnableCommandLineFlag);

	bTransportRequested = bConfigEnabled || bCommandLineEnabled;

	FString PermissionText = TEXT("ReadOnly");
	if (GConfig != nullptr)
	{
		GConfig->GetString(
			Hansa::Automation::ConfigSection,
			Hansa::Automation::PermissionKey,
			PermissionText,
			GGameIni);
	}
	FParse::Value(
		FCommandLine::Get(),
		Hansa::Automation::PermissionCommandLineValue,
		PermissionText);

	Hansa::Automation::EHansaAutomationPermissionLevel MaximumPermission =
		Hansa::Automation::EHansaAutomationPermissionLevel::ReadOnly;
	const bool bPermissionValid = Hansa::Automation::TryParsePermissionLevel(PermissionText, MaximumPermission);

	const FString AuthenticationToken = FPlatformMisc::GetEnvironmentVariable(
		Hansa::Automation::AuthenticationTokenEnvironmentVariable);
	const FString PipeName = FPlatformMisc::GetEnvironmentVariable(
		Hansa::Automation::PipeNameEnvironmentVariable);
	const bool bTokenValid = Hansa::Automation::IsValidStartupToken(AuthenticationToken);
	const bool bPipeNameValid = Hansa::Automation::IsValidPipeName(PipeName);
	bool bSessionEnabled = bTransportRequested && bPermissionValid && bTokenValid && bPipeNameValid;
	SessionService = MakeUnique<Hansa::Automation::FHansaAutomationSessionService>(
		Hansa::Automation::FHansaAutomationSessionSettings::CreateDefault(
			bSessionEnabled,
			MaximumPermission,
			AuthenticationToken));

	if (bSessionEnabled)
	{
		NamedPipeEndpoint = MakeUnique<Hansa::Automation::FHansaAutomationNamedPipeEndpoint>(PipeName, *SessionService);
		if (!NamedPipeEndpoint->Start())
		{
			NamedPipeEndpoint.Reset();
			SessionService = MakeUnique<Hansa::Automation::FHansaAutomationSessionService>(
				Hansa::Automation::FHansaAutomationSessionSettings::CreateDefault(
					false,
					MaximumPermission,
					AuthenticationToken));
			bSessionEnabled = false;
		}
		else
		{
			UE_LOG(
				LogHansaAutomation,
				Display,
				TEXT("Hansa automation named-pipe endpoint is enabled with maximum permission '%s'."),
				Hansa::Automation::LexToString(MaximumPermission));
		}
	}
	if (!bSessionEnabled && bTransportRequested)
	{
		UE_LOG(
			LogHansaAutomation,
			Error,
			TEXT("Hansa automation was requested but remains disabled because its permission, short-lived token, pipe name, or endpoint startup is invalid."));
	}
	else if (!bTransportRequested)
	{
		UE_LOG(
			LogHansaAutomation,
			Verbose,
			TEXT("Hansa automation is disabled. Explicit non-Shipping enablement and a short-lived token are required."));
	}
}

void FHansaAutomationModule::ShutdownModule()
{
	NamedPipeEndpoint.Reset();
	SessionService.Reset();
	bTransportRequested = false;
}

bool FHansaAutomationModule::IsSessionBoundaryEnabled() const
{
	return SessionService.IsValid() && SessionService->IsEnabled();
}

Hansa::Automation::FHansaAutomationSessionService& FHansaAutomationModule::GetSessionService()
{
	check(SessionService.IsValid());
	return *SessionService;
}

const Hansa::Automation::FHansaAutomationSessionService& FHansaAutomationModule::GetSessionService() const
{
	check(SessionService.IsValid());
	return *SessionService;
}

IMPLEMENT_MODULE(FHansaAutomationModule, HansaAutomation)
