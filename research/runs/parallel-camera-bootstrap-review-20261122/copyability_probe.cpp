#include "mka/retail_camera_world.hpp"
#include <type_traits>
using Domain=mka::retail::NativeRetailRenderCameras;
static_assert(std::is_copy_constructible_v<Domain>);
static_assert(std::is_move_constructible_v<Domain>);
// Both operations are public and retain the borrowed raw Session pointer.
