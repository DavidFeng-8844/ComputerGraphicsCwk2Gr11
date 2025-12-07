#include <catch2/catch_amalgamated.hpp>

#include <numbers>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Perspective projection", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;

	using namespace Catch::Matchers;

	// "Standard" projection matrix presented in the exercises. Assumes
	// standard window size (e.g., 1280x720).
	//
	// Field of view (FOV) = 60 degrees
	// Window size is 1280x720 and we defined the aspect ratio as w/h
	// Near plane at 0.1 and far at 100
	SECTION( "Standard" )
	{
		auto const proj = make_perspective_projection(
			60.f * std::numbers::pi_v<float> / 180.f,
			1280/float(720),
			0.1f, 100.f
		);

		REQUIRE_THAT( proj.v[0], WithinAbs( 0.974279, kEps_ ) );
		REQUIRE_THAT( proj.v[1], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[2], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[3], WithinAbs( 0.f, kEps_ ) );

		REQUIRE_THAT( proj.v[4], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[5], WithinAbs( 1.732051f, kEps_ ) );
		REQUIRE_THAT( proj.v[6], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[7], WithinAbs( 0.f, kEps_ ) );

		REQUIRE_THAT( proj.v[8], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[9], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[10], WithinAbs( -1.002002f, kEps_ ) );
		REQUIRE_THAT( proj.v[11], WithinAbs( -0.200200f, kEps_ ) );

		REQUIRE_THAT( proj.v[12], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[13], WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( proj.v[14], WithinAbs( -1.f, kEps_ ) );
		REQUIRE_THAT( proj.v[15], WithinAbs( 0.f, kEps_ ) );
	}
}
