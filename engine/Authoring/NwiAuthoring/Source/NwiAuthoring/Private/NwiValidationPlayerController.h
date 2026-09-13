// Test-only player controller with explicit locality for deterministic listen-host validation.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NwiValidationPlayerController.generated.h"

UCLASS(Transient)
class ANwiValidationPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    bool bValidationLocal = false;

    virtual bool IsLocalController() const override
    {
        return bValidationLocal;
    }
};
