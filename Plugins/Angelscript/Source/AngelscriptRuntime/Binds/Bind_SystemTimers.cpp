#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Kismet/KismetSystemLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_SystemTimers((int32)FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptBinds::FNamespace System_("System");

	FAngelscriptBinds::BindGlobalFunction(
		"FTimerHandle SetTimer(const UObject Object, const FName& FunctionName, float32 Time, bool bLooping = false)",
		[](const UObject* Object, const FName& FunctionName, float Time, bool bLooping)
		{
			return UKismetSystemLibrary::K2_SetTimer(const_cast<UObject*>(Object), FunctionName.ToString(), Time, bLooping, false, 0.f, 0.f);
		});

	FAngelscriptBinds::BindGlobalFunction(
		"bool IsTimerPausedHandle(FTimerHandle Handle)",
		[](FTimerHandle Handle)
		{
			return UKismetSystemLibrary::K2_IsTimerPausedHandle(FAngelscriptEngine::CurrentWorldContext, Handle);
		});
	FAngelscriptBinds::SetPreviousBindRequiresWorldContext(true);

	FAngelscriptBinds::BindGlobalFunction(
		"void PauseTimerHandle(FTimerHandle Handle)",
		[](FTimerHandle Handle)
		{
			UKismetSystemLibrary::K2_PauseTimerHandle(FAngelscriptEngine::CurrentWorldContext, Handle);
		});
	FAngelscriptBinds::SetPreviousBindRequiresWorldContext(true);

	FAngelscriptBinds::BindGlobalFunction(
		"void UnPauseTimerHandle(FTimerHandle Handle)",
		[](FTimerHandle Handle)
		{
			UKismetSystemLibrary::K2_UnPauseTimerHandle(FAngelscriptEngine::CurrentWorldContext, Handle);
		});
	FAngelscriptBinds::SetPreviousBindRequiresWorldContext(true);

	FAngelscriptBinds::BindGlobalFunction(
		"void ClearAndInvalidateTimerHandle(FTimerHandle& Handle)",
		[](FTimerHandle& Handle)
		{
			UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(FAngelscriptEngine::CurrentWorldContext, Handle);
		});
	FAngelscriptBinds::SetPreviousBindRequiresWorldContext(true);
});
