// Copyright 2018 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AutoSizeSettings.generated.h"


UCLASS(config = AutoSizeComments, defaultconfig)
class AUTOSIZECOMMENTS_API UAutoSizeSettings : public UObject
{
	GENERATED_BODY()
		
public:
	UAutoSizeSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, config, Category = Default)
	FVector2D CommentNodePadding;

	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bUseRandomColor;

	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor DefaultCommentColor;

	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bRunCollisionSolver;

	UPROPERTY(EditAnywhere, config, Category = Default)
	float CollisionMovementSpeed;
};
