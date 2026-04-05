using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

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

internal sealed record AngelscriptSupportedModules(
	HashSet<string> All,
	HashSet<string> EditorOnly);

internal sealed record AngelscriptModuleGenerationSummary(
	string ModuleName,
	bool EditorOnly,
	int TotalEntries,
	int DirectBindEntries,
	int StubEntries,
	int ShardCount);

internal static class AngelscriptFunctionTableCodeGenerator
{
	private static readonly Regex QuotedStringPattern = new("\"([^\"]+)\"", RegexOptions.Compiled);
	private const int MaxEntriesPerShard = 256;

	public static int Generate(IUhtExportFactory factory)
	{
		AngelscriptSupportedModules supportedModules = LoadSupportedModules(factory);
		int generatedFileCount = 0;
		HashSet<string> generatedPaths = new(StringComparer.OrdinalIgnoreCase);
		List<AngelscriptModuleGenerationSummary> moduleSummaries = new();

		foreach (UhtModule module in factory.Session.Modules)
		{
			if (!supportedModules.All.Contains(module.ShortName))
			{
				continue;
			}

			AngelscriptModuleGenerationSummary? moduleSummary = GenerateModule(factory, module, supportedModules.EditorOnly.Contains(module.ShortName), generatedPaths);
			if (moduleSummary != null)
			{
				generatedFileCount += moduleSummary.ShardCount;
				moduleSummaries.Add(moduleSummary);
			}
		}

		DeleteStaleOutputs(factory, generatedPaths);
		WriteCoverageDiagnostics(moduleSummaries);

		return generatedFileCount;
	}

	private static AngelscriptModuleGenerationSummary? GenerateModule(IUhtExportFactory factory, UhtModule module, bool editorOnly, HashSet<string> generatedPaths)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		List<AngelscriptGeneratedFunctionEntry> entries = new();

		CollectEntries(factory, module.ScriptPackage, includes, entries);
		if (entries.Count == 0)
		{
			return null;
		}

