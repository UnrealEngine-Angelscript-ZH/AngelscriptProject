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

		foreach (UhtModule module in factory.Session.Modules)
		{
			packageCount++;
			CountBlueprintCallableFunctions(module.ScriptPackage, ref classCount, ref functionCount);
		}

		Console.WriteLine(
			"AngelscriptUhtPlugin exporter visited {0} packages, {1} classes, {2} BlueprintCallable/Pure functions.",
			packageCount,
			classCount,
			functionCount);
	}

	private static void CountBlueprintCallableFunctions(UhtType type, ref int classCount, ref int functionCount)
	{
		if (type is UhtClass classObj)
		{
			classCount++;
			foreach (UhtType child in classObj.Children)
			{
				if (child is UhtFunction function && IsBlueprintCallable(function))
				{
					functionCount++;
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CountBlueprintCallableFunctions(child, ref classCount, ref functionCount);
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
