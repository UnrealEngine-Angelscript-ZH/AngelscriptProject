using System;
using System.Collections.Generic;
using System.Text;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUhtPlugin;

internal sealed record AngelscriptGeneratedFunctionEntry(
	string ClassName,
	string FunctionName,
	string EraseMacro)
{
	public string BuildRegistrationLine()
	{
		return $"\tFAngelscriptBinds::AddFunctionEntry({ClassName}::StaticClass(), \"{FunctionName}\", {{ {EraseMacro} }});";
	}
}

internal static class AngelscriptFunctionTableCodeGenerator
{
	public static int Generate(IUhtExportFactory factory)
	{
		int generatedFileCount = 0;

		foreach (UhtModule module in factory.Session.Modules)
		{
			if (TryGenerateModule(factory, module))
			{
				generatedFileCount++;
			}
		}

		return generatedFileCount;
	}

	private static bool TryGenerateModule(IUhtExportFactory factory, UhtModule module)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		List<AngelscriptGeneratedFunctionEntry> entries = new();

		CollectEntries(factory, module.ScriptPackage, includes, entries);
		if (entries.Count == 0)
		{
			return false;
		}

		entries.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			return classComparison != 0
				? classComparison
				: StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
		});

		StringBuilder builder = new();
		builder.AppendLine("#include \"CoreMinimal.h\"");
		builder.AppendLine("#include \"Core/AngelscriptBinds.h\"");
		builder.AppendLine("#include \"Core/AngelscriptEngine.h\"");
		builder.AppendLine("#include \"Core/FunctionCallers.h\"");

		foreach (string include in includes)
		{
			builder.Append("#include \"").Append(include).AppendLine("\"");
		}

		builder.AppendLine();
		builder.Append("AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionTable_").Append(module.ShortName)
			.AppendLine("((int32)FAngelscriptBinds::EOrder::Late + 50, []()");
		builder.AppendLine("{");

		foreach (AngelscriptGeneratedFunctionEntry entry in entries)
		{
			builder.AppendLine(entry.BuildRegistrationLine());
		}

		builder.Append("\tUE_LOG(Angelscript, Log, TEXT(\"[UHT] Registered %d generated BlueprintCallable entries for module %s\"), ")
			.Append(entries.Count)
			.Append(", TEXT(\"")
			.Append(module.ShortName)
			.AppendLine("\"));");

		builder.AppendLine("});");

		string outputPath = factory.MakePath($"AS_FunctionTable_{module.ShortName}", ".cpp");
		factory.CommitOutput(outputPath, builder);
		return true;
	}

	private static void CollectEntries(IUhtExportFactory factory, UhtType type, SortedSet<string> includes, List<AngelscriptGeneratedFunctionEntry> entries)
	{
		if (type is UhtClass classObj)
		{
			foreach (UhtType child in classObj.Children)
			{
				if (child is UhtFunction function && ShouldGenerate(classObj, function))
				{
					if (classObj.HeaderFile != null)
					{
						factory.AddExternalDependency(classObj.HeaderFile.FilePath);
						string includePath = factory.GetModuleShortestIncludePath(classObj.HeaderFile.Module, classObj.HeaderFile.FilePath);
						string normalizedIncludePath = includePath.Replace('\\', '/');
						includes.Add(normalizedIncludePath);
					}

					string eraseMacro;
					if (classObj.ClassType == UhtClassType.Interface || classObj.ClassType == UhtClassType.NativeInterface)
					{
						eraseMacro = "ERASE_NO_FUNCTION()";
					}
					else if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? _))
					{
						eraseMacro = signature!.BuildEraseMacro();
					}
					else
					{
						eraseMacro = "ERASE_NO_FUNCTION()";
					}

					entries.Add(new AngelscriptGeneratedFunctionEntry(classObj.SourceName, function.SourceName, eraseMacro));
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CollectEntries(factory, child, includes, entries);
		}
	}

	private static bool ShouldGenerate(UhtClass classObj, UhtFunction function)
	{
		if (!AngelscriptFunctionTableExporter.IsBlueprintCallable(function))
		{
			return false;
		}

		if (function.MetaData.ContainsKey("NotInAngelscript") || function.MetaData.ContainsKey("BlueprintInternalUseOnly"))
		{
			return false;
		}

		return !function.FunctionExportFlags.ToString().Contains("CustomThunk", StringComparison.Ordinal);
	}
}
