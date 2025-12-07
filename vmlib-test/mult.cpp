#include <catch2/catch_amalgamated.hpp>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Matrix multiplication", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;
	using namespace Catch::Matchers;

	SECTION( "Identity multiplication" )
	{
		Mat44f a = { {
			1.f, 2.f, 3.f, 4.f,
			5.f, 6.f, 7.f, 8.f,
			9.f, 10.f, 11.f, 12.f,
			13.f, 14.f, 15.f, 16.f
		} };

		auto result = kIdentity44f * a;

		for (std::size_t i = 0; i < 4; ++i) {
			for (std::size_t j = 0; j < 4; ++j) {
				REQUIRE_THAT( result.v[i*4+j], WithinAbs( a.v[i*4+j], kEps_ ) );
			}
		}
	}

	SECTION( "Known multiplication" )
	{
		Mat44f a = { {
			1.f, 2.f, 3.f, 4.f,
			5.f, 6.f, 7.f, 8.f,
			9.f, 10.f, 11.f, 12.f,
			13.f, 14.f, 15.f, 16.f
		} };

		Mat44f b = { {
			2.f, 0.f, 0.f, 0.f,
			0.f, 2.f, 0.f, 0.f,
			0.f, 0.f, 2.f, 0.f,
			0.f, 0.f, 0.f, 2.f
		} };

		auto result = a * b;

		// Expected: each element of a multiplied by 2
		REQUIRE_THAT( result.v[0], WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.v[1], WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.v[2], WithinAbs( 6.f, kEps_ ) );
		REQUIRE_THAT( result.v[3], WithinAbs( 8.f, kEps_ ) );
	}
}

TEST_CASE( "Matrix-vector multiplication", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;
	using namespace Catch::Matchers;

	SECTION( "Identity transformation" )
	{
		Vec4f v = { 1.f, 2.f, 3.f, 4.f };
		auto result = kIdentity44f * v;

		REQUIRE_THAT( result.x, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 4.f, kEps_ ) );
	}

	SECTION( "Scaling transformation" )
	{
		Mat44f scale = { {
			2.f, 0.f, 0.f, 0.f,
			0.f, 3.f, 0.f, 0.f,
			0.f, 0.f, 4.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		} };

		Vec4f v = { 1.f, 1.f, 1.f, 1.f };
		auto result = scale * v;

		REQUIRE_THAT( result.x, WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}
}
