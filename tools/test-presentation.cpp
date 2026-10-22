#include "graphics/Presentation.h"
#include <cassert>
#include <cstdio>

int main()
{
	using Sexy::FitPresentationRect;
	auto aWide = FitPresentationRect(800, 600, 1920, 1080);
	assert(aWide.mX == 240 && aWide.mY == 0 && aWide.mWidth == 1440 && aWide.mHeight == 1080);
	auto aTall = FitPresentationRect(800, 600, 1280, 1024);
	assert(aTall.mX == 0 && aTall.mY == 32 && aTall.mWidth == 1280 && aTall.mHeight == 960);
	auto aNative = FitPresentationRect(800, 600, 800, 600);
	assert(aNative.mX == 0 && aNative.mY == 0 && aNative.mWidth == 800 && aNative.mHeight == 600);

	const int aSizes[][2] = {
		{1920, 1080}, {2560, 1440}, {3840, 2160}, {3440, 1440},
		{1280, 1024}, {1024, 768}, {600, 800}, {1919, 1079}, {640, 480}
	};
	for (const auto& aSize : aSizes)
	{
		auto aRect = FitPresentationRect(800, 600, aSize[0], aSize[1]);
		assert(aRect.mWidth > 0 && aRect.mHeight > 0);
		assert(aRect.mX >= 0 && aRect.mY >= 0);
		assert(aRect.mX + aRect.mWidth <= aSize[0] && aRect.mY + aRect.mHeight <= aSize[1]);
		assert(aSize[0] - aRect.mWidth - 2 * aRect.mX <= 1);
		assert(aSize[1] - aRect.mHeight - 2 * aRect.mY <= 1);
		// Aspect error is bounded by integer pixel rounding.
		assert(std::abs(aRect.mWidth * 600 - aRect.mHeight * 800) < 800);
		assert(aRect.mWidth == aSize[0] || aRect.mHeight == aSize[1]);
	}
	assert(FitPresentationRect(800, 600, 0, 1080).mWidth == 0);
	assert(FitPresentationRect(800, 0, 1920, 1080).mHeight == 0);
	assert(FitPresentationRect(800, 600, 1, 1).mWidth == 1);
	std::puts("PASS: presentation fits 4:3, wide, ultrawide, tall, small and odd-sized displays");
}
