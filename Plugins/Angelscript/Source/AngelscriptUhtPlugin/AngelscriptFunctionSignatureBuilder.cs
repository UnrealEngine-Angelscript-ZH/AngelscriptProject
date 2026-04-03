using System;
using System.Collections.Generic;
using System.Text;
using EpicGames.UHT.Types;

namespace AngelscriptUhtPlugin;

internal sealed record AngelscriptFunctionSignature(
	string OwningType,
	string FunctionName,
	string ReturnType,
	IReadOnlyList<string> ParameterTypes,
	bool IsStatic,
	bool IsConst)
{
	public string BuildEraseMacro()
	{
		string parameterPack = ParameterTypes.Count == 0
			? "()"
			: $"({string.Join(", ", ParameterTypes)})";

		if (IsConst && !IsStatic)
		{
			parameterPack += " const";
		}

		return IsStatic
			? $"ERASE_FUNCTION_PTR({OwningType}::{FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))"
			: $"ERASE_METHOD_PTR({OwningType}, {FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))";
	}
}

internal static class AngelscriptFunctionSignatureBuilder
{
	public static bool TryBuild(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? failureReason)
	{
		signature = null;

		if (function.FunctionType != UhtFunctionType.Function)
		{
			failureReason = "non-function";
			return false;
		}

		List<string> parameterTypes = new();
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is not UhtProperty property)
			{
				failureReason = "non-property-parameter";
				return false;
			}

			if (property.ArrayDimensions != null)
			{
				failureReason = "static-array-parameter";
				return false;
			}

			parameterTypes.Add(BuildPropertyType(property));
		}

		string returnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildPropertyType(returnProperty)
			: "void";

		signature = new AngelscriptFunctionSignature(
			classObj.SourceName,
			function.SourceName,
			returnType,
			parameterTypes,
			HasFunctionFlag(function, "Static"),
			HasFunctionFlag(function, "Const"));

		failureReason = null;
		return true;
	}

	private static string BuildPropertyType(UhtProperty property)
	{
		StringBuilder builder = new();
		property.AppendFullDecl(builder, UhtPropertyTextType.ClassFunctionArgOrRetVal, true);
		return builder.ToString().Trim();
	}

	private static bool HasFunctionFlag(UhtFunction function, string flagName)
	{
		return function.FunctionFlags.ToString().Contains(flagName, StringComparison.Ordinal);
	}
}
