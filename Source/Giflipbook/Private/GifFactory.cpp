// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#include "GifFactory.h"

#include "Giflipbook.h"

#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ObjectTools.h"
#include "PackageTools.h"

struct FrameData
{
	TArray<uint8> Frame;
	long Width;
	long Height;
};

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
	TArray<FrameData> GifData;
	UTexture2D* Texture = nullptr;
	GIF_Load((void*)Buffer, BufferEnd - Buffer, UGifFactory::Frame, 0, (void*)&GifData, 0L);

	for (int i = 0; i < GifData.Num(); ++i) {
		FString FileName = FString::Printf(TEXT("%s_%d"), *Name.ToString(), i);
		UTexture2D* CreatedTexture = CreateTextureFromRawData(InParent, *FileName, Flags, Context, Warn, GifData[i].Frame, GifData[i].Width, GifData[i].Height);
		if (CreatedTexture == nullptr) {
			return nullptr;
		}
	}
	return nullptr;
}


void UGifFactory::Frame(void* RawData, GIF_WHDR* GifFrame)
{
	TArray<FrameData>* GifData = (TArray<FrameData>*)RawData;

	FrameData CurrentFrame;
	CurrentFrame.Width = GifFrame->xdim;
	CurrentFrame.Height = GifFrame->ydim;
	long size = CurrentFrame.Width * CurrentFrame.Height;
	CurrentFrame.Frame.AddUninitialized(size * 4);

	// TODO: Support interlace
	//GifFrame->intr

	// TODO; Support mode
	//GifFrame->mode

	UE_LOG(LogGiflipbook, Log, TEXT("VVVVVVVVVVVVVVVVVVVVVVVV"));

	for (int i = 0; i < size; ++i)
	{
		bool bIsTransparent = GifFrame->tran == GifFrame->bptr[i];
		CurrentFrame.Frame[i * 4 + 0] = GifFrame->cpal[GifFrame->bptr[i]].B;
		CurrentFrame.Frame[i * 4 + 1] = GifFrame->cpal[GifFrame->bptr[i]].G;
		CurrentFrame.Frame[i * 4 + 2] = GifFrame->cpal[GifFrame->bptr[i]].R;
		CurrentFrame.Frame[i * 4 + 3] = bIsTransparent ? 0 : 255;
	}

	UE_LOG(LogGiflipbook, Log, TEXT("AAAAAAAAAAAAAAAAAAAAAAAA"));

	GifData->Add(CurrentFrame);
}

UTexture2D* UGifFactory::CreateTextureFromRawData
(
	UObject*				InParent,
	FName					Name,
	EObjectFlags			Flags,
	UObject*				Context,
	FFeedbackContext*		Warn,
	const TArray<uint8>&	InRawData,
	const long&				Width,
	const long&				Height
)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	TSharedPtr<IImageWrapper> BmpImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP);
	if (!BmpImageWrapper.IsValid())
	{
		return nullptr;
	}
	if (!BmpImageWrapper->SetRaw(InRawData.GetData(), InRawData.Num(), Width, Height, ERGBFormat::BGRA, 8))
	{
		return nullptr;
	}
	// Check the resolution of the imported texture to ensure validity
	//if (!IsImportResolutionValid(BmpImageWrapper->GetWidth(), BmpImageWrapper->GetHeight(), /*bAllowNonPowerOfTwo*/ true, Warn))
	//{
	//	return nullptr;
	//}


	FString TextureName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("T_%s"), *Name.ToString()));

	FString BasePackageName = FPackageName::GetLongPackagePath(InParent->GetName()) / TextureName;
	BasePackageName = PackageTools::SanitizePackageName(BasePackageName);


	FString ObjectPath = BasePackageName + TEXT(".") + TextureName;
	UTexture* ExistingTexture = LoadObject<UTexture>(NULL, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);

	UPackage* TexturePackage = nullptr;
	if (ExistingTexture == nullptr)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString FinalPackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), FinalPackageName, TextureName);

		TexturePackage = CreatePackage(NULL, *FinalPackageName);
	}
	else
	{
		TexturePackage = ExistingTexture->GetOutermost();
	}

	UTexture2D* Texture = CreateTexture2D(TexturePackage, *TextureName, Flags);
	if (Texture)
	{
		// Set texture properties.
		Texture->Source.Init(
			BmpImageWrapper->GetWidth(),
			BmpImageWrapper->GetHeight(),
			/*NumSlices=*/ 1,
			/*NumMips=*/ 1,
			TSF_BGRA8
		);
		TArray<uint8> RawBMP;
		if (BmpImageWrapper->GetRaw(BmpImageWrapper->GetFormat(), BmpImageWrapper->GetBitDepth(), RawBMP))
		{
			uint8* MipData = Texture->Source.LockMip(0);
			FMemory::Memcpy(MipData, RawBMP.GetData(), RawBMP.Num());
			Texture->Source.UnlockMip(0);
		}
		TexturePackage->SetDirtyFlag(true);
		CleanUp();
	}
	FAssetRegistryModule::AssetCreated(Texture);
	Texture->MarkPackageDirty();
	Texture->PostEditChange();

	return Texture;
}

#undef LOCTEXT_NAMESPACE
