#include "mka/retail_initial_task.hpp"

int main() {
    // Keep an actual relocation to the production factory. Merely checking a
    // nonvolatile known function address could be optimized out and fail to
    // pull the real parent code and its required child symbols from the archive.
    auto volatile factory = &mka::retail::NativeRetailInitialTaskBody::create;
    return factory == nullptr ? 1 : 0;
}
