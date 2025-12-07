#include <catch2/catch_amalgamated.hpp>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Translation matrix", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;
	using namespace Catch::Matchers;

	SECTION( "Basic translation" )
	{
		auto trans = make_translation(Vec3f{ 5.f, 10.f, 15.f });

		Vec4f v = { 1.f, 2.f, 3.f, 1.f };
		auto result = trans * v;

		REQUIRE_THAT( result.x, WithinAbs( 6.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 12.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 18.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Zero translation" )
	{
		auto trans = make_translation(Vec3f{ 0.f, 0.f, 0.f });

		for (std::size_t i = 0; i < 4; ++i) {
			for (std::size_t j = 0; j < 4; ++j) {
				REQUIRE_THAT( trans.v[i*4+j], WithinAbs( kIdentity44f.v[i*4+j], kEps_ ) );
			}
		}
	}

	SECTION( "Negative translation" )
	{
		auto trans = make_translation(Vec3f{ -3.f, -4.f, -5.f });

		Vec4f v = { 10.f, 20.f, 30.f, 1.f };
		auto result = trans * v;

		REQUIRE_THAT( result.x, WithinAbs( 7.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 16.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 25.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}
}
