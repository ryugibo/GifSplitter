// Copyright 2018 Ryugibo, Inc. All Rights Reserved.

#include "GifSplitter.h"

#define LOCTEXT_NAMESPACE "FGifSplitterModule"

void FGifSplitterModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FGifSplitterModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGifSplitterModule, GifSplitter)

DEFINE_LOG_CATEGORY(LogGifSplitter)
