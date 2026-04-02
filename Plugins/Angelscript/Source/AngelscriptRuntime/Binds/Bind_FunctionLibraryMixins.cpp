#include "AngelscriptBinds.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/LevelStreaming.h"

#include "FunctionLibraries/RuntimeCurveLinearColorMixinLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FunctionLibraryMixins((int32)FAngelscriptBinds::EOrder::Late + 110, []
{
	auto LevelStreaming_ = FAngelscriptBinds::ExistingClass("ULevelStreaming");
#if WITH_EDITOR
	LevelStreaming_.Method("bool GetShouldBeVisibleInEditor() const", METHOD_TRIVIAL(ULevelStreaming, GetShouldBeVisibleInEditor));
#endif

	auto RuntimeCurveLinearColor_ = FAngelscriptBinds::ExistingClass("FRuntimeCurveLinearColor");
	RuntimeCurveLinearColor_.Method(
		"void AddDefaultKey(float32 InTime, FLinearColor InColor)",
		[](FRuntimeCurveLinearColor* Target, float InTime, const FLinearColor& InColor)
		{
			Target->ColorCurves[0].AddKey(InTime, InColor.R);
			Target->ColorCurves[1].AddKey(InTime, InColor.G);
			Target->ColorCurves[2].AddKey(InTime, InColor.B);
			Target->ColorCurves[3].AddKey(InTime, InColor.A);
		});

	FAngelscriptBinds::FNamespace RuntimeCurveLinearColorHelperNs("URuntimeCurveLinearColorMixinLibrary");
	FAngelscriptBinds::BindGlobalFunction(
		"void AddDefaultKey(FRuntimeCurveLinearColor& Target, float32 InTime, FLinearColor InColor)",
		[](FRuntimeCurveLinearColor& Target, float InTime, const FLinearColor& InColor)
		{
			URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(Target, InTime, InColor);
		});
});
