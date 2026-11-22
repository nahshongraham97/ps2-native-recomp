#include "mka/retail_camera_world.hpp"
#include <type_traits>
using D=mka::retail::NativeRetailRenderCameras;
static_assert(!std::is_constructible_v<D,mka::retail::NativeRetailGameSession&>);
static_assert(!std::is_copy_constructible_v<D>);
static_assert(!std::is_copy_assignable_v<D>);
static_assert(!std::is_move_constructible_v<D>);
static_assert(!std::is_move_assignable_v<D>);
