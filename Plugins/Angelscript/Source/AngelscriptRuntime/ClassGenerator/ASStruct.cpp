#include "ClassGenerator/ASStruct.h"

#include "Misc/SecureHash.h"

#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "source/as_objecttype.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

struct FASStructOps : UASStruct::ICppStructOps
{
	UASStruct* Struct;
	asCObjectType* ScriptType;

	asIScriptFunction* EqualsFunction;
	asIScriptFunction* ConstructFunction;
	asIScriptFunction* ToStrFunction;
	asIScriptFunction* HashFunction;

	struct FASFakeVTable : public UE::CoreUObject::Private::FStructOpsFakeVTable
	{
		void* Construct;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		void* ConstructForTests;
#endif
		void* Destruct;
		void* Copy;
		void* Identical;
		void* GetStructTypeHash;
	};

	FASFakeVTable FakeVTable;

	FASStructOps(UASStruct* InStruct, int32 InSize, int32 InAlignment)
		: UASStruct::ICppStructOps(InSize, InAlignment)
		, Struct(InStruct)
		, ScriptType((asCObjectType*)InStruct->ScriptType)
	{
		SetFromStruct(InStruct);
		FakeVPtr = &FakeVTable;

		FakeVTable.Flags =
			UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Construct
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::ConstructForTests
#endif
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Destruct
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Copy
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Identical
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::GetStructTypeHash;

		FakeVTable.Construct = reinterpret_cast<void*>(&FASStructOps::Construct);
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		FakeVTable.ConstructForTests = reinterpret_cast<void*>(&FASStructOps::Construct);
#endif
		FakeVTable.Destruct = reinterpret_cast<void*>(&FASStructOps::Destruct);
		FakeVTable.Copy = reinterpret_cast<void*>(&FASStructOps::Copy);
		FakeVTable.Identical = reinterpret_cast<void*>(&FASStructOps::Identical);
		FakeVTable.GetStructTypeHash = reinterpret_cast<void*>(&FASStructOps::GetStructTypeHash);

		FMemory::Memzero(FakeVTable.Capabilities);
		FakeVTable.Capabilities.HasDestructor = true;
		FakeVTable.Capabilities.HasCopy = true;
		FakeVTable.Capabilities.HasIdentical = (EqualsFunction != nullptr);
		FakeVTable.Capabilities.HasGetTypeHash = (HashFunction != nullptr);
		FakeVTable.Capabilities.ComputedPropertyFlags |= (HashFunction != nullptr) ? CPF_HasGetValueTypeHash : CPF_None;
	}

	void SetFromStruct(UASStruct* InStruct)
	{
		check(InStruct == Struct);
		ScriptType = (asCObjectType*)InStruct->ScriptType;

		if (ScriptType != nullptr)
		{
			auto& Manager = FAngelscriptEngine::Get();
			if (ScriptType->beh.construct != 0)
				ConstructFunction = Manager.Engine->GetFunctionById(ScriptType->beh.construct);
			else
				ConstructFunction = nullptr;

			if (ScriptType->GetFirstMethod("opEquals") != nullptr)
			{
				FString StructName = ANSI_TO_TCHAR(ScriptType->GetName());
				FString EqualsDecl = FString::Printf(TEXT("bool opEquals(const %s& Other) const"), *StructName);
				EqualsFunction = ScriptType->GetMethodByDecl(TCHAR_TO_ANSI(*EqualsDecl));
			}
			else
			{
				EqualsFunction = nullptr;
			}

			if (ScriptType->GetFirstMethod("ToString") != nullptr)
			{
				ToStrFunction = ScriptType->GetMethodByDecl("FString ToString() const");
			}
			else
			{
				ToStrFunction = nullptr;
			}

			if (ScriptType->GetFirstMethod("Hash") != nullptr)
			{
				const FString HashDecl = TEXT("uint32 Hash() const");
				HashFunction = ScriptType->GetMethodByDecl(TCHAR_TO_ANSI(*HashDecl));
			}
			else
			{
				HashFunction = nullptr;
			}
		}
		else
		{
			EqualsFunction = nullptr;
			ConstructFunction = nullptr;
			ToStrFunction = nullptr;
			HashFunction = nullptr;
		}
	}

