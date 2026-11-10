#include "UI/CreatorColorWheel.h"
#include "Widgets/Colors/SColorWheel.h"

void UCreatorColorWheel::SetHSV(const FLinearColor& InHSV)
{
	HSV = InHSV;
}

void UCreatorColorWheel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Wheel.Reset();
}

TSharedRef<SWidget> UCreatorColorWheel::RebuildWidget()
{
	TWeakObjectPtr<UCreatorColorWheel> WeakThis(this);
	Wheel = SNew(SColorWheel)
		.SelectedColor_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->HSV : FLinearColor::White; })
		.OnValueChanged_Lambda([WeakThis](FLinearColor NewHSV)
		{
			if (UCreatorColorWheel* Self = WeakThis.Get())
			{
				Self->HSV = NewHSV;
				if (Self->OnChanged)
				{
					Self->OnChanged(NewHSV, false);
				}
			}
		})
		.OnMouseCaptureEnd_Lambda([WeakThis]()
		{
			if (UCreatorColorWheel* Self = WeakThis.Get())
			{
				if (Self->OnChanged)
				{
					Self->OnChanged(Self->HSV, true);
				}
			}
		});
	return Wheel.ToSharedRef();
}
