#include <catch2/catch_amalgamated.hpp>

#include <numbers>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Rotation matrices", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;
	using namespace Catch::Matchers;

	SECTION( "Rotation X - 90 degrees" )
	{
		auto rot = make_rotation_x(std::numbers::pi_v<float> / 2.f);

		Vec4f v = { 0.f, 1.f, 0.f, 1.f };
		auto result = rot * v;

		REQUIRE_THAT( result.x, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation Y - 90 degrees" )
	{
		auto rot = make_rotation_y(std::numbers::pi_v<float> / 2.f);

		Vec4f v = { 1.f, 0.f, 0.f, 1.f };
		auto result = rot * v;

		REQUIRE_THAT( result.x, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( -1.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation Z - 90 degrees" )
	{
		auto rot = make_rotation_z(std::numbers::pi_v<float> / 2.f);

		Vec4f v = { 1.f, 0.f, 0.f, 1.f };
		auto result = rot * v;

		REQUIRE_THAT( result.x, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation X - Identity at 0 degrees" )
	{
		auto rot = make_rotation_x(0.f);

		for (std::size_t i = 0; i < 4; ++i) {
			for (std::size_t j = 0; j < 4; ++j) {
				REQUIRE_THAT( rot.v[i*4+j], WithinAbs( kIdentity44f.v[i*4+j], kEps_ ) );
			}
		}
	}
}
