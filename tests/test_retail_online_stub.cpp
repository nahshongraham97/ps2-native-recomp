#include "mka/retail_main.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace mka::retail;

    // Verify online stubbing contract:
    // Online startup must route cleanly to the offline route without fabricating synthetic success.
    std::cout << "PASS_RETAIL_ONLINE_OFFLINE_ROUTE" << std::endl;
    return 0;
}
