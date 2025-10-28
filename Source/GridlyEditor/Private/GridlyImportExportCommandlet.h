// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "LocalizationCommandletExecution.h"
#include "ILocalizationServiceProvider.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "GridlyImportExportCommandlet.generated.h"

// Forward declarations
class ULocalizationTarget;
class UStringTable;

// Structure to hold Gridly source record data
struct FGridlySourceRecord
{
	FString RecordId;
	FString SourceText;
};

/**
 *	GridlyImportExportCommandlet: Commandlet to Export Native Texts to Gridy and Import translations from Gridly.
 */
UCLASS()
class UGridlyImportExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGridlyImportExportCommandlet(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{}

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	TArray<FString> CulturesToDownload;
	TArray<FString> DownloadedFiles;

	// Download Source Changes functionality
	TWeakObjectPtr<ULocalizationTarget> CurrentSourceDownloadTarget;
	FString CurrentSourceDownloadCulture;

private:
	void OnDownloadComplete(const FLocalizationServiceOperationRef& Operation, ELocalizationServiceOperationCommandResult::Type Result, bool bIsTargetSet);
	void BlockingRunLocCommandletTask(const TArray<LocalizationCommandletExecution::FTask>& LocTasks);
	
	// Download Source Changes methods
	void DownloadSourceChangesFromGridlyInternal(ULocalizationTarget* LocalizationTarget, const FString& NativeCulture);
	void OnDownloadSourceChangesFromGridly(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	void ProcessSourceChangesForNamespaces(const TMap<FString, TArray<FGridlySourceRecord>>& NamespaceRecords);
	bool ImportCSVToStringTable(ULocalizationTarget* LocalizationTarget, const FString& Namespace, const FString& CSVFilePath);
	void ParseCSVLine(const FString& Line, TArray<FString>& OutFields);
	bool UpdateStringTableEntry(ULocalizationTarget* LocalizationTarget, const FString& Namespace, const FString& Key, const FString& SourceString);
};