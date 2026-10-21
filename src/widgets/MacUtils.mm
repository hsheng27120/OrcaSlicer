#include "MacUtils.hpp"

#if __APPLE__
#import <AppKit/AppKit.h>

#include <algorithm>

namespace Slic3r {
namespace GUI {

double mac_max_scaling_factor()
{
    double scaling = 1.;
    if ([NSScreen screens] == nil) {
        scaling = [[NSScreen mainScreen] backingScaleFactor];
    } else {
	    for (int i = 0; i < [[NSScreen screens] count]; ++ i)
	    	scaling = std::max<double>(scaling, [[[NSScreen screens] objectAtIndex:0] backingScaleFactor]);
	}
    return scaling;
}

}
}
#endif // __APPLE__
