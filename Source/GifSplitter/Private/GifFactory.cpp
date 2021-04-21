// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#include "GifFactory.h"
#include "GifSplitter.h"
#include "AssetRegistryModule.h"
#include "Factories.h"
#include "Serialization/BufferArchive.h"
#include "PackageTools.h"
#include "gif_load/gif_load.h"


const long UGifFactory::INTERLACED_OFFSETS[] = { 0, 4, 2, 1 };
const long UGifFactory::INTERLACED_JUMPS[] = { 8, 8, 4, 2 };

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
	CleanUp();

	FGifData GifData;
	GIF_Load((void*)Buffer, BufferEnd - Buffer, UGifFactory::Frame, 0, (void*)&GifData, 0L);

	UTexture2D* LastObject = nullptr;
	for (int i = 0; i < GifData.Buffer.Num(); ++i) {
		const TArray<uint8>& FrameBuffer = GifData.Buffer[i];
		const uint8* PtrFrameBuffer = FrameBuffer.GetData();
		const uint8* PtrFrameBufferEnd = PtrFrameBuffer + FrameBuffer.Num();

		FString FileName = UPackageTools::SanitizePackageName(FString::Printf(TEXT("%s_%d"), *Name.ToString(), i));
		FString NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + FileName;
		UPackage* Package = CreatePackage(*NewPackageName);
		Package->FullyLoad();
		LastObject = Cast<UTexture2D>(Super::FactoryCreateBinary(Class, Package, *FileName, Flags, Context, Type, PtrFrameBuffer, PtrFrameBufferEnd, Warn));
		if (LastObject == nullptr)
		{
			UE_LOG(LogGifSplitter, Error, TEXT("Failed import gif frame %d / %d"), i + 1, GifData.Buffer.Num());
			break;
		}
		if (i + 1 < GifData.Buffer.Num())
		{
			FAssetRegistryModule::AssetCreated(LastObject);
			LastObject->MarkPackageDirty();
			LastObject->PostEditChange();
			AdditionalImportedObjects.Add(LastObject);
		}
	}
	return LastObject;
}

void UGifFactory::Frame(void* RawData, GIF_WHDR* GifFrame)
{
	check(RawData != nullptr);
	check(GifFrame != nullptr);

	FGifData* GifData = (FGifData*)RawData;

	if (GifFrame->ifrm == 0)
	{
		uint32 BackgroundColor = GetFrameFromPalette(GifFrame, GetBackgroundIndex(GifFrame));
		GifData->LastValidPixel.Init(BackgroundColor, GifFrame->xdim * GifFrame->ydim);
		GifData->BeforePixel.Init(BackgroundColor, GifFrame->xdim * GifFrame->ydim);
	}

	FTGAFileHeader TGAHeader;

	TGAHeader.IdFieldLength = 0; // 0
	TGAHeader.ColorMapType = 0; // 1
	TGAHeader.ImageTypeCode = 2; // 2
	TGAHeader.ColorMapOrigin = 0; // 3
	TGAHeader.ColorMapLength = 0; // 5
	TGAHeader.ColorMapEntrySize = 0; // 7
	TGAHeader.XOrigin; // 8
	TGAHeader.YOrigin; // 10
	TGAHeader.Width = GifFrame->xdim; // 12
	TGAHeader.Height = GifFrame->ydim; // 14
	TGAHeader.BitsPerPixel = 32; // 16
	TGAHeader.ImageDescriptor = 0x20; // 17

	FBufferArchive Buffer;
	Buffer << TGAHeader;

	if (GifFrame->intr == 0)
	{
		for (long yd_i = 0; yd_i < GifFrame->ydim; ++yd_i)
		{
			for (long xd_i = 0; xd_i < GifFrame->xdim; ++xd_i)
			{
				uint32 Frame = ParseFrame(xd_i, yd_i, GifFrame, GifData);
				Buffer << Frame;
			}
		}
	}
	else
	{
		for (long pass_i = 0; pass_i < 4; ++pass_i)
		{
			for (long yd_i = INTERLACED_OFFSETS[pass_i]; yd_i < GifFrame->ydim; yd_i += INTERLACED_JUMPS[pass_i])
			{
				for (long xd_i = 0; xd_i < GifFrame->xdim; ++xd_i)
				{
					uint32 Frame = ParseFrame(xd_i, yd_i, GifFrame, GifData);
					Buffer << Frame;
				}
			}
		}
	}

	GifData->Buffer.Add(Buffer);
}

uint32 UGifFactory::ParseFrame(long IndexX, long IndexY, GIF_WHDR* GifFrame, FGifData* GifData)
{
	check(GifFrame != nullptr);
	check(GifData != nullptr);

	long Index = IndexX + (IndexY * GifFrame->xdim);

	bool IsXInFrame = FMath::IsWithin(IndexX, GifFrame->frxo, GifFrame->frxo + GifFrame->frxd);
	bool IsYInFrame = FMath::IsWithin(IndexY, GifFrame->fryo, GifFrame->fryo + GifFrame->fryd);

	uint32 OutFrame = 0;
	if (IsXInFrame && IsYInFrame)
	{
		long FrameIndex = (IndexX - GifFrame->frxo) + ((IndexY - GifFrame->fryo) * GifFrame->frxd);
		uint32 PaletteIndex = GifFrame->bptr[FrameIndex];

		if (PaletteIndex != GetBackgroundIndex(GifFrame))
		{
			OutFrame = GetFrameFromPalette(GifFrame, PaletteIndex);
		}
	}

	if (OutFrame == 0)
	{
		OutFrame = GifData->LastValidPixel[Index];
	}
	else
	{
		GifData->LastValidPixel[Index] = OutFrame;
	}

	if (IsXInFrame && IsYInFrame)
	{
		switch (GifFrame->mode)
		{
		case GIF_NONE:
		case GIF_CURR:
			GifData->BeforePixel[Index] = OutFrame;
			break;
		case GIF_BKGD:
			GifData->BeforePixel[Index] = GetFrameFromPalette(GifFrame, GetBackgroundIndex(GifFrame));
			break;
		case GIF_PREV:
			GifData->BeforePixel[Index] = GifData->LastValidPixel[Index];
			break;
		}
	}

	return OutFrame;
}

long UGifFactory::GetBackgroundIndex(GIF_WHDR* GifFrame)
{
	check(GifFrame != nullptr);

	return (GifFrame->tran >= 0) ? GifFrame->tran : GifFrame->bkgd;
}

uint32 UGifFactory::GetFrameFromPalette(GIF_WHDR* GifFrame, uint32 PaletteIndex)
{
	check(GifFrame != nullptr);

	uint32 OutFrame = 0x00000000;

#if GIF_BIGE
	OutFrame = (GifFrame->cpal[PaletteIndex].R << 8) | (GifFrame->cpal[PaletteIndex].G << 16) | (GifFrame->cpal[PaletteIndex].B << 24);
#else
	OutFrame = (GifFrame->cpal[PaletteIndex].R << 16) | (GifFrame->cpal[PaletteIndex].G << 8) | (GifFrame->cpal[PaletteIndex].B);
#endif // GIF_BIGE

	if (PaletteIndex != GetBackgroundIndex(GifFrame))
	{
#if GIF_BIGE
		OutFrame = OutFrame | 0xFF;
#else
		OutFrame = OutFrame | 0xFF000000;
#endif // GIF_BIGE
	}

	return OutFrame;
}