	static void Construct(FASStructOps* Ops, void* Dest)
	{
		if (Ops->ScriptType == nullptr)
		{
			FMemory::Memzero(Dest, Ops->GetSize());
			return;
		}

		if (Ops->ConstructFunction != nullptr)
		{
			FAngelscriptContext Context;
			Context->Prepare(Ops->ConstructFunction);
			Context->SetObject((asIScriptObject*)Dest);
			Context->Execute();
		}
		else
		{
			FMemory::Memzero(Dest, Ops->GetSize());
		}
	}

	static void Destruct(FASStructOps* Ops, void* Dest)
	{
		if (Ops->ScriptType == nullptr)
		{
			FMemory::Memzero(Dest, Ops->GetSize());
			return;
		}

		auto* ScriptObject = (asCScriptObject*)(Dest);
		ScriptObject->CallDestructor(Ops->ScriptType);
	}

	static bool Copy(FASStructOps* Ops, void* Dest, void const* Src, int32 ArrayDim)
	{
		if (Ops->ScriptType == nullptr)
			return true;

		auto* DestObject = (asCScriptObject*)(Dest);
		auto* SourceObject = (asCScriptObject*)(Src);
		DestObject->PerformCopy(SourceObject, Ops->ScriptType, Ops->ScriptType);
		return true;
	}

	static bool Identical(FASStructOps* Ops, const void* A, const void* B, uint32 PortFlags, bool& bOutResult)
	{
		if (Ops->ScriptType == nullptr)
			return false;
		if (Ops->EqualsFunction == nullptr)
			return false;

		FAngelscriptContext Context;
		Context->Prepare(Ops->EqualsFunction);
		Context->SetObject((asIScriptObject*)A);
		Context->SetArgAddress(0, (void*)B);
		Context->Execute();
		bOutResult = (Context->GetReturnByte() != 0);
		return true;
	}

	static uint32 GetStructTypeHash(FASStructOps* Ops, const void* Src)
	{
		if (Ops->HashFunction == nullptr)
			return 0;

		FAngelscriptContext Context;
		Context->Prepare(Ops->HashFunction);
		Context->SetObject(const_cast<void*>(Src));
		Context->Execute();
		return Context->GetReturnDWord();
	}
};

UASStruct::UASStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UASStruct::ICppStructOps* UASStruct::CreateCppStructOps()
{
	return new FASStructOps(this, GetPropertiesSize(), GetMinAlignment());
}

void UASStruct::PrepareCppStructOps()
{
	if (CppStructOps == nullptr)
		SetCppStructOps(CreateCppStructOps());
	Super::PrepareCppStructOps();
}

void UASStruct::UpdateScriptType()
{
	FASStructOps* Ops = ((FASStructOps*)GetCppStructOps());
	Ops->SetFromStruct(this);

	if (Ops->EqualsFunction != nullptr)
		StructFlags = EStructFlags(StructFlags | STRUCT_IdenticalNative);
	else
		StructFlags = EStructFlags(StructFlags & ~STRUCT_IdenticalNative);
}

asIScriptFunction* UASStruct::GetToStringFunction() const
{
	if (ICppStructOps* StructOps = GetCppStructOps())
	{
		return ((FASStructOps*)StructOps)->ToStrFunction;
	}

	return nullptr;
}

void UASStruct::SetGuid(FName FromName)
{
	FString HashString = TEXT("Script:");
	HashString += FromName.ToString();

	ensure(HashString.Len());

	const uint32 BufferLength = HashString.Len() * sizeof(HashString[0]);
	uint32 HashBuffer[5];
	FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast<uint8*>(HashBuffer));
	Guid = FGuid(HashBuffer[1], HashBuffer[2], HashBuffer[3], HashBuffer[4]);
}
