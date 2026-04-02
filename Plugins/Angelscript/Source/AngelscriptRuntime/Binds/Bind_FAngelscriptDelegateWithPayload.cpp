#include "AngelscriptDelegateWithPayload.h"
#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

extern FString GetSignatureStringForFunction(UFunction* Function);

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AngelscriptDelegateWithPayload((int32)FAngelscriptBinds::EOrder::Late, []
{
	auto Delegate_ = FAngelscriptBinds::ExistingClass("FAngelscriptDelegateWithPayload");

	Delegate_.Method("void ExecuteIfBound() const", &FAngelscriptDelegateWithPayload::ExecuteIfBound);
	Delegate_.Method("bool IsBound() const", &FAngelscriptDelegateWithPayload::IsBound);

	Delegate_.Method("void BindUFunction(UObject Object, const FName& FunctionName)",
	[](FAngelscriptDelegateWithPayload& Delegate, UObject* Object, const FName& FunctionName)
	{
		Delegate.BindUFunction(Object, FunctionName);
	});

	Delegate_.Method("void BindWithPayload(UObject Object, const FName& FunctionName, const ?&in Payload)",
	[](FAngelscriptDelegateWithPayload& Delegate, UObject* Object, const FName& FunctionName, void* PayloadPtr, int PayloadScriptTypeId)
	{
		Delegate.BindUFunctionWithPayload(Object, FunctionName, PayloadPtr, PayloadScriptTypeId);
	});
});

bool FAngelscriptDelegateWithPayload::IsBound() const
{
	return Object.IsValid() && !FunctionName.IsNone();
}

void FAngelscriptDelegateWithPayload::ExecuteIfBound() const
{
	if (!Object.IsValid() || FunctionName.IsNone())
	{
		return;
	}

	UFunction* Function = Object->FindFunction(FunctionName);
	if (Function == nullptr)
	{
		return;
	}

	Object->ProcessEvent(Function, Payload.IsValid() ? (void*)Payload.GetMemory() : nullptr);
}

void FAngelscriptDelegateWithPayload::BindUFunction(UObject* InObject, FName InFunctionName)
{
	if (!IsValid(InObject))
	{
		FAngelscriptEngine::Throw("Invalid object passed to BindUFunction.");
		return;
	}

	UFunction* Function = InObject->FindFunction(InFunctionName);
	if (Function == nullptr)
	{
		const FString Debug = FString::Printf(TEXT("\nCould not find function %s\nIs it declared UFUNCTION()?"), *InFunctionName.ToString());
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Debug));
		return;
	}

	if (Function->NumParms != 0 || Function->GetReturnProperty() != nullptr)
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: void FAngelscriptDelegateWithPayload()\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Function));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	Payload.Reset();
	Object = InObject;
	FunctionName = InFunctionName;
}

