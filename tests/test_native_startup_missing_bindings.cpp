#include "native_retail_startup.hpp"
// Link-only negative fixture. No success stand-ins for missing production
// initialization or game continuation are supplied and this is never executed.
auto* volatile required_native_entry=&mka::retail::run_native_retail;
int main(){return required_native_entry?0:1;}
