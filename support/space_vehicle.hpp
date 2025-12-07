#ifndef SPACE_VEHICLE_HPP_INCLUDED
#define SPACE_VEHICLE_HPP_INCLUDED

#include "simple_obj.hpp"
#include <vector>

// Structure to hold a vehicle part with its color
struct VehiclePart
{
	SimpleObjMesh mesh;
	Vec3f color;
};

// Generate a space vehicle (rocket/UFO) using primitive shapes
// Returns a vector of parts, each with its own color
// The vehicle is centered at origin, pointing up (Y+)
std::vector<VehiclePart> generate_space_vehicle();

#endif // SPACE_VEHICLE_HPP_INCLUDED

