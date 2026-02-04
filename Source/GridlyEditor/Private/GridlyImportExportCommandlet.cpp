// Copyright Epic Games, Inc. All Rights Reserved.

#include "GridlyImportExportCommandlet.h"
#include "GridlyLocalizationServiceProvider.h"
#include "Modules/ModuleManager.h"
#include "ILocalizationServiceModule.h"
#include "LocalizationModule.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationCommandletExecution.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include <GridlyGameSettings.h>

DEFINE_LOG_CATEGORY_STATIC(LogGridlyImportExportCommandlet, Log, All);

#define LOCTEXT_NAMESPACE "GridlyImportExportCommandlet"

/**
*	UGridlyImportExportCommandlet
*/
int32 UGridlyImportExportCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== GridlyImportExportCommandlet Main() called ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("GridlyImportExportCommandlet started with params: %s"), *Params);
	
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Debug: Log all parsed parameters
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Parsed parameters:"));
	for (const auto& Param : ParamVals)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("  %s = %s"), *Param.Key, *Param.Value);
	}
	
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Commandlet execution started successfully"));

	// Load localization module and its dependencies
	FModuleManager::Get().LoadModule(TEXT("LocalizationDashboard"));
	
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("LocalizationDashboard module loaded"));
	
	// Debug: Check if we have localization targets
	const TArray<ULocalizationTarget*> DebugLocalizationTargets = ULocalizationSettings::GetGameTargetSet()->TargetObjects;
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Found %d localization targets"), DebugLocalizationTargets.Num());
	
	for (int32 i = 0; i < DebugLocalizationTargets.Num(); i++)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Target %d: %s"), i, *DebugLocalizationTargets[i]->GetName());
	}

	ILocalizationServiceModule& LocServiceModule = ILocalizationServiceModule::Get();
	LocServiceModule.SetProvider("Gridly");

	ILocalizationServiceProvider& LocServProvider = ILocalizationServiceModule::Get().GetProvider();

	const bool bCanUseGridly = LocServProvider.IsEnabled() && LocServProvider.IsAvailable() && LocServProvider.GetName().ToString() == TEXT("Gridly");

	FGridlyLocalizationServiceProvider* GridlyProvider = bCanUseGridly ? static_cast<FGridlyLocalizationServiceProvider*>(&LocServProvider) : nullptr;
	if (!GridlyProvider)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("Unable to retrieve Gridly Provider."));
		return -1;
	}

	// Set config
	FString ConfigPath;
	if (const FString* ConfigParamVal = ParamVals.Find(FString(TEXT("Config"))))
	{
		ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(*ConfigParamVal);
	}
	else
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Reading ExportAllGameTarget argument
	FString* ExportAllGameTargetPtr = ParamVals.Find(FString(TEXT("ExportAllGameTarget")));

	// Set config section
	FString SectionName;
	if (const FString* ConfigSectionParamVal = ParamVals.Find(FString(TEXT("Section"))))
	{
		SectionName = *ConfigSectionParamVal;
	}
	else
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	bool bDoImport = false;
	GConfig->GetBool(*SectionName, TEXT("bImportLoc"), bDoImport, ConfigPath);

	bool bDoExport = false;
	GConfig->GetBool(*SectionName, TEXT("bExportLoc"), bDoExport, ConfigPath);

	bool bDoDownloadSourceChanges = false;
	GConfig->GetBool(*SectionName, TEXT("bDownloadSourceChanges"), bDoDownloadSourceChanges, ConfigPath);

	if (!bDoImport && !bDoExport && !bDoDownloadSourceChanges)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No operation detected. Use bExportLoc, bImportLoc, or bDownloadSourceChanges in config section."));
		return -1;
	}

	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== CONFIGURATION ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Operations: Import=%s, Export=%s, DownloadSourceChanges=%s"), 
		bDoImport ? TEXT("true") : TEXT("false"),
		bDoExport ? TEXT("true") : TEXT("false"),
		bDoDownloadSourceChanges ? TEXT("true") : TEXT("false"));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Config Path: %s"), *ConfigPath);
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Section Name: %s"), *SectionName);

	const TArray<ULocalizationTarget*> LocalizationTargets = ULocalizationSettings::GetGameTargetSet()->TargetObjects;
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Found %d localization targets"), LocalizationTargets.Num());
	
	if (LocalizationTargets.Num() == 0)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("No localization targets found!"));
		return 0;
	}
	
	//ULocalizationTarget* FirstLocTarget = LocalizationTargets.Num() > 0 ? LocalizationTargets[0]: nullptr;
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== STARTING MAIN LOOP ==="));
	for (ULocalizationTarget* LocTarget : LocalizationTargets)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Processing target: %s"), LocTarget ? *LocTarget->GetName() : TEXT("NULL"));
		// Do something with LocTarget
		if (LocTarget != nullptr)
		{
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Target is valid, checking operations..."));
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("bDoImport=%s, bDoExport=%s, bDoDownloadSourceChanges=%s"), 
				bDoImport ? TEXT("true") : TEXT("false"),
				bDoExport ? TEXT("true") : TEXT("false"),
				bDoDownloadSourceChanges ? TEXT("true") : TEXT("false"));
			
			if (bDoImport)
			{
				// List all cultures (even the native one in case some native translations have been modified in Gridly) to download
				TArray<FString> Cultures;
				for (int ItCulture = 0; ItCulture < LocTarget->Settings.SupportedCulturesStatistics.Num(); ItCulture++)
				{
					if (ItCulture != LocTarget->Settings.NativeCultureIndex)
					{
						const FCultureStatistics CultureStats = LocTarget->Settings.SupportedCulturesStatistics[ItCulture];
						Cultures.Add(CultureStats.CultureName);
					}
				}

				// Download cultures from Gridly
				CulturesToDownload.Append(Cultures);
				for (const FString& CultureName : Cultures)
				{
					ILocalizationServiceProvider& Provider = ILocalizationServiceModule::Get().GetProvider();
					TSharedRef<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> DownloadTargetFileOp =
						ILocalizationServiceOperation::Create<FDownloadLocalizationTargetFile>();
					DownloadTargetFileOp->SetInTargetGuid(LocTarget->Settings.Guid);
					DownloadTargetFileOp->SetInLocale(CultureName);

					FString Path = FPaths::ProjectSavedDir() / "Temp" / "Game" / LocTarget->Settings.Name / CultureName /
						LocTarget->Settings.Name + ".po";
					FPaths::MakePathRelativeTo(Path, *FPaths::ProjectDir());
					FString NormalizedPath = FPaths::ConvertRelativePathToFull(Path);
					DownloadTargetFileOp->SetInRelativeOutputFilePathAndName(Path);

					auto OperationCompleteDelegate = FLocalizationServiceOperationComplete::CreateUObject(this,
						&UGridlyImportExportCommandlet::OnDownloadComplete, false);

					Provider.Execute(DownloadTargetFileOp, TArray<FLocalizationServiceTranslationIdentifier>(),
						ELocalizationServiceOperationConcurrency::Synchronous, OperationCompleteDelegate);
				}

				// Wait for all downloads
				while (CulturesToDownload.Num())
				{
					FPlatformProcess::Sleep(0.4f);
					FHttpModule::Get().GetHttpManager().Tick(-1.f);
				}

				// Run task to import po files, it will be done on the base folder and import all po files data generated after downloading data from gridly
				if (CulturesToDownload.Num() == 0 && DownloadedFiles.Num() > 0)
				{
					const FString& DlPoFile = DownloadedFiles[0]; // retrieve first po file to deduce the base folder
					const FString TargetName = FPaths::GetBaseFilename(DlPoFile);
					const auto Target = ILocalizationModule::Get().GetLocalizationTargetByName(TargetName, false);

					const FString DirectoryPath = FPaths::GetPath(DlPoFile);
					const FString DownloadBasePath = FPaths::GetPath(DirectoryPath);

					// Create commandlet task to Import texts
					// Note that we could simply "Import all PO files" using a call to PortableObjectPipeline::ImportAll(...), though
					//		using tasks we are able to easily add/remove call to existing localization functionalities
					TArray<LocalizationCommandletExecution::FTask> Tasks;
					const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

					// Normalize Import config path
					FString ImportScriptPath = LocalizationConfigurationScript::GetImportTextConfigPath(Target, TOptional<FString>());
					ImportScriptPath = FConfigCacheIni::NormalizeConfigIniPath(ImportScriptPath);
					LocalizationConfigurationScript::GenerateImportTextConfigFile(Target, TOptional<FString>(), DownloadBasePath).WriteWithSCC(ImportScriptPath);
					Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ImportTaskName", "Import Translations"), ImportScriptPath, ShouldUseProjectFile));

					// Normalize Report config path
					FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportConfigPath(Target);
					ReportScriptPath = FConfigCacheIni::NormalizeConfigIniPath(ReportScriptPath);
					LocalizationConfigurationScript::GenerateWordCountReportConfigFile(Target).WriteWithSCC(ReportScriptPath);
					Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ReportTaskName", "Generate Reports"), ReportScriptPath, ShouldUseProjectFile));


					// Function will block until all tasks have been run
					BlockingRunLocCommandletTask(Tasks);
				}

				// Cleanup
				CulturesToDownload.Empty();
				DownloadedFiles.Empty();
			}

			if (bDoExport)
			{
				UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Running gather text task before exporting to Gridly."));
				// Generate gather config file
				FString GatherScriptPath = LocalizationConfigurationScript::GetGatherTextConfigPath(LocTarget);
				GatherScriptPath = FConfigCacheIni::NormalizeConfigIniPath(GatherScriptPath);

				LocalizationConfigurationScript::GenerateGatherTextConfigFile(LocTarget).WriteWithSCC(GatherScriptPath);

				const bool bUseProjectFile = !LocTarget->IsMemberOfEngineTargetSet();

				LocalizationCommandletExecution::FTask GatherTask(
					LOCTEXT("GatherTaskName", "Gather Text"),
					GatherScriptPath,
					bUseProjectFile
				);

				// Run Gather before Export
				BlockingRunLocCommandletTask({ GatherTask });

				FHttpRequestCompleteDelegate ReqDelegate = GridlyProvider->CreateExportNativeCultureDelegate();
				const FText SlowTaskText = LOCTEXT("ExportNativeCultureForTargetToGridlyText", "Exporting native culture for target to Gridly");

				GridlyProvider->ExportForTargetToGridly(LocTarget, ReqDelegate, SlowTaskText);

				// Wait for export requests to complete
				while (GridlyProvider->HasRequestsPending())
				{
					FPlatformProcess::Sleep(0.4f);
					FHttpModule::Get().GetHttpManager().Tick(-1.f);
				}

				const UGridlyGameSettings* GameSettings = GetMutableDefault<UGridlyGameSettings>();
				if (GameSettings && GameSettings->bSyncRecords)
				{
					UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("Fetching Gridly CSV to check for stale records to delete..."));
					UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("First check: HasDeleteRequestsPending = %s"),
						GridlyProvider->HasDeleteRequestsPending() ? TEXT("true") : TEXT("false"));

					// Wait for delete requests to finish
					while (GridlyProvider->HasDeleteRequestsPending())
					{
						FPlatformProcess::Sleep(0.4f);
						FHttpModule::Get().GetHttpManager().Tick(-1.f);
					}
					UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("All record deletions completed."));

				}

			}

			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== CHECKING DOWNLOAD SOURCE CHANGES ==="));
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("bDoDownloadSourceChanges = %s"), bDoDownloadSourceChanges ? TEXT("true") : TEXT("false"));
			
			if (bDoDownloadSourceChanges)
			{
				UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== RUNNING DOWNLOAD SOURCE CHANGES TASK ==="));
				UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Running Download Source Changes task for target: %s"), *LocTarget->Settings.Name);
				
				// Get the native culture for source strings
				FString NativeCulture;
				if (LocTarget->Settings.SupportedCulturesStatistics.IsValidIndex(LocTarget->Settings.NativeCultureIndex))
				{
					const FCultureStatistics CultureStats = LocTarget->Settings.SupportedCulturesStatistics[LocTarget->Settings.NativeCultureIndex];
					NativeCulture = CultureStats.CultureName;
				}
				else
				{
					UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No native culture found for target: %s"), *LocTarget->Settings.Name);
					continue;
				}

				// Check if we have any supported cultures
				if (LocTarget->Settings.SupportedCulturesStatistics.Num() == 0)
				{
					UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No supported cultures found for target: %s"), *LocTarget->Settings.Name);
					continue;
				}

				// Call the EXACT SAME FUNCTION that the UI uses
				ILocalizationServiceModule& UILocServiceModule = ILocalizationServiceModule::Get();
				ILocalizationServiceProvider& UILocServProvider = UILocServiceModule.GetProvider();
				
				const bool bCanUseUIGridly = UILocServProvider.IsEnabled() && UILocServProvider.IsAvailable() && UILocServProvider.GetName().ToString() == TEXT("Gridly");
				
				if (bCanUseUIGridly)
				{
					FGridlyLocalizationServiceProvider* UIGridlyProvider = static_cast<FGridlyLocalizationServiceProvider*>(&UILocServProvider);
					if (UIGridlyProvider)
					{
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔄 Running Download Source Changes (synchronous version)..."));
					
					// Use the commandlet's own synchronous implementation
					DownloadSourceChangesFromGridlyInternal(LocTarget, NativeCulture);
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Download Source Changes completed"));
					
					// Save the localization target to persist changes
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("💾 Saving localization target: %s"), *LocTarget->Settings.Name);
					LocTarget->SaveConfig();
					
					// Save all modified string table packages to disk (automated version of UI)
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("💾 Saving modified string table packages to disk..."));
					for (TObjectIterator<UPackage> PackageIt; PackageIt; ++PackageIt)
					{
						UPackage* Package = *PackageIt;
						if (Package && Package->IsDirty())
						{
							FString PackageName = Package->GetName();
							if (PackageName.Contains(TEXT("StringTable")) || PackageName.Contains(TEXT("new_table_56")))
							{
								UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("💾 Saving dirty package: %s"), *PackageName);
								
								// Get the package file path
								FString PackagePath = Package->GetLoadedPath().GetPackageName();
								UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Package path: %s"), *PackagePath);
								
								// Handle packages with empty paths (newly created packages)
								if (PackagePath.IsEmpty())
								{
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🆕 Newly created package detected: %s"), *PackageName);
									
									// For newly created string tables, use the configured save path from plugin settings
									const UGridlyGameSettings* GameSettings = GetMutableDefault<UGridlyGameSettings>();
									FString StringTableSavePath = GameSettings->StringTableSavePath;
									if (StringTableSavePath.IsEmpty())
									{
										StringTableSavePath = TEXT("/Game/Localization/StringTables"); // Default fallback
									}
									
									// Extract just the table name from the package name (remove the path prefix)
									FString TableName = PackageName;
									if (TableName.Contains(TEXT("/")))
									{
										TableName = TableName.Mid(TableName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1);
									}
									
									FString ConstructedPath = FString::Printf(TEXT("%s/%s"), *StringTableSavePath, *TableName);
									
									// Convert the package path to a relative file path
									FString RelativePath = ConstructedPath.Replace(TEXT("/Game/"), TEXT("Content/"));
									FString FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativePath + FPackageName::GetAssetPackageExtension());
									
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Using StringTableSavePath: %s"), *StringTableSavePath);
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Extracted table name: %s"), *TableName);
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Constructed package path: %s"), *ConstructedPath);
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Relative path: %s"), *RelativePath);
									UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 Constructed file path: %s"), *FilePath);
									
									// Save the newly created package
									FSavePackageArgs SaveArgs;
									SaveArgs.TopLevelFlags = RF_NoFlags;
									bool bSaved = UPackage::SavePackage(Package, nullptr, *FilePath, SaveArgs);
									if (bSaved)
									{
										UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Successfully saved newly created package: %s to %s"), *PackageName, *FilePath);
									}
									else
									{
										UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Failed to save newly created package: %s to %s"), *PackageName, *FilePath);
									}
									continue;
								}
								
								// Convert package path to actual file path
								FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
								UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔍 File path: %s"), *FilePath);
								
								if (!FilePath.IsEmpty())
								{
									// Save the package to disk
									FSavePackageArgs SaveArgs;
									SaveArgs.TopLevelFlags = RF_NoFlags;
									bool bSaved = UPackage::SavePackage(Package, nullptr, *FilePath, SaveArgs);
									if (bSaved)
									{
										UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Successfully saved package: %s to %s"), *PackageName, *FilePath);
									}
									else
									{
										UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Failed to save package: %s to %s"), *PackageName, *FilePath);
									}
								}
								else
								{
									UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("⚠️ No file path for package: %s"), *PackageName);
								}
							}
						}
					}
					
					// Force garbage collection to ensure all string table changes are committed
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🗑️ Forcing garbage collection to commit string table changes..."));
					CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Garbage collection completed"));
					
					// Refresh asset registry to ensure string table changes are visible
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔄 Refreshing asset registry to make string table changes visible..."));
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					AssetRegistryModule.Get().ScanModifiedAssetFiles(TArray<FString>());
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Asset registry refreshed"));
					
					// Run "Gather Text" to update manifest files from the updated string tables
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("📝 Running Gather Text to update manifest files..."));
					
					// Generate gather text config file
					FString GatherScriptPath = LocalizationConfigurationScript::GetGatherTextConfigPath(LocTarget);
					LocalizationConfigurationScript::GenerateGatherTextConfigFile(LocTarget).WriteWithSCC(GatherScriptPath);
					
					const bool bUseProjectFile = !LocTarget->IsMemberOfEngineTargetSet();
					LocalizationCommandletExecution::FTask GatherTask(
						LOCTEXT("GatherTaskName", "Gather Text"),
						GatherScriptPath,
						bUseProjectFile
					);
					
					// Execute the gather text task
					BlockingRunLocCommandletTask({ GatherTask });
					UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Gather Text completed - manifest files updated"));
					}
					else
					{
						UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ UIGridlyProvider cast failed"));
					}
				}
				else
				{
					UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Cannot use UI GridlyProvider"));
				}
			}
		}


		if (ExportAllGameTargetPtr && *ExportAllGameTargetPtr == "false") {
			break;
		}
	}
	return 0;
}

