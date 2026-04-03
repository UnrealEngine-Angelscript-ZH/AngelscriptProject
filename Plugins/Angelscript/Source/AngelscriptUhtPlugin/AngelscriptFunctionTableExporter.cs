using System;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUhtPlugin;

[UnrealHeaderTool]
internal static class AngelscriptFunctionTableExporter
{
	[UhtExporter(
		Name = "AngelscriptFunctionTable",
		Description = "Exports Angelscript function table data",
		Options = UhtExporterOptions.Default | UhtExporterOptions.CompileOutput,
		ModuleName = "AngelscriptRuntime")]
	private static void Export(IUhtExportFactory factory)
	{
		int packageCount = 0;
		int classCount = 0;
		int functionCount = 0;
		int reconstructedCount = 0;
		int skippedCount = 0;

		foreach (UhtModule module in factory.Session.Modules)
		{
			packageCount++;
			CountBlueprintCallableFunctions(module.ScriptPackage, ref classCount, ref functionCount, ref reconstructedCount, ref skippedCount);
		}

		Console.WriteLine(
			"AngelscriptUhtPlugin exporter visited {0} packages, {1} classes, {2} BlueprintCallable/Pure functions, reconstructed {3}, skipped {4}.",
			packageCount,
			classCount,
			functionCount,
			reconstructedCount,
			skippedCount);
	}

	private static void CountBlueprintCallableFunctions(UhtType type, ref int classCount, ref int functionCount, ref int reconstructedCount, ref int skippedCount)
	{
		if (type is UhtClass classObj)
		{
			classCount++;
			foreach (UhtType child in classObj.Children)
			{
				if (child is UhtFunction function && IsBlueprintCallable(function))
				{
					functionCount++;
					if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? _))
					{
						_ = signature!.BuildEraseMacro();
						reconstructedCount++;
					}
					else
					{
						skippedCount++;
					}
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CountBlueprintCallableFunctions(child, ref classCount, ref functionCount, ref reconstructedCount, ref skippedCount);
		}
	}

	private static bool IsBlueprintCallable(UhtFunction function)
	{
		string functionFlags = function.FunctionFlags.ToString();

		return function.FunctionType == UhtFunctionType.Function &&
			(functionFlags.Contains("BlueprintCallable", StringComparison.Ordinal) ||
			functionFlags.Contains("BlueprintPure", StringComparison.Ordinal));
	}
}
