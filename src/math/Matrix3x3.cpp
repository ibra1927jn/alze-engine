#include "Matrix3x3.h"

namespace engine {
namespace math {

std::ostream& operator<<(std::ostream& os, const Matrix3x3& mat) {
    os << "[" << mat.get(0,0) << ", " << mat.get(0,1) << ", " << mat.get(0,2) << "]\n"
       << "[" << mat.get(1,0) << ", " << mat.get(1,1) << ", " << mat.get(1,2) << "]\n"
       << "[" << mat.get(2,0) << ", " << mat.get(2,1) << ", " << mat.get(2,2) << "]";
    return os;
}

} // namespace math
} // namespace engine