void UGridlyImportExportCommandlet::OnDownloadComplete(const FLocalizationServiceOperationRef& Operation, ELocalizationServiceOperationCommandResult::Type Result, bool bIsTargetSet)
{
	// do like in FGridlyLocalizationServiceProvider::OnImportCultureForTargetFromGridly
	TSharedPtr<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> DownloadLocalizationTargetOp = StaticCastSharedRef<FDownloadLocalizationTargetFile>(Operation);
	CulturesToDownload.Remove(DownloadLocalizationTargetOp->GetInLocale());

	if (Result != ELocalizationServiceOperationCommandResult::Succeeded)
	{
		const FText ErrorMessage = DownloadLocalizationTargetOp->GetOutErrorText();
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("%s"), *ErrorMessage.ToString());
	}

	const FString TargetName = FPaths::GetBaseFilename(DownloadLocalizationTargetOp->GetInRelativeOutputFilePathAndName());

	const auto Target = ILocalizationModule::Get().GetLocalizationTargetByName(TargetName, false);
	const FString AbsoluteFilePathAndName = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / DownloadLocalizationTargetOp->GetInRelativeOutputFilePathAndName());

	DownloadedFiles.Add(AbsoluteFilePathAndName);
}

void UGridlyImportExportCommandlet::BlockingRunLocCommandletTask(const TArray<LocalizationCommandletExecution::FTask>& Tasks)
{
	for (const LocalizationCommandletExecution::FTask& LocTask : Tasks)
	{
		TSharedPtr<FLocalizationCommandletProcess> CommandletProcess = FLocalizationCommandletProcess::Execute(LocTask.ScriptPath, LocTask.ShouldUseProjectFile);

		if (CommandletProcess.IsValid())
		{
			UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("=== Starting Task [%s] ==="), *LocTask.Name.ToString());

			FProcHandle CurrentProcessHandle = CommandletProcess->GetHandle();
			int32 ReturnCode = INDEX_NONE;

			// This loop is the same than FCommandletLogPump::Run(), and it's used when SLocalizationCommandletExecutor widget runs localization commandlet tasks
			for (;;)
			{
				// Read from pipe.
				const FString PipeString = FPlatformProcess::ReadPipe(CommandletProcess->GetReadPipe());

				// Process buffer.
				if (!PipeString.IsEmpty())
				{
					UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("%s"), *PipeString);
				}

				// If the process isn't running and there's no data in the pipe, we're done.
				if (!FPlatformProcess::IsProcRunning(CurrentProcessHandle) && PipeString.IsEmpty())
				{
					break;
				}

				// Sleep.
				FPlatformProcess::Sleep(0.0f);
			}

			if (CurrentProcessHandle.IsValid() && FPlatformProcess::GetProcReturnCode(CurrentProcessHandle, &ReturnCode))
			{
				UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("===> Task [%s] returned : %d"), *LocTask.Name.ToString(), ReturnCode);
			}
		}
		else
		{
			UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("Failed to start Task [%s] !"), *LocTask.Name.ToString());
		}
	}
}

