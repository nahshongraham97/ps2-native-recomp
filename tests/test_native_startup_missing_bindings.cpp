#include "native_retail_startup.hpp"
// Link-only negative fixture. Production CRT/platform/main control are linked;
// unresolved actual subsystem bodies must still reject the full entry. No
// success stand-ins are supplied, and this program is never executed.
auto* volatile required_native_entry=&mka::retail::run_native_retail;
int main(){return required_native_entry?0:1;}
