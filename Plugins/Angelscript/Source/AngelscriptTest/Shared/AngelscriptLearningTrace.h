#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

namespace AngelscriptTestSupport
{
	enum class EAngelscriptLearningTracePhase : uint8
	{
		EngineBootstrap,
		Binding,
		Compile,
		Bytecode,
		ClassGeneration,
		Execution,
		UEBridge,
		Debug,
		GC,
		Editor,
	};

	enum class EAngelscriptLearningTraceDetailLevel : uint8
	{
		Summary,
		Verbose,
	};

	struct FAngelscriptLearningTraceKeyValue
	{
		FString Key;
		FString Value;
	};

	struct FAngelscriptLearningTraceEvent
	{
		int32 SequenceIndex = 0;
		EAngelscriptLearningTracePhase Phase = EAngelscriptLearningTracePhase::EngineBootstrap;
		FString StepId;
		FString Action;
		FString Observation;
		TArray<FAngelscriptLearningTraceKeyValue> Evidence;
		TArray<FString> CodeBlocks;
	};

	struct FAngelscriptLearningTraceDiagnostic
	{
		FString Section;
		int32 Row = 0;
		int32 Column = 0;
		FString Severity;
		FString Message;
	};

	struct FAngelscriptLearningTraceSinkConfig
	{
		bool bEmitToAutomation = true;
		bool bEmitToLog = true;
		bool bEmitToFile = false;
		EAngelscriptLearningTraceDetailLevel DetailLevel = EAngelscriptLearningTraceDetailLevel::Summary;
		FString OutputRoot;
	};

	class FAngelscriptLearningTraceSession
	{
	public:
		explicit FAngelscriptLearningTraceSession(
			const FString& InSessionName,
			const FAngelscriptLearningTraceSinkConfig& InSinkConfig = FAngelscriptLearningTraceSinkConfig());

		void BeginPhase(EAngelscriptLearningTracePhase InPhase);
		void AddStep(const FString& Action, const FString& Observation);
		void AddKeyValue(const FString& Key, const FString& Value);
		void AddCodeBlock(const FString& CodeBlock);

		const TArray<FAngelscriptLearningTraceEvent>& GetEvents() const;
		TArray<FString> BuildSummaryLines() const;
		void FlushToAutomation(FAutomationTestBase& Test) const;
		void FlushToLog() const;
		bool FlushToFile() const;

	private:
		const FAngelscriptLearningTraceEvent* GetLastEvent() const;
		FAngelscriptLearningTraceEvent* GetLastEvent();
		FString BuildEventLine(const FAngelscriptLearningTraceEvent& Event) const;
		static FString GetPhaseLabel(EAngelscriptLearningTracePhase Phase);
		FString GetOutputRoot() const;

		FString SessionName;
		FAngelscriptLearningTraceSinkConfig SinkConfig;
		TArray<FAngelscriptLearningTraceEvent> Events;
		EAngelscriptLearningTracePhase CurrentPhase = EAngelscriptLearningTracePhase::EngineBootstrap;
		int32 CurrentPhaseStepIndex = 0;
		int32 NextSequenceIndex = 1;
	};

	FString FormatLearningTraceKeyValueList(const TArray<FAngelscriptLearningTraceKeyValue>& Entries, const FString& Title);
	FString FormatLearningTraceDiagnostics(const TArray<FAngelscriptLearningTraceDiagnostic>& Diagnostics);
	FString FormatLearningTraceStringList(const TArray<FString>& Lines, const FString& Title);
	FString FormatLearningTraceBytecode(const TArray<uint32>& Dwords, int32 PreviewCount);
	FString FormatLearningTraceClassSummary(
		const FString& ClassName,
		const FString& SuperClassName,
		bool bIsActorDerived,
		const TArray<FString>& FunctionSummaries,
		const TArray<FString>& PropertySummaries);
}
