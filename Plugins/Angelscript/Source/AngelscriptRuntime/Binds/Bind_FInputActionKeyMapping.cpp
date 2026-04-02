#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "GameFramework/PlayerInput.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FInputActionKeyMapping(FAngelscriptBinds::EOrder::Late, []
{
	auto FInputActionKeyMapping_ = FAngelscriptBinds::ExistingClass("FInputActionKeyMapping");

	FInputActionKeyMapping_.Method("bool opEquals(const FInputActionKeyMapping& Other) const", METHOD_TRIVIAL(FInputActionKeyMapping, operator==));
});