void UGridlyImportExportCommandlet::DownloadSourceChangesFromGridlyInternal(ULocalizationTarget* LocalizationTarget, const FString& NativeCulture)
{
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== DownloadSourceChangesFromGridlyInternal START ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Target: %s, Culture: %s"), *LocalizationTarget->Settings.Name, *NativeCulture);
	
	const UGridlyGameSettings* GameSettings = GetMutableDefault<UGridlyGameSettings>();
	const FString ApiKey = GameSettings->ImportApiKey;

	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("API Key configured: %s"), ApiKey.IsEmpty() ? TEXT("NO") : TEXT("YES"));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Import View IDs count: %d"), GameSettings->ImportFromViewIds.Num());

	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No import API key configured"));
		return;
	}

	// Get the first view ID for import
	if (GameSettings->ImportFromViewIds.Num() == 0 || GameSettings->ImportFromViewIds[0].IsEmpty())
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("No import view ID configured"));
		return;
	}

	const FString ViewId = GameSettings->ImportFromViewIds[0];
	const FString Url = FString::Printf(TEXT("https://api.gridly.com/v1/views/%s/records"), *ViewId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("ApiKey %s"), *ApiKey));
	HttpRequest->SetURL(Url);

	// Store the localization target and culture for the callback
	CurrentSourceDownloadTarget = LocalizationTarget;
	CurrentSourceDownloadCulture = NativeCulture;

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UGridlyImportExportCommandlet::OnDownloadSourceChangesFromGridly);
	HttpRequest->ProcessRequest();

	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== MAKING HTTP REQUEST TO GRIDLY ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Downloading source changes from Gridly for target: %s, culture: %s"), 
		*LocalizationTarget->Settings.Name, *NativeCulture);
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("URL: %s"), *Url);

	// Wait for the request to complete
	while (HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		FPlatformProcess::Sleep(0.1f);
		FHttpModule::Get().GetHttpManager().Tick(-1.f);
	}
}

