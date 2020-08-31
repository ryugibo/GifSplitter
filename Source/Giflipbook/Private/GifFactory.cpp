// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#include "GifFactory.h"

#include "Giflipbook.h"

#include "Engine/Texture2D.h"

UGifFactory::UGifFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTexture2D::StaticClass();

	Formats.Add(TEXT("gif;Texture"));
}

UObject* UGifFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	UTexture2D* Texture = nullptr;
	GIF_Load((void*)Buffer, BufferEnd - Buffer, UGifFactory::Frame, 0, (void*)Texture, 0L);
	return Texture;
}


void UGifFactory::Frame(void* data, GIF_WHDR* whdr)
{
	UE_LOG(LogGiflipbook, Log, TEXT("VVVVVVVVVVVVVVVVVVVVVVVV"));

	long size = whdr->xdim * whdr->ydim;
	for (int i = 0; i < size; ++i)
	{
		UE_LOG(LogGiflipbook, Log, TEXT("\tR: %d"), whdr->cpal[whdr->bptr[i]].R);
		UE_LOG(LogGiflipbook, Log, TEXT("\tG: %d"), whdr->cpal[whdr->bptr[i]].G);
		UE_LOG(LogGiflipbook, Log, TEXT("\tB: %d"), whdr->cpal[whdr->bptr[i]].B);
	}

	UE_LOG(LogGiflipbook, Log, TEXT("AAAAAAAAAAAAAAAAAAAAAAAA"));
}

#undef LOCTEXT_NAMESPACE