		entries.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			return classComparison != 0
				? classComparison
				: StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
		});

		int generatedShardCount = 0;
		int directBindEntries = 0;
		int stubEntries = 0;
		foreach (AngelscriptGeneratedFunctionEntry entry in entries)
		{
			if (entry.EraseMacro == "ERASE_NO_FUNCTION()")
			{
				stubEntries++;
			}
			else
			{
				directBindEntries++;
			}
		}

		int shardCount = (entries.Count + MaxEntriesPerShard - 1) / MaxEntriesPerShard;
		for (int shardIndex = 0; shardIndex < shardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, entries.Count - startIndex);
			string outputPath = factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".cpp");
			factory.CommitOutput(outputPath, BuildShard(module.ShortName, editorOnly, includes, entries, startIndex, entryCount, shardIndex, shardCount));
			generatedPaths.Add(outputPath);
			generatedShardCount++;
		}

		return new AngelscriptModuleGenerationSummary(module.ShortName, editorOnly, entries.Count, directBindEntries, stubEntries, generatedShardCount);
	}

	private static void WriteCoverageDiagnostics(List<AngelscriptModuleGenerationSummary> moduleSummaries)
	{
		moduleSummaries.Sort(static (left, right) =>
		{
			int stubComparison = right.StubEntries.CompareTo(left.StubEntries);
			return stubComparison != 0
				? stubComparison
				: StringComparer.Ordinal.Compare(left.ModuleName, right.ModuleName);
		});

		Console.WriteLine("AngelscriptUHTTool per-module coverage diagnostics:");
		foreach (AngelscriptModuleGenerationSummary summary in moduleSummaries)
		{
			Console.WriteLine(
				"  - {0}{1}: total={2}, direct={3}, stubs={4}, shards={5}",
				summary.ModuleName,
				summary.EditorOnly ? " [EditorOnly]" : string.Empty,
				summary.TotalEntries,
				summary.DirectBindEntries,
				summary.StubEntries,
				summary.ShardCount);
		}
	}

	private static StringBuilder BuildShard(string moduleShortName, bool editorOnly, SortedSet<string> includes, List<AngelscriptGeneratedFunctionEntry> entries, int startIndex, int entryCount, int shardIndex, int shardCount)
	{
		StringBuilder builder = new();
		if (editorOnly)
		{
			builder.AppendLine("#if WITH_EDITOR");
		}

		builder.AppendLine("PRAGMA_DISABLE_DEPRECATION_WARNINGS");
		builder.AppendLine("#include \"CoreMinimal.h\"");
		builder.AppendLine("#include \"Core/AngelscriptBinds.h\"");
		builder.AppendLine("#include \"Core/AngelscriptEngine.h\"");
		builder.AppendLine("#include \"Core/FunctionCallers.h\"");

		foreach (string include in includes)
		{
			builder.Append("#include \"").Append(include).AppendLine("\"");
		}

		builder.AppendLine();
		builder.Append("AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionTable_")
			.Append(moduleShortName)
			.Append('_')
			.Append(shardIndex.ToString("D3"))
			.AppendLine("((int32)FAngelscriptBinds::EOrder::Late + 50, []()");
		builder.AppendLine("{");

		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			builder.AppendLine(entries[entryIndex].BuildRegistrationLine());
		}

		builder.Append("\tUE_LOG(Angelscript, Log, TEXT(\"[UHT] Registered %d generated BlueprintCallable entries for module %s shard %d/%d\"), ")
			.Append(entryCount)
			.Append(", TEXT(\"")
			.Append(moduleShortName)
			.Append("\"), ")
			.Append(shardIndex + 1)
			.Append(", ")
			.Append(shardCount)
			.AppendLine(");");

		builder.AppendLine("});");
		builder.AppendLine("PRAGMA_ENABLE_DEPRECATION_WARNINGS");
		if (editorOnly)
		{
			builder.AppendLine("#endif");
		}

		return builder;
	}

	private static AngelscriptSupportedModules LoadSupportedModules(IUhtExportFactory factory)
	{
		string buildCsPath = ResolveRuntimeBuildCsPath(factory);
		factory.AddExternalDependency(buildCsPath);

		HashSet<string> allModules = new(StringComparer.OrdinalIgnoreCase)
		{
			"AngelscriptRuntime",
		};
		HashSet<string> editorOnlyModules = new(StringComparer.OrdinalIgnoreCase);

		bool inDependencyBlock = false;
		bool inEditorBlock = false;
		foreach (string rawLine in File.ReadAllLines(buildCsPath))
		{
			string line = rawLine.Trim();
			if (line.StartsWith("if (Target.bBuildEditor)", StringComparison.Ordinal))
			{
				inEditorBlock = true;
			}

			if (line.Contains("DependencyModuleNames.AddRange", StringComparison.Ordinal))
			{
				inDependencyBlock = true;
			}

			if (inDependencyBlock)
			{
				foreach (Match match in QuotedStringPattern.Matches(line))
				{
					string moduleName = match.Groups[1].Value;
					allModules.Add(moduleName);
					if (inEditorBlock)
					{
						editorOnlyModules.Add(moduleName);
					}
				}

				if (line.Contains("});", StringComparison.Ordinal))
				{
					inDependencyBlock = false;
				}
			}

			if (inEditorBlock && line == "}")
			{
				inEditorBlock = false;
			}
		}

		return new AngelscriptSupportedModules(allModules, editorOnlyModules);
	}

	private static string ResolveRuntimeBuildCsPath(IUhtExportFactory factory)
	{
		foreach (UhtModule module in factory.Session.Modules)
		{
			if (!module.ShortName.Equals("AngelscriptRuntime", StringComparison.Ordinal))
			{
				continue;
			}

			if (TryFindFirstHeaderPath(module.ScriptPackage, out string? headerPath) && !string.IsNullOrEmpty(headerPath))
			{
				string normalizedHeaderPath = headerPath.Replace('\\', '/');
				string marker = "/Source/AngelscriptRuntime/";
				int markerIndex = normalizedHeaderPath.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
				if (markerIndex >= 0)
				{
					string moduleRoot = normalizedHeaderPath.Substring(0, markerIndex + marker.Length - 1);
					return Path.Combine(moduleRoot, "AngelscriptRuntime.Build.cs");
				}
			}
		}

		throw new InvalidOperationException("Unable to locate AngelscriptRuntime.Build.cs from UHT session modules.");
	}

	private static bool TryFindFirstHeaderPath(UhtType type, out string? headerPath)
	{
		if (type is UhtClass classObj && classObj.HeaderFile != null)
		{
			headerPath = classObj.HeaderFile.FilePath;
			return true;
		}

		foreach (UhtType child in type.Children)
		{
			if (TryFindFirstHeaderPath(child, out headerPath))
			{
				return true;
			}
		}

		headerPath = null;
		return false;
	}

	private static void DeleteStaleOutputs(IUhtExportFactory factory, HashSet<string> generatedPaths)
	{
		string outputDirectory = Path.GetDirectoryName(factory.MakePath("AS_FunctionTable_Stale", ".cpp"))!;
		if (!Directory.Exists(outputDirectory))
		{
			return;
		}

		foreach (string existingFile in Directory.EnumerateFiles(outputDirectory, "AS_FunctionTable_*.cpp"))
		{
			if (!generatedPaths.Contains(existingFile))
			{
				File.Delete(existingFile);
			}
		}
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
		if (classObj.HeaderFile == null || !IsSupportedHeader(classObj.HeaderFile.FilePath))
		{
			return false;
		}

		if (!AngelscriptFunctionTableExporter.IsBlueprintCallable(function))
		{
			return false;
		}

		if (function.MetaData.ContainsKey("NotInAngelscript") ||
			(function.MetaData.ContainsKey("BlueprintInternalUseOnly") && !function.MetaData.ContainsKey("UsableInAngelscript")))
		{
			return false;
		}

		if (classObj.SourceName == "UUniversalObjectLocatorScriptingExtensions" &&
			(function.SourceName == "MakeUniversalObjectLocator" || function.SourceName == "UniversalObjectLocatorFromString"))
		{
			return false;
		}

		return !function.FunctionExportFlags.ToString().Contains("CustomThunk", StringComparison.Ordinal);
	}

	private static bool IsSupportedHeader(string headerPath)
	{
		string normalizedHeaderPath = headerPath.Replace('\\', '/');
		if (normalizedHeaderPath.Contains("/Private/", StringComparison.OrdinalIgnoreCase))
		{
			return false;
		}

		if (normalizedHeaderPath.EndsWith("/Components/InstancedSkinnedMeshComponent.h", StringComparison.OrdinalIgnoreCase))
		{
			return false;
		}

		return true;
	}
}
