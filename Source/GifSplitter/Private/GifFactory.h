// Copyright 2020 Ryugibo, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/TextureFactory.h"
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
	static void Frame(void* data, struct GIF_WHDR* GifFrame);

	static bool ParseFrame(long IndexX, long IndexY, struct GIF_WHDR* GifFrame, uint32* OutColor);

	FORCEINLINE static uint32 GetBackground(struct GIF_WHDR* GifFrame);

	static const long INTERLACED_OFFSETS[];
	static const long INTERLACED_JUMPS[];
};
