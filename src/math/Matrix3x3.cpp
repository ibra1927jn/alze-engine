#include "Matrix3x3.h"
#include <cstdio>

namespace engine {
namespace math {

std::string Matrix3x3::toString() const {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "[%g, %g, %g]\n[%g, %g, %g]\n[%g, %g, %g]",
        get(0,0), get(0,1), get(0,2),
        get(1,0), get(1,1), get(1,2),
        get(2,0), get(2,1), get(2,2));
    return buf;
}

} // namespace math
} // namespace engine