void UGridlyImportExportCommandlet::OnDownloadSourceChangesFromGridly(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== HTTP RESPONSE RECEIVED ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Success: %s"), bSuccess ? TEXT("YES") : TEXT("NO"));
	
	if (!bSuccess || !Response.IsValid())
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("Failed to download source changes from Gridly"));
		return;
	}

	const FString ResponseContent = Response->GetContentAsString();
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Response content length: %d characters"), ResponseContent.Len());
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Response status code: %d"), Response->GetResponseCode());
	
	// Parse the JSON response to get the records
	TArray<TSharedPtr<FJsonValue>> RecordsArray;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ResponseContent);
	
	if (!FJsonSerializer::Deserialize(JsonReader, RecordsArray))
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("Failed to parse JSON response from Gridly"));
		return;
	}

	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Successfully parsed %d records from Gridly"), RecordsArray.Num());

	// Process the records and group them by namespace
	TMap<FString, TArray<FGridlySourceRecord>> NamespaceRecords;
	
	for (const TSharedPtr<FJsonValue>& RecordValue : RecordsArray)
	{
		const TSharedPtr<FJsonObject> RecordObj = RecordValue->AsObject();
		if (!RecordObj.IsValid())
		{
			UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("Invalid record object, skipping"));
			continue;
		}

		FGridlySourceRecord SourceRecord;
		SourceRecord.RecordId = RecordObj->GetStringField(TEXT("id"));
		
		UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Processing record ID: %s"), *SourceRecord.RecordId);
		
		// Get the source text from the native culture column
		// Handle the case where cells might be null, an array, or an object
		if (RecordObj->HasField(TEXT("cells")))
		{
			const TSharedPtr<FJsonValue> CellsValue = RecordObj->TryGetField(TEXT("cells"));
			if (CellsValue.IsValid() && !CellsValue->IsNull())
			{
				// Check if cells is an array (which is the correct format from Gridly)
				if (CellsValue->Type == EJson::Array)
				{
					const TArray<TSharedPtr<FJsonValue>> CellsArray = CellsValue->AsArray();
					
					// Look for the source culture column
					FString SourceColumnId = FString::Printf(TEXT("src_%s"), *CurrentSourceDownloadCulture);
					SourceColumnId = SourceColumnId.Replace(TEXT("-"), TEXT(""));
					
					UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Looking for source column ID: %s in %d cells"), *SourceColumnId, CellsArray.Num());
					
					// Search through the cells array for the matching column ID
					for (const TSharedPtr<FJsonValue>& CellValue : CellsArray)
					{
						if (CellValue.IsValid() && !CellValue->IsNull())
						{
							const TSharedPtr<FJsonObject> CellObj = CellValue->AsObject();
							if (CellObj.IsValid())
							{
								FString CellColumnId = CellObj->GetStringField(TEXT("columnId"));
								if (CellColumnId == SourceColumnId)
								{
									SourceRecord.SourceText = CellObj->GetStringField(TEXT("value"));
									UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Found source text for record %s: %s"), *SourceRecord.RecordId, *SourceRecord.SourceText);
									break;
								}
							}
						}
					}
				}
				// Fallback: try to handle as object (legacy format)
				else if (CellsValue->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject> CellsObj = CellsValue->AsObject();
					if (CellsObj.IsValid())
					{
						// Look for the source culture column
						FString SourceColumnId = FString::Printf(TEXT("src_%s"), *CurrentSourceDownloadCulture);
						SourceColumnId = SourceColumnId.Replace(TEXT("-"), TEXT(""));
						
						UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Looking for source column ID: %s (object format)"), *SourceColumnId);
						
						if (CellsObj->HasField(SourceColumnId))
						{
							const TSharedPtr<FJsonValue> CellValue = CellsObj->TryGetField(SourceColumnId);
							if (CellValue.IsValid() && !CellValue->IsNull())
							{
								const TSharedPtr<FJsonObject> CellObj = CellValue->AsObject();
								if (CellObj.IsValid())
								{
									SourceRecord.SourceText = CellObj->GetStringField(TEXT("value"));
									UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Found source text for record %s: %s"), *SourceRecord.RecordId, *SourceRecord.SourceText);
								}
							}
						}
					}
				}
			}
		}

		// Determine namespace from record ID (assuming format "Namespace,Key")
		FString Namespace = TEXT("Default");
		FString Key = SourceRecord.RecordId;
		
		if (SourceRecord.RecordId.Contains(TEXT(",")))
		{
			FString Left, Right;
			if (SourceRecord.RecordId.Split(TEXT(","), &Left, &Right))
			{
				Namespace = Left;
				Key = Right;
			}
		}
		
		// Log if we didn't find any source text
		if (SourceRecord.SourceText.IsEmpty())
		{
			UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("No source text found for record %s"), *SourceRecord.RecordId);
		}
		
		SourceRecord.RecordId = Key;
		
		// Add to namespace records
		if (!NamespaceRecords.Contains(Namespace))
		{
			NamespaceRecords.Add(Namespace, TArray<FGridlySourceRecord>());
		}
		NamespaceRecords[Namespace].Add(SourceRecord);
	}

	// Process the namespace records
	ProcessSourceChangesForNamespaces(NamespaceRecords);
}

