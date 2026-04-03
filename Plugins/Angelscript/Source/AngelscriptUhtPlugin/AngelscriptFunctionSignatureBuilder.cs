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

			parameterTypes.Add(BuildParameterType(property));
		}

		string returnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildReturnType(returnProperty)
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

	private static string BuildParameterType(UhtProperty property)
	{
		StringBuilder builder = new();
		string propertyFlags = property.PropertyFlags.ToString();

		if (propertyFlags.Contains("ConstParm", StringComparison.Ordinal))
		{
			builder.Append("const ");
		}

		property.AppendText(builder, UhtPropertyTextType.ClassFunctionArgOrRetVal);

		if (propertyFlags.Contains("ReferenceParm", StringComparison.Ordinal) ||
			propertyFlags.Contains("OutParm", StringComparison.Ordinal))
		{
			builder.Append('&');
		}

		return builder.ToString().Trim();
	}

	private static string BuildReturnType(UhtProperty property)
	{
		StringBuilder builder = new();
		string propertyFlags = property.PropertyFlags.ToString();

		if (propertyFlags.Contains("ConstParm", StringComparison.Ordinal))
		{
			builder.Append("const ");
		}

		property.AppendText(builder, UhtPropertyTextType.ClassFunctionArgOrRetVal);

		if (propertyFlags.Contains("ReferenceParm", StringComparison.Ordinal))
		{
			builder.Append('&');
		}

		return builder.ToString().Trim();
	}

	private static bool HasFunctionFlag(UhtFunction function, string flagName)
	{
		return function.FunctionFlags.ToString().Contains(flagName, StringComparison.Ordinal);
	}
}
