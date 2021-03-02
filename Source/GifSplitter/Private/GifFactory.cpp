// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#include "GifFactory.h"
#include "GifSplitter.h"
#include "AssetRegistryModule.h"
#include "Factories.h"
#include "Serialization/BufferArchive.h"
#include "gif_load/gif_load.h"


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
	TArray<TArray<uint8>> GifData;
	GIF_Load((void*)Buffer, BufferEnd - Buffer, UGifFactory::Frame, 0, (void*)&GifData, 0L);

	UObject* LastObject = nullptr;
	for (int i = 0; i < GifData.Num(); ++i) {
		TArray<uint8>& FrameBuffer = GifData[i];
		const uint8* PtrFrameBuffer = FrameBuffer.GetData();
		const uint8* PtrFrameBufferEnd = PtrFrameBuffer + FrameBuffer.Num();

		FString FileName = FString::Printf(TEXT("%s_%d"), *Name.ToString(), i);
		LastObject = Super::FactoryCreateBinary(Class, InParent, *FileName, Flags, Context, Type, PtrFrameBuffer, PtrFrameBufferEnd, Warn);
		if (i < (GifData.Num() - 1))
		{
			FAssetRegistryModule::AssetCreated(LastObject);
			LastObject->MarkPackageDirty();
			LastObject->PostEditChange();
		}
	}
	return LastObject;
}

void UGifFactory::Frame(void* RawData, GIF_WHDR* GifFrame)
{
	TArray<TArray<uint8>>* GifData = (TArray<TArray<uint8>>*)RawData;
	const TArray<uint8>& BeforeFrame = (GifData->Num() == 0) ? TArray<uint8>() : GifData->Last();

	FTGAFileHeader TGAHeader;

	TGAHeader.IdFieldLength; // 0
	TGAHeader.ColorMapType = 0; // 1
	TGAHeader.ImageTypeCode = 2; // 2
	TGAHeader.ColorMapOrigin; // 3
	TGAHeader.ColorMapLength; // 5
	TGAHeader.ColorMapEntrySize; // 7
	TGAHeader.XOrigin; // 8
	TGAHeader.YOrigin; // 10
	TGAHeader.Width = GifFrame->xdim; // 12
	TGAHeader.Height = GifFrame->ydim; // 14
	TGAHeader.BitsPerPixel = 32; // 16
	TGAHeader.ImageDescriptor = 0x20; // 17

	FBufferArchive Buffer;
	Buffer << TGAHeader;

	uint32 Size = GifFrame->xdim * GifFrame->ydim;

	for (uint32 i = 0; i < Size; ++i)
	{
		uint32 Frame = 0;
		if (GifFrame->tran != GifFrame->bptr[i])
		{
			// RRGGBBAA
			Frame = (GifFrame->cpal[GifFrame->bptr[i]].R << 24) | (GifFrame->cpal[GifFrame->bptr[i]].G << 16) | (GifFrame->cpal[GifFrame->bptr[i]].B << 8) | 0xFF;
		}

		Buffer << Frame;
	}

	GifData->Add(Buffer);
}