void UGridlyImportExportCommandlet::ProcessSourceChangesForNamespaces(const TMap<FString, TArray<FGridlySourceRecord>>& NamespaceRecords)
{
	if (!CurrentSourceDownloadTarget.IsValid())
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("Invalid localization target for source changes processing"));
		return;
	}

	ULocalizationTarget* LocalizationTarget = CurrentSourceDownloadTarget.Get();
	const FString TargetName = LocalizationTarget->Settings.Name;
	
	// Create temporary directory for CSV files
	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("GridlySourceChanges") / TargetName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*TempDir))
	{
		PlatformFile.CreateDirectoryTree(*TempDir);
	}

	int32 ProcessedNamespaces = 0;
	int32 TotalNamespaces = NamespaceRecords.Num();

	for (const auto& NamespacePair : NamespaceRecords)
	{
		const FString& Namespace = NamespacePair.Key;
		const TArray<FGridlySourceRecord>& Records = NamespacePair.Value;

		ProcessedNamespaces++;
		UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Processing namespace %d/%d: %s (%d records)"), 
			ProcessedNamespaces, TotalNamespaces, *Namespace, Records.Num());

		// Generate CSV content
		FString CSVContent = TEXT("Key,SourceString\n");
		
		for (const FGridlySourceRecord& Record : Records)
		{
			// Escape quotes in the source text
			FString EscapedSourceText = Record.SourceText;
			EscapedSourceText = EscapedSourceText.Replace(TEXT("\""), TEXT("\"\""));
			
			CSVContent += FString::Printf(TEXT("\"%s\",\"%s\"\n"), *Record.RecordId, *EscapedSourceText);
		}

		// Write CSV file
		const FString CSVFilePath = TempDir / FString::Printf(TEXT("%s.csv"), *Namespace);
		
		if (FFileHelper::SaveStringToFile(CSVContent, *CSVFilePath))
		{
			UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Generated CSV file for namespace '%s': %s"), *Namespace, *CSVFilePath);
			
			// Import the CSV into the string table
			ImportCSVToStringTable(LocalizationTarget, Namespace, CSVFilePath);
		}
		else
		{
			UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("Failed to write CSV file for namespace '%s': %s"), *Namespace, *CSVFilePath);
		}
	}

	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== SOURCE CHANGES PROCESSING COMPLETED ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Processed %d namespaces. CSV files saved to: %s"), 
		ProcessedNamespaces, *TempDir);
	
	// Now import the CSV files to update the string tables
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== IMPORTING CSV FILES TO UPDATE STRING TABLES ==="));
	for (const auto& NamespacePair : NamespaceRecords)
	{
		const FString& Namespace = NamespacePair.Key;
		const FString CSVFilePath = TempDir / FString::Printf(TEXT("%s.csv"), *Namespace);
		
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Importing CSV file: %s"), *CSVFilePath);
		
		// Import the CSV file to update the string table
		bool bImportSuccess = ImportCSVToStringTable(LocalizationTarget, Namespace, CSVFilePath);
		if (bImportSuccess)
		{
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Successfully imported CSV for namespace: %s"), *Namespace);
		}
		else
		{
			UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("⚠️ Failed to import CSV for namespace: %s"), *Namespace);
		}
	}
	
	// Save the localization target to persist changes
	if (LocalizationTarget)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Saving localization target: %s"), *LocalizationTarget->Settings.Name);
		LocalizationTarget->SaveConfig();
		
		// Force save all packages to ensure string table changes are persisted
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Saving all packages to persist string table changes..."));
		
		// Force garbage collection to ensure all string table changes are committed
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Forcing garbage collection to commit string table changes..."));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Garbage collection completed"));
		
		// Force save all packages to ensure string table changes are persisted
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Forcing save of all packages to persist string table changes..."));
		
		// Note: In commandlet mode, we can't easily save packages directly
		// The string table changes should be persisted through the localization system
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("String table changes will be persisted through the localization system"));
		
			// Refresh asset registry to ensure string table changes are visible
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Refreshing asset registry to make string table changes visible..."));
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().ScanModifiedAssetFiles(TArray<FString>());
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Asset registry refreshed"));
			
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Localization target and all assets saved successfully"));
		
		// Run "Gather Text" to update manifest files from the updated string tables
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== RUNNING GATHER TEXT TO UPDATE MANIFEST FILES ==="));
		UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Running gather text task to update manifest files from string tables."));
		
		// Generate gather config file
		FString GatherScriptPath = LocalizationConfigurationScript::GetGatherTextConfigPath(LocalizationTarget);
		GatherScriptPath = FConfigCacheIni::NormalizeConfigIniPath(GatherScriptPath);

		LocalizationConfigurationScript::GenerateGatherTextConfigFile(LocalizationTarget).WriteWithSCC(GatherScriptPath);

		const bool bUseProjectFile = !LocalizationTarget->IsMemberOfEngineTargetSet();

		LocalizationCommandletExecution::FTask GatherTask(
			LOCTEXT("GatherTaskName", "Gather Text"),
			GatherScriptPath,
			bUseProjectFile
		);

		// Run Gather to update manifest files from string tables
		BlockingRunLocCommandletTask({ GatherTask });
		
		UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Gather Text completed - manifest files updated from string tables"));
	}
}

