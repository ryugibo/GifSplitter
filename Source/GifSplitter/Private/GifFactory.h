// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Factories/TextureFactory.h"

#include "gif_load/gif_load.h"

#include "GifFactory.generated.h"

/**
 *
 */
UCLASS()
class UGifFactory : public UTextureFactory
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, class FFeedbackContext* Warn) override;
	//~ End UFactory Interface

private:
	class UTexture2D* CreateTextureFromRawData(UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, class FFeedbackContext* Warn, const TArray<uint8>& InRawData, const long& InWidth, const long& InHeight);

	static void Frame(void* data, struct GIF_WHDR* whdr);
};
