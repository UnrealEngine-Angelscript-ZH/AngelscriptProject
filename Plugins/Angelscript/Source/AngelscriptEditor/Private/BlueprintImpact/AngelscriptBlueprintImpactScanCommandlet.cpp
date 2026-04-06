#include "BlueprintImpact/AngelscriptBlueprintImpactScanCommandlet.h"

#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

namespace
{
	enum class EBlueprintImpactCommandletExitCode : int32
	{
		Success = 0,
		InvalidArguments = 1,
		EngineNotReady = 2,
		AssetScanFailure = 3,
	};

	void AppendChangedScriptsFromDelimitedValue(const FString& Value, TArray<FString>& OutScripts)
	{
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT(",;"), true);
		for (const FString& Part : Parts)
		{
			const FString Trimmed = Part.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				OutScripts.Add(Trimmed);
			}
		}
	}

	bool TryReadChangedScriptsFile(const FString& FilePath, TArray<FString>& OutScripts)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
		{
			return false;
		}

		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				OutScripts.Add(Trimmed);
			}
		}

		return true;
	}
}

int32 UAngelscriptBlueprintImpactScanCommandlet::Main(const FString& Params)
{
	if (!FAngelscriptEngine::Get().bDidInitialCompileSucceed)
	{
		UE_LOG(Angelscript, Error, TEXT("Blueprint impact commandlet requires a successfully initialized Angelscript engine."));
		return static_cast<int32>(EBlueprintImpactCommandletExitCode::EngineNotReady);
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest Request;

	FString ChangedScriptsValue;
	if (FParse::Value(*Params, TEXT("ChangedScript="), ChangedScriptsValue))
	{
		AppendChangedScriptsFromDelimitedValue(ChangedScriptsValue, Request.ChangedScripts);
	}

	FString ChangedScriptsFile;
	if (FParse::Value(*Params, TEXT("ChangedScriptFile="), ChangedScriptsFile))
	{
		if (!TryReadChangedScriptsFile(ChangedScriptsFile, Request.ChangedScripts))
		{
			UE_LOG(Angelscript, Error, TEXT("Blueprint impact commandlet failed to read ChangedScriptFile: %s"), *ChangedScriptsFile);
			return static_cast<int32>(EBlueprintImpactCommandletExitCode::InvalidArguments);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult ScanResult = AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(
		FAngelscriptEngine::Get(),
		AssetRegistryModule.Get(),
		Request);

	UE_LOG(
		Angelscript,
		Display,
		TEXT("{ \"BlueprintImpact\": { \"FullScan\": %s, \"ChangedScripts\": %d, \"MatchingModules\": %d, \"Classes\": %d, \"Structs\": %d, \"Enums\": %d, \"Delegates\": %d, \"CandidateAssets\": %d, \"Matches\": %d, \"FailedAssetLoads\": %d } }"),
		Request.IsFullScan() ? TEXT("true") : TEXT("false"),
		ScanResult.NormalizedChangedScripts.Num(),
		ScanResult.MatchingModules.Num(),
		ScanResult.Symbols.Classes.Num(),
		ScanResult.Symbols.Structs.Num(),
		ScanResult.Symbols.Enums.Num(),
		ScanResult.Symbols.Delegates.Num(),
		ScanResult.CandidateAssets.Num(),
		ScanResult.Matches.Num(),
		ScanResult.FailedAssetLoads);

	for (const AngelscriptEditor::BlueprintImpact::FBlueprintImpactMatch& Match : ScanResult.Matches)
	{
		TArray<FString> ReasonStrings;
		for (const AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason Reason : Match.Reasons)
		{
			ReasonStrings.Add(AngelscriptEditor::BlueprintImpact::LexToString(Reason));
		}

		UE_LOG(
			Angelscript,
			Display,
			TEXT("[BlueprintImpact] %s | Reasons=%s"),
			*Match.AssetData.GetObjectPathString(),
			*FString::Join(ReasonStrings, TEXT(",")));
	}

	return ScanResult.FailedAssetLoads > 0
		? static_cast<int32>(EBlueprintImpactCommandletExitCode::AssetScanFailure)
		: static_cast<int32>(EBlueprintImpactCommandletExitCode::Success);
}
