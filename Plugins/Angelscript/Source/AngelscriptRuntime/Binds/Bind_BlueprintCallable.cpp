#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptDocs.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "ClassGenerator/ASClass.h"

#include "Helper_FunctionSignature.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

extern void RegisterBlueprintEventByScriptName(UClass* Class, const FString& ScriptName, UFunction* Function);

namespace
{
	bool IsGlobalFunctionAlreadyBound(const FString& NamespaceName, const FString& Declaration)
	{
		auto* ScriptEngine = FAngelscriptEngine::Get().GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		if (NamespaceName.IsEmpty())
		{
			return ScriptEngine->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*Declaration)) != nullptr;
		}

		FAngelscriptBinds::FNamespace Namespace(NamespaceName);
		return ScriptEngine->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*Declaration)) != nullptr;
	}

	bool IsMethodAlreadyBound(const FString& ClassName, const FString& Declaration)
	{
		auto* ScriptEngine = FAngelscriptEngine::Get().GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*ClassName));
		return TypeInfo != nullptr && TypeInfo->GetMethodByDecl(TCHAR_TO_ANSI(*Declaration)) != nullptr;
	}

	bool IsMethodNameAlreadyBound(const FString& ClassName, const FString& MethodName)
	{
		auto* ScriptEngine = FAngelscriptEngine::Get().GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*ClassName));
		return TypeInfo != nullptr && TypeInfo->GetMethodByName(TCHAR_TO_ANSI(*MethodName)) != nullptr;
	}
}

// Bind a native function to angelscript, provided all
// argument and return types are known as FAngelscriptTypes.
void BindBlueprintCallable(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptMethodBind& DBBind
#if !AS_USE_BIND_DB
	, const TCHAR* OverrideName
#endif
)
{
#if !AS_USE_BIND_DB
	// Don't bind functions that aren't native
	if (!Function->HasAnyFunctionFlags(FUNC_Native))
		return;

	if (FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(Function))
		return;
#endif

	UClass* OwningClass = CastChecked<UClass>(Function->GetOuter());
	FFuncEntry* Entry = nullptr;

	if (OwningClass != nullptr)
	{
		FString Name = Function->GetFName().ToString();
		auto* map = FAngelscriptBinds::GetClassFuncMaps().Find(OwningClass);

		if (map)
			Entry = map->Find(Name);
	}

	// Don't bind functions without a native pointer
	if (Entry == nullptr)
		return;

	auto* DirectNativePointer = &Entry->FuncPtr;
	if (DirectNativePointer == nullptr || !DirectNativePointer->IsBound())
		return;

	//auto* DirectNativePointer = &FuncInMap->Key;	
	//if (!DirectNativePointer->IsBound())
	//	return;	

#if AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature;
	Signature.InitFromDB(InType, Function, DBBind, /* bInitTypes= */ false);

#elif !AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature(InType, Function, OverrideName);

	// Don't bind things that have types that are unknown to us
	if (!Signature.bAllTypesValid)
		return;
#endif

	// FGenericFuncPtr is a copy of asSFuncPtr, so do a direct memcpy
	asSFuncPtr ASFuncPtr;
	static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must be the same struct as asSFuncPtr");
	FMemory::Memcpy(&ASFuncPtr, DirectNativePointer, sizeof(asSFuncPtr));

	// Actually bind into angelscript engine
	if (Signature.bStaticInScript)
	{
		const bool bAlreadyBoundInGlobalScope = Signature.bGlobalScope && IsGlobalFunctionAlreadyBound(FString(), Signature.Declaration);
		const bool bAlreadyBoundInNamespace = IsGlobalFunctionAlreadyBound(Signature.ClassName, Signature.Declaration);

		// Some functions have a meta tag to put them in global scope
		if (Signature.bGlobalScope && !bAlreadyBoundInGlobalScope)
		{
			//int GlobalFunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, FuncInMap->Value);			
			int GlobalFunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, Entry->Caller);
			Signature.ModifyScriptFunction(GlobalFunctionId);
		}

		// Static functions should be bound as a global function in a namespace
		if (!bAlreadyBoundInNamespace)
		{
			FAngelscriptBinds::FNamespace ns(Signature.ClassName);
			//int FunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, FuncInMap->Value);
			int FunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, Entry->Caller);
			Signature.ModifyScriptFunction(FunctionId);
		}
	}
	else if (Signature.bStaticInUnreal)
	{
		if (IsMethodAlreadyBound(Signature.ClassName, Signature.Declaration) || IsMethodNameAlreadyBound(Signature.ClassName, Signature.ScriptName))
		{
			return;
		}

		// This is a static function converted through mixin to a script member function
		int FunctionId = FAngelscriptBinds::BindMethodDirect
		(
			Signature.ClassName,
			Signature.Declaration, ASFuncPtr,
			asCALL_CDECL_OBJFIRST, Entry->Caller /*FuncInMap->Value*/
		);
		Signature.ModifyScriptFunction(FunctionId);
	}
	else
	{
		if (IsMethodAlreadyBound(InType->GetAngelscriptTypeName(), Signature.Declaration) || IsMethodNameAlreadyBound(InType->GetAngelscriptTypeName(), Signature.ScriptName))
		{
			return;
		}

		//auto caller = ASAutoCaller::FunctionCaller::Make();
		//caller.MethodPtr = DirectNativePointer;
		// Member methods should be bound as THISCALL		
		int FunctionId = FAngelscriptBinds::BindMethodDirect
		(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration, ASFuncPtr, asCALL_THISCALL, Entry->Caller /*FuncInMap->Value*/
		);
		Signature.ModifyScriptFunction(FunctionId);
	}

#if AS_CAN_GENERATE_JIT
#if AS_USE_BIND_DB
	SCRIPT_NATIVE_UFUNCTION(Function, FPackageName::ObjectPathToObjectName(DBBind.UnrealPath), Signature.bTrivial);
#else
	SCRIPT_NATIVE_UFUNCTION(Function, Function->GetName(), Signature.bTrivial);
#endif
#endif

#if !AS_USE_BIND_DB
	Signature.WriteToDB(DBBind);
#endif
}
