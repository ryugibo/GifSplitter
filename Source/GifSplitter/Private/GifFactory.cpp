// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#include "GifFactory.h"
#include "GifSplitter.h"
#include "AssetRegistryModule.h"
#include "Factories.h"
#include "Serialization/BufferArchive.h"
#include "gif_load/gif_load.h"


struct FGifData
{
	TArray<uint32> BeforePixel;
	TArray<uint32> LastValidPixel;
	TArray<TArray<uint8>> Buffer;
};

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
	FGifData GifData;
	GIF_Load((void*)Buffer, BufferEnd - Buffer, UGifFactory::Frame, 0, (void*)&GifData, 0L);

	UTexture2D* LastObject = nullptr;
	for (int i = 0; i < GifData.Buffer.Num(); ++i) {
		const TArray<uint8>& FrameBuffer = GifData.Buffer[i];
		const uint8* PtrFrameBuffer = FrameBuffer.GetData();
		const uint8* PtrFrameBufferEnd = PtrFrameBuffer + FrameBuffer.Num();

		FString FileName = FString::Printf(TEXT("%s_%d"), *Name.ToString(), i);
		LastObject = Cast<UTexture2D>(Super::FactoryCreateBinary(Class, InParent, *FileName, Flags, Context, Type, PtrFrameBuffer, PtrFrameBufferEnd, Warn));
		if (LastObject == nullptr)
		{
			UE_LOG(LogGifSplitter, Error, TEXT("Failed import gif frame %d / %d"), i + 1, GifData.Buffer.Num());
			break;
		}
		if (i < (GifData.Buffer.Num() - 1))
		{
			LastObject->PostEditChange();
			LastObject->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(LastObject);
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

	if (GifFrame->intr == 0)
	{
		for (long yd_i = 0; yd_i < GifFrame->ydim; ++yd_i)
		{
			for (long xd_i = 0; xd_i < GifFrame->xdim; ++xd_i)
			{
				long Index = xd_i + (yd_i * GifFrame->xdim);
				uint32 Frame;
				if (ParseFrame(xd_i, yd_i, GifFrame, &Frame))
				{
					GifData->LastValidPixel[Index] = Frame;
				}
				else
				{
					Frame = GifData->BeforePixel[Index];
				}

				switch (GifFrame->mode)
				{
				case GIF_NONE:
				case GIF_CURR:
					GifData->BeforePixel[Index] = Frame;
					break;
				case GIF_BKGD:
					GifData->BeforePixel[Index] = GetBackground(GifFrame);
					break;
				case GIF_PREV:
					GifData->BeforePixel[Index] = GifData->LastValidPixel[Index];
					break;
				}

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
					long Index = xd_i + (yd_i * GifFrame->xdim);
					uint32 Frame = 0;
					if (ParseFrame(xd_i, yd_i, GifFrame, &Frame))
					{
						GifData->LastValidPixel[Index] = Frame;
					}
					else
					{
						Frame = GifData->BeforePixel[Index];
					}

					switch (GifFrame->mode)
					{
					case GIF_NONE:
					case GIF_CURR:
						GifData->BeforePixel[Index] = Frame;
						break;
					case GIF_BKGD:
						GifData->BeforePixel[Index] = GetBackground(GifFrame);
						break;
					case GIF_PREV:
						GifData->BeforePixel[Index] = GifData->LastValidPixel[Index];
						break;
					}

					Buffer << Frame;
				}
			}
		}
	}

	GifData->Buffer.Add(Buffer);
}

bool UGifFactory::ParseFrame(long IndexX, long IndexY, GIF_WHDR* GifFrame, uint32* OutColor)
{
	check(GifFrame != nullptr);
	check(OutColor != nullptr);

	uint32 Frame = 0;
	if (FMath::IsWithin(IndexX, GifFrame->frxo, GifFrame->frxd) &&
		FMath::IsWithin(IndexY, GifFrame->fryo, GifFrame->fryd))
	{
		long FrameIndex = (IndexX - GifFrame->frxo) + ((IndexY - GifFrame->fryo) * GifFrame->frxd);
		if (GifFrame->tran != GifFrame->bptr[FrameIndex])
		{
			// RRGGBBAA
			*OutColor = (GifFrame->cpal[GifFrame->bptr[FrameIndex]].R << 24) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].G << 16) | (GifFrame->cpal[GifFrame->bptr[FrameIndex]].B << 8) | 0xFF;
			return true;
		}
	}
	return false;
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
