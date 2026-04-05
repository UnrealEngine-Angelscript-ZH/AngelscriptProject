#include "ClassReloadHelper.h"

#include "Dump/AngelscriptCSVWriter.h"
#include "Dump/AngelscriptStateDump.h"

#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptEditorStateDump, Log, All);

namespace
{
	FString GetObjectName(const UObject* Object)
	{
		return Object != nullptr ? Object->GetName() : FString();
	}

	void SaveEditorReloadState(const FString& OutputDir)
	{
		FCSVWriter Writer;
		Writer.AddHeader({
			TEXT("Category"),
			TEXT("OldName"),
			TEXT("NewName")
		});

		FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
		for (const TPair<UClass*, UClass*>& ReloadClass : ReloadState.ReloadClasses)
		{
			Writer.AddRow({ TEXT("ReloadClass"), GetObjectName(ReloadClass.Key), GetObjectName(ReloadClass.Value) });
		}

		for (UClass* NewClass : ReloadState.NewClasses)
		{
			Writer.AddRow({ TEXT("NewClass"), FString(), GetObjectName(NewClass) });
		}

		for (UEnum* ReloadEnum : ReloadState.ReloadEnums)
		{
			const FString EnumName = GetObjectName(ReloadEnum);
			Writer.AddRow({ TEXT("ReloadEnum"), EnumName, EnumName });
		}

		for (UEnum* NewEnum : ReloadState.NewEnums)
		{
			Writer.AddRow({ TEXT("NewEnum"), FString(), GetObjectName(NewEnum) });
		}

		for (const TPair<UScriptStruct*, UScriptStruct*>& ReloadStruct : ReloadState.ReloadStructs)
		{
			Writer.AddRow({ TEXT("ReloadStruct"), GetObjectName(ReloadStruct.Key), GetObjectName(ReloadStruct.Value) });
		}

		for (const TPair<UDelegateFunction*, UDelegateFunction*>& ReloadDelegate : ReloadState.ReloadDelegates)
		{
			Writer.AddRow({ TEXT("ReloadDelegate"), GetObjectName(ReloadDelegate.Key), GetObjectName(ReloadDelegate.Value) });
		}

		const FString Filename = FPaths::Combine(OutputDir, TEXT("EditorReloadState.csv"));
		FString ErrorMessage;
		if (!Writer.SaveToFile(Filename, &ErrorMessage))
		{
			UE_LOG(LogAngelscriptEditorStateDump, Error, TEXT("Failed to save editor reload state dump: %s"), *ErrorMessage);
		}
	}

	FString GetExtensionLocationString(const EScriptEditorMenuExtensionLocation Location)
	{
		const UEnum* LocationEnum = StaticEnum<EScriptEditorMenuExtensionLocation>();
		return LocationEnum != nullptr
			? LocationEnum->GetNameStringByValue(static_cast<int64>(Location))
			: FString();
	}

	void SaveEditorMenuExtensions(const FString& OutputDir)
	{
		FCSVWriter Writer;
		Writer.AddHeader({
			TEXT("ExtensionPoint"),
			TEXT("Location"),
			TEXT("SectionName")
		});

		const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot> Snapshots = UScriptEditorMenuExtension::GetRegisteredExtensionSnapshots();
		for (const UScriptEditorMenuExtension::FRegisteredExtensionSnapshot& Snapshot : Snapshots)
		{
			Writer.AddRow({
				Snapshot.ExtensionPoint.ToString(),
				GetExtensionLocationString(Snapshot.Location),
				Snapshot.SectionName.ToString()
			});
		}

		const FString Filename = FPaths::Combine(OutputDir, TEXT("EditorMenuExtensions.csv"));
		FString ErrorMessage;
		if (!Writer.SaveToFile(Filename, &ErrorMessage))
		{
			UE_LOG(LogAngelscriptEditorStateDump, Error, TEXT("Failed to save editor menu extensions dump: %s"), *ErrorMessage);
		}
	}

	void DumpEditorState(const FString& OutputDir)
	{
		SaveEditorReloadState(OutputDir);
		SaveEditorMenuExtensions(OutputDir);
	}
}

namespace AngelscriptEditor::Private
{
	void RegisterStateDumpExtension(FDelegateHandle& OutHandle)
	{
		if (!OutHandle.IsValid())
		{
			OutHandle = FAngelscriptStateDump::OnDumpExtensions.AddStatic(&DumpEditorState);
		}
	}

	void UnregisterStateDumpExtension(FDelegateHandle& InOutHandle)
	{
		if (InOutHandle.IsValid())
		{
			FAngelscriptStateDump::OnDumpExtensions.Remove(InOutHandle);
			InOutHandle.Reset();
		}
	}
}
