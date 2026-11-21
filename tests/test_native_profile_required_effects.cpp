#include "mka/retail_profile_aggregate_main.hpp"
#include "mka/retail_profile_resource_ids.hpp"

// Link-only fixture; never executed. Required native children must prevent
// these real algorithms from linking without their actual implementations.
auto* volatile required_aggregate = &mka::retail::refresh_retail_profile_aggregate_from_2432a0;
auto volatile required_ids = &mka::retail::NativeRetailProfileResourceIds::record_from_446f80;
int main() { return required_aggregate && required_ids ? 0 : 1; }