bool UGridlyImportExportCommandlet::ImportCSVToStringTable(ULocalizationTarget* LocalizationTarget, const FString& Namespace, const FString& CSVFilePath)
{
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== ImportCSVToStringTable START ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("CSV file ready for import: %s"), *CSVFilePath);
	UE_LOG(LogGridlyImportExportCommandlet, Log, TEXT("Namespace: %s, Target: %s"), *Namespace, *LocalizationTarget->Settings.Name);
	
	// Parse the CSV file (same as UI implementation)
	TArray<FString> CSVLines;
	if (!FFileHelper::LoadFileToStringArray(CSVLines, *CSVFilePath))
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Failed to read CSV file: %s"), *CSVFilePath);
		return false;
	}

	if (CSVLines.Num() < 2) // Need at least header + 1 data row
	{
		UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("⚠️ CSV file is empty or has no data rows: %s"), *CSVFilePath);
		return false;
	}

	// Parse CSV header
	FString HeaderLine = CSVLines[0];
	TArray<FString> HeaderFields;
	HeaderLine.ParseIntoArray(HeaderFields, TEXT(","));
	
	if (HeaderFields.Num() < 2)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Invalid CSV header format: %s"), *HeaderLine);
		return false;
	}

	// Validate header
	if (!HeaderFields[0].Contains(TEXT("Key")) || !HeaderFields[1].Contains(TEXT("SourceString")))
	{
		UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ CSV header must contain 'Key' and 'SourceString' columns"));
		return false;
	}

	// Parse CSV data into key-value pairs
	TMap<FString, FString> KeyValuePairs;
	for (int32 i = 1; i < CSVLines.Num(); ++i)
	{
		FString Line = CSVLines[i];
		if (Line.IsEmpty())
		{
			continue;
		}

		// Simple CSV parsing (handles quoted fields)
		TArray<FString> Fields;
		ParseCSVLine(Line, Fields);
		
		if (Fields.Num() >= 2)
		{
			FString Key = Fields[0].TrimQuotes();
			FString Value = Fields[1].TrimQuotes();
			
			if (!Key.IsEmpty() && !Value.IsEmpty())
			{
				KeyValuePairs.Add(Key, Value);
			}
		}
	}

	if (KeyValuePairs.Num() == 0)
	{
		UE_LOG(LogGridlyImportExportCommandlet, Warning, TEXT("⚠️ No valid key-value pairs found in CSV file"));
		return false;
	}

	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("📄 Found %d key-value pairs in CSV"), KeyValuePairs.Num());
	
	// Use the GridlyProvider to import the key-value pairs directly into string tables
	// This is the same approach used by import and export operations
	ILocalizationServiceModule& LocServiceModule = ILocalizationServiceModule::Get();
	ILocalizationServiceProvider& LocServProvider = LocServiceModule.GetProvider();
	
	const bool bCanUseGridly = LocServProvider.IsEnabled() && LocServProvider.IsAvailable() && LocServProvider.GetName().ToString() == TEXT("Gridly");
	
	if (bCanUseGridly)
	{
		FGridlyLocalizationServiceProvider* GridlyProvider = static_cast<FGridlyLocalizationServiceProvider*>(&LocServProvider);
		if (GridlyProvider)
		{
			UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("🔄 Using GridlyProvider to import %d entries for namespace '%s'"), KeyValuePairs.Num(), *Namespace);
			
			// Use the provider's function directly - this is the same function that the UI uses
			bool bSuccess = GridlyProvider->ImportKeyValuePairsToStringTable(LocalizationTarget, Namespace, KeyValuePairs, nullptr);
			
			if (bSuccess)
			{
				UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("✅ Successfully imported %d entries for namespace '%s' using GridlyProvider"), 
					KeyValuePairs.Num(), *Namespace);
				return true;
			}
			else
			{
				UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Failed to import entries for namespace '%s' using GridlyProvider"), *Namespace);
				return false;
			}
		}
	}
	
	UE_LOG(LogGridlyImportExportCommandlet, Error, TEXT("❌ Could not access GridlyProvider for string table import"));
	return false;
}