void FAngelscriptDelegateWithPayload::BindUFunctionWithPayload(UObject* InObject, FName InFunctionName, void* PayloadPtr, int PayloadScriptTypeId)
{
	UScriptStruct* StructType = Cast<UScriptStruct>(FAngelscriptEngine::Get().GetUnrealStructFromAngelscriptTypeId(PayloadScriptTypeId));
	if (StructType == nullptr)
	{
		StructType = FAngelscriptDelegateWithPayload::GetBoxedPrimitiveStructFromTypeId(PayloadScriptTypeId);
	}

	if (StructType == nullptr)
	{
		FAngelscriptEngine::Throw("Invalid payload type");
		return;
	}

	if (!IsValid(InObject))
	{
		FAngelscriptEngine::Throw("Invalid object passed to BindUFunction.");
		return;
	}

	UFunction* Function = InObject->FindFunction(InFunctionName);
	if (Function == nullptr)
	{
		const FString Debug = FString::Printf(TEXT("\nCould not find function %s\nIs it declared UFUNCTION()?"), *InFunctionName.ToString());
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Debug));
		return;
	}

	bool bSignatureMismatch = false;
	FStructProperty* StructProp = CastField<FStructProperty>(Function->PropertyLink);
	if (StructProp == nullptr && Function->PropertyLink == nullptr)
	{
		bSignatureMismatch = true;
	}
	else if (StructProp == nullptr)
	{
		switch (PayloadScriptTypeId & asTYPEID_MASK_SEQNBR)
		{
		case asTYPEID_BOOL:
			bSignatureMismatch = !Function->PropertyLink->IsA<FBoolProperty>();
			break;
		case asTYPEID_INT8:
			bSignatureMismatch = !Function->PropertyLink->IsA<FInt8Property>();
			break;
		case asTYPEID_UINT8:
			bSignatureMismatch = !Function->PropertyLink->IsA<FByteProperty>();
			break;
		case asTYPEID_INT16:
			bSignatureMismatch = !Function->PropertyLink->IsA<FInt16Property>();
			break;
		case asTYPEID_UINT16:
			bSignatureMismatch = !Function->PropertyLink->IsA<FUInt16Property>();
			break;
		case asTYPEID_INT32:
			bSignatureMismatch = !Function->PropertyLink->IsA<FIntProperty>();
			break;
		case asTYPEID_UINT32:
			bSignatureMismatch = !Function->PropertyLink->IsA<FUInt32Property>();
			break;
		case asTYPEID_INT64:
			bSignatureMismatch = !Function->PropertyLink->IsA<FInt64Property>();
			break;
		case asTYPEID_UINT64:
			bSignatureMismatch = !Function->PropertyLink->IsA<FUInt64Property>();
			break;
		case asTYPEID_FLOAT32:
			bSignatureMismatch = !Function->PropertyLink->IsA<FFloatProperty>();
			break;
		case asTYPEID_FLOAT64:
			bSignatureMismatch = !Function->PropertyLink->IsA<FDoubleProperty>();
			break;
		default:
		{
			auto& Manager = FAngelscriptEngine::Get();
			asCTypeInfo* ScriptType = (asCTypeInfo*)Manager.Engine->GetTypeInfoById(PayloadScriptTypeId);
			if (ScriptType != nullptr && (ScriptType->flags & asOBJ_ENUM) && ScriptType->size == 1)
			{
				FEnumProperty* EnumProp = CastField<FEnumProperty>(Function->PropertyLink);
				FByteProperty* ByteProp = CastField<FByteProperty>(Function->PropertyLink);
				if (EnumProp == nullptr && ByteProp == nullptr)
				{
					bSignatureMismatch = true;
				}
			}
			else
			{
				bSignatureMismatch = true;
			}
		}
		break;
		}
	}
	else if (StructProp->Struct != StructType)
	{
		bSignatureMismatch = true;
	}

	if (Function->NumParms != 1 || bSignatureMismatch || Function->GetReturnProperty() != nullptr)
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: void FAngelscriptDelegateWithPayload(? Payload)\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Function));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	Payload.InitializeAs(StructType, (const uint8*)PayloadPtr);
	Object = InObject;
	FunctionName = InFunctionName;
}

void FAngelscriptDelegateWithPayload::Reset()
{
	Payload.Reset();
	Object.Reset();
	FunctionName = NAME_None;
}

UScriptStruct* FAngelscriptDelegateWithPayload::GetBoxedPrimitiveStructFromTypeId(int TypeId)
{
	switch (TypeId & asTYPEID_MASK_SEQNBR)
	{
	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		return FAngelscriptBoxedByte::StaticStruct();
	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		return FAngelscriptBoxedShort::StaticStruct();
	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		return FAngelscriptBoxedInt32::StaticStruct();
	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		return FAngelscriptBoxedInt64::StaticStruct();
	case asTYPEID_FLOAT32:
		return FAngelscriptBoxedFloat::StaticStruct();
	case asTYPEID_FLOAT64:
		return FAngelscriptBoxedDouble::StaticStruct();
	}

	auto& Manager = FAngelscriptEngine::Get();
	asCTypeInfo* ScriptType = (asCTypeInfo*)Manager.Engine->GetTypeInfoById(TypeId);
	if (ScriptType != nullptr && (ScriptType->flags & asOBJ_ENUM) && ScriptType->size == 1)
	{
		return FAngelscriptBoxedByte::StaticStruct();
	}

	return nullptr;
}
