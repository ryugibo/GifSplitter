// Copyright 2018 Ryugibo, Inc. All Rights Reserved.

#include "Giflipbook.h"

#define LOCTEXT_NAMESPACE "FGiflipbookModule"

void FGiflipbookModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FGiflipbookModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGiflipbookModule, Giflipbook)

DEFINE_LOG_CATEGORY(LogGiflipbook)
