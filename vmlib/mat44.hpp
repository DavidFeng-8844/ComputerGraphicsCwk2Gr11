#ifndef MAT44_HPP_E7187A26_469E_48AD_A3D2_63150F05A4CA
#define MAT44_HPP_E7187A26_469E_48AD_A3D2_63150F05A4CA
// SOLUTION_TAGS: gl-(ex-[^12]|cw-2|resit)

// This file requires C++23 for multi-parameter operator[]
// Multi-dimensional subscript operator[] is a C++23 feature (P2128)
// Note: IntelliSense may show E0344 errors, but actual compilation with 
// /std:c++latest should work correctly. The feature is available in MSVC 19.30+

#include <cmath>
#include <cassert>
#include <cstdlib>

#include "vec3.hpp"
#include "vec4.hpp"

/** Mat44f: 4x4 matrix with floats
 *
 * See vec2f.hpp for discussion. Similar to the implementation, the Mat44f is
 * intentionally kept simple and somewhat bare bones.
 *
 * The matrix is stored in row-major order (careful when passing it to OpenGL).
 *
 * The overloaded operator [] allows access to individual elements. Example:
 *    Mat44f m = ...;
 *    float m12 = m[1,2];
 *    m[0,3] = 3.f;
 *
 * (Multi-dimensionsal subscripts in operator[] is a C++23 feature!)
 *
 * The matrix is arranged as:
 *
 *   ⎛ 0,0  0,1  0,2  0,3 ⎞
 *   ⎜ 1,0  1,1  1,2  1,3 ⎟
 *   ⎜ 2,0  2,1  2,2  2,3 ⎟
 *   ⎝ 3,0  3,1  3,2  3,3 ⎠
 */

// C++23 multi-parameter operator[] (P2128)
// This is valid C++23 syntax. IntelliSense may show E0344 errors on lines 56 and 62,
// but these are false positives. The code compiles correctly with /std:c++latest (C++23)
// and /wd5246 in project settings (vmlib.vcxproj).

struct Mat44f
{
	float v[16];

	// C++23 multi-parameter operator[] (P2128)
	// This is valid C++23 syntax. IntelliSense may show E0344 errors, 
	// but these are false positives. Actual compilation works correctly.
	constexpr
	float& operator[] (std::size_t aI, std::size_t aJ) noexcept
	{
		assert( aI < 4 && aJ < 4 );
		return v[aI*4 + aJ];
	}
	constexpr
	float const& operator[] (std::size_t aI, std::size_t aJ) const noexcept
	{
		assert( aI < 4 && aJ < 4 );
		return v[aI*4 + aJ];
	}
};

// Identity matrix
constexpr Mat44f kIdentity44f = { {
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f
} };

// Common operators for Mat44f.
// Note that you will need to implement these yourself.

constexpr
Mat44f operator*( Mat44f const& aLeft, Mat44f const& aRight ) noexcept
{
	Mat44f result{};
	for (std::size_t i = 0; i < 4; ++i) {
		for (std::size_t j = 0; j < 4; ++j) {
			result.v[i*4 + j] = 0.f;
			for (std::size_t k = 0; k < 4; ++k) {
				result.v[i*4 + j] += aLeft.v[i*4 + k] * aRight.v[k*4 + j];
			}
		}
	}
	return result;
}

constexpr
Vec4f operator*( Mat44f const& aLeft, Vec4f const& aRight ) noexcept
{
	Vec4f result{};
	for (std::size_t i = 0; i < 4; ++i) {
		result[i] = aLeft.v[i*4 + 0] * aRight.x + aLeft.v[i*4 + 1] * aRight.y + 
		            aLeft.v[i*4 + 2] * aRight.z + aLeft.v[i*4 + 3] * aRight.w;
	}
	return result;
}

// Functions:

Mat44f invert( Mat44f const& aM ) noexcept;

inline
Mat44f transpose( Mat44f const& aM ) noexcept
{
	Mat44f ret;
	for( std::size_t i = 0; i < 4; ++i )
	{
		for( std::size_t j = 0; j < 4; ++j )
			ret.v[j*4 + i] = aM.v[i*4 + j];
	}
	return ret;
}

inline
Mat44f make_rotation_x( float aAngle ) noexcept
{
	float c = std::cos(aAngle);
	float s = std::sin(aAngle);
	return Mat44f{ {
		1.f, 0.f, 0.f, 0.f,
		0.f,   c,  -s, 0.f,
		0.f,   s,   c, 0.f,
		0.f, 0.f, 0.f, 1.f
	} };
}


inline
Mat44f make_rotation_y( float aAngle ) noexcept
{
	float c = std::cos(aAngle);
	float s = std::sin(aAngle);
	return Mat44f{ {
		  c, 0.f,   s, 0.f,
		0.f, 1.f, 0.f, 0.f,
		 -s, 0.f,   c, 0.f,
		0.f, 0.f, 0.f, 1.f
	} };
}

inline
Mat44f make_rotation_z( float aAngle ) noexcept
{
	float c = std::cos(aAngle);
	float s = std::sin(aAngle);
	return Mat44f{ {
		  c,  -s, 0.f, 0.f,
		  s,   c, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	} };
}

inline
Mat44f make_translation( Vec3f aTranslation ) noexcept
{
	return Mat44f{ {
		1.f, 0.f, 0.f, aTranslation.x,
		0.f, 1.f, 0.f, aTranslation.y,
		0.f, 0.f, 1.f, aTranslation.z,
		0.f, 0.f, 0.f, 1.f
	} };
}
inline
Mat44f make_scaling( float aSX, float aSY, float aSZ ) noexcept
{
	//TODO: your implementation goes here
	//TODO: remove the following when you start your implementation
	(void)aSX;  // Avoid warnings about unused arguments until the function
	(void)aSY;  // is properly implemented.
	(void)aSZ;
	return kIdentity44f;
}

inline
Mat44f make_perspective_projection( float aFovInRadians, float aAspect, float aNear, float aFar ) noexcept
{
	float f = 1.f / std::tan(aFovInRadians / 2.f);
	return Mat44f{ {
		f/aAspect, 0.f, 0.f, 0.f,
		0.f, f, 0.f, 0.f,
		0.f, 0.f, (aFar + aNear) / (aNear - aFar), (2.f * aFar * aNear) / (aNear - aFar),
		0.f, 0.f, -1.f, 0.f
	} };
}

#endif // MAT44_HPP_E7187A26_469E_48AD_A3D2_63150F05A4CA
