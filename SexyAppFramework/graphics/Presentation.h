#ifndef __PRESENTATION_H__
#define __PRESENTATION_H__

#include "misc/Rect.h"

namespace Sexy
{
// Fit the logical canvas without stretching or cropping; spare pixels form bars.
inline Rect FitPresentationRect(int theWidth, int theHeight, int theDisplayWidth, int theDisplayHeight)
{
	if (theWidth <= 0 || theHeight <= 0 || theDisplayWidth <= 0 || theDisplayHeight <= 0)
		return Rect();

	int aWidth = theDisplayWidth;
	int aHeight = theDisplayHeight;
	if (static_cast<long long>(theDisplayWidth) * theHeight > static_cast<long long>(theDisplayHeight) * theWidth)
		aWidth = std::max(1, static_cast<int>(static_cast<long long>(theDisplayHeight) * theWidth / theHeight));
	else
		aHeight = std::max(1, static_cast<int>(static_cast<long long>(theDisplayWidth) * theHeight / theWidth));
	return Rect((theDisplayWidth - aWidth) / 2, (theDisplayHeight - aHeight) / 2, aWidth, aHeight);
}
}

#endif
