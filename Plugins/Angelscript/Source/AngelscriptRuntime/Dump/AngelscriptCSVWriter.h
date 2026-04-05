#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

struct FCSVWriter
{
	void AddHeader(TArray<FString> InHeader)
	{
		Header = MoveTemp(InHeader);
	}

	void AddRow(TArray<FString> InRow)
	{
		Rows.Add(MoveTemp(InRow));
	}

	int32 GetRowCount() const
	{
		return Rows.Num();
	}

	bool SaveToFile(const FString& Filename, FString* OutError = nullptr) const
	{
		if (Header.IsEmpty())
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("CSV header is empty.");
			}
			return false;
		}

		const FString Directory = FPaths::GetPath(Filename);
		if (!Directory.IsEmpty() && !IFileManager::Get().DirectoryExists(*Directory) && !IFileManager::Get().MakeDirectory(*Directory, true))
		{
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(TEXT("Failed to create directory '%s'."), *Directory);
			}
			return false;
		}

		FString Content;
		AppendLine(Content, Header);
		for (const TArray<FString>& Row : Rows)
		{
			AppendLine(Content, Row);
		}

		if (!FFileHelper::SaveStringToFile(Content, *Filename, FFileHelper::EEncodingOptions::ForceUTF8))
		{
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(TEXT("Failed to save CSV file '%s'."), *Filename);
			}
			return false;
		}

		return true;
	}

private:
	static FString EscapeField(const FString& Field)
	{
		const bool bNeedsQuotes = Field.Contains(TEXT(","))
			|| Field.Contains(TEXT("\""))
			|| Field.Contains(TEXT("\n"))
			|| Field.Contains(TEXT("\r"));

		if (!bNeedsQuotes)
		{
			return Field;
		}

		FString EscapedField = Field.Replace(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *EscapedField);
	}

	static void AppendLine(FString& OutContent, const TArray<FString>& Fields)
	{
		for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			if (FieldIndex > 0)
			{
				OutContent += TEXT(",");
			}

			OutContent += EscapeField(Fields[FieldIndex]);
		}

		OutContent += TEXT("\r\n");
	}

	TArray<FString> Header;
	TArray<TArray<FString>> Rows;
};
