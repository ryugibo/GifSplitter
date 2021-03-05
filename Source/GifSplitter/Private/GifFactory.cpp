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
		GifData->LastValidPixel.Init(GetBackground(GifFrame), GifFrame->xdim * GifFrame->ydim);
		GifData->BeforePixel.Init(GetBackground(GifFrame), GifFrame->xdim * GifFrame->ydim);
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
	uint32 OutFrame = 0;
	if (FMath::IsWithin(IndexX, GifFrame->frxo, GifFrame->frxd) &&
		FMath::IsWithin(IndexY, GifFrame->fryo, GifFrame->fryd))
	{
		long FrameIndex = (IndexX - GifFrame->frxo) + ((IndexY - GifFrame->fryo) * GifFrame->frxd);
		if (GifFrame->tran != GifFrame->bptr[FrameIndex])
		{
#if GIF_BIGE
			OutFrame = 0xFF | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].R << 8) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].G << 16) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].B << 24);
#else
			OutFrame = 0xFF000000 | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].R << 16) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].G << 8) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].B);
#endif // GIF_BIGE
		}
	}

	if (OutFrame == 0)
	{
		OutFrame = GifData->BeforePixel[Index];
	}
	else
	{
		GifData->LastValidPixel[Index] = OutFrame;
	}

	switch (GifFrame->mode)
	{
	case GIF_NONE:
	case GIF_CURR:
		GifData->BeforePixel[Index] = OutFrame;
		break;
	case GIF_BKGD:
		GifData->BeforePixel[Index] = GetBackground(GifFrame);
		break;
	case GIF_PREV:
		GifData->BeforePixel[Index] = GifData->LastValidPixel[Index];
		break;
	}

	return OutFrame;
}

uint32 UGifFactory::GetBackground(GIF_WHDR* GifFrame)
{
	check(GifFrame != nullptr);

	uint32 OutBackground = 0;
	if (GifFrame->tran >= 0)
	{
		OutBackground = GifFrame->tran;
	}
	else
	{
		OutBackground = GifFrame->bkgd;
	}
	return OutBackground;
}