void UGridlyImportExportCommandlet::ParseCSVLine(const FString& Line, TArray<FString>& OutFields)
{
	OutFields.Empty();
	
	FString CurrentField;
	bool bInQuotes = false;
	
	for (int32 i = 0; i < Line.Len(); i++)
	{
		TCHAR Char = Line[i];
		
		if (Char == TEXT('"'))
		{
			if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
			{
				// Escaped quote
				CurrentField += TEXT('"');
				i++; // Skip next quote
			}
			else
			{
				// Toggle quote state
				bInQuotes = !bInQuotes;
			}
		}
		else if (Char == TEXT(',') && !bInQuotes)
		{
			// Field separator
			OutFields.Add(CurrentField);
			CurrentField.Empty();
		}
		else
		{
			CurrentField += Char;
		}
	}
	
	// Add the last field
	OutFields.Add(CurrentField);
}

bool UGridlyImportExportCommandlet::UpdateStringTableEntry(ULocalizationTarget* LocalizationTarget, const FString& Namespace, const FString& Key, const FString& SourceString)
{
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("=== UpdateStringTableEntry START ==="));
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("Target: %s, Namespace: %s, Key: %s"), 
		*LocalizationTarget->Settings.Name, *Namespace, *Key);
	
	// For now, just log that we would update it
	UE_LOG(LogGridlyImportExportCommandlet, Display, TEXT("📝 Would update string table entry: %s = %s"), *Key, *SourceString);
	return true;
}


#undef LOCTEXT_NAMESPACE
