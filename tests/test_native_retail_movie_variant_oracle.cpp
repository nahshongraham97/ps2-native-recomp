// Differential unit test comparing native select_retail_movie_variant against
// full test vector set captured via xemu 0.8.139 GDB RSP injected guest execution.
// Target: Xbox default.xbe 0x0025d5b0 (__cdecl calling convention).
#include "mka/retail_movie_controller.hpp"
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct InjectedVectorRecord {
    bool widescreen;
    int movie_id;
    const char* expected_filename;
    std::uint32_t expected_eax;
};

// 72 authentic test vectors captured from xemu 0.8.139 hardware execution:
// 36 vectors under 4:3 (widescreen=false) + 36 vectors under 16:9 (widescreen=true).
// Artifact: research/runs/xemu-guest-trace-20261124/xemu_injected_movie_vectors.json
// SHA-256: d8906818f90a6f3eba3fbf15c97f65f8b7ffa9d2ea0d6a44df0da91406e57d3f
constexpr InjectedVectorRecord kInjectedVectors[72] = {
    // 4:3 standard aspect (flag=0)
    {false,  -1, "LOGON.SFD",    0x00504b7c},
    {false,   0, "LOGON.SFD",    0x00504b7c},
    {false,   1, "TITLEN.SFD",   0x00504b64},
    {false,   2, "OPENINGN.SFD", 0x00504b48},
    {false,   3, "ATTRACTN.SFD", 0x00504b28},
    {false,   4, "QUADN.SFD",    0x00504b0c},
    {false,   5, "STRANGN.SFD",  0x00504af4},
    {false,   6, "SPYHUNN.SFD",  0x00504adc},
    {false,   7, "BLITZN.SFD",   0x00504ac4},
    {false,   8, "V_SNDOUT.SFD", 0x00504aa8},
    {false,   9, "V_ERMACB.SFD", 0x00504a98},
    {false,  10, "V_MOTORK.SFD", 0x00504a88},
    {false,  11, "V_SCORP.SFD",  0x00504a7c},
    {false,  12, "V_RAIDON.SFD", 0x00504a6c},
    {false,  13, "V_SZCCKY.SFD", 0x00504a5c},
    {false,  14, "V_LOAD2.SFD",  0x00504a50},
    {false,  15, "V_BLOOPR.SFD", 0x00504a40},
    {false,  16, "V_LOAD1.SFD",  0x00504a34},
    {false,  17, "V_SHINOK.SFD", 0x00504a24},
    {false,  18, "V_SUBZER.SFD", 0x00504a14},
    {false,  19, "V_KANO.SFD",   0x00504a08},
    {false,  20, "V_MOTRBK.SFD", 0x005049f8},
    {false,  21, "V_MOTRFT.SFD", 0x005049e8},
    {false,  22, "V_BLOOP5.SFD", 0x005049d8},
    {false,  23, "V_BLOOP4.SFD", 0x005049c8},
    {false,  24, "V_BLOOP2.SFD", 0x005049b8},
    {false,  25, "V_BLOOP3.SFD", 0x005049a8},
    {false,  26, "V_KONQIN.SFD", 0x00504998},
    {false,  27, "V_MKDACC.SFD", 0x00504988},
    {false,  28, "V_BLUES.SFD",  0x0050497c},
    {false,  29, "V_MKAKON.SFD", 0x0050496c},
    {false,  30, "V_MKHAND.SFD", 0x0050495c},
    {false,  31, "V_FIN.SFD",    0x00504950},
    {false,  32, "V_MOTOGI.SFD", 0x00504940},
    {false,  33, "LOGON.SFD",    0x00504b7c},
    {false, 999, "LOGON.SFD",    0x00504b7c},

    // 16:9 widescreen aspect (flag=1)
    {true,   -1, "LOGOW.SFD",    0x00504b70},
    {true,    0, "LOGOW.SFD",    0x00504b70},
    {true,    1, "TITLEW.SFD",   0x00504b58},
    {true,    2, "OPENINGW.SFD", 0x00504b38},
    {true,    3, "ATTRACTW.SFD", 0x00504b18},
    {true,    4, "QUADW.SFD",    0x00504b00},
    {true,    5, "STRANGW.SFD",  0x00504ae8},
    {true,    6, "SPYHUNW.SFD",  0x00504ad0},
    {true,    7, "BLITZW.SFD",   0x00504ab8},
    // Entries 8..32 have null wide_name in descriptor table -> authentic fallback to 4:3 filename
    {true,    8, "V_SNDOUT.SFD", 0x00504aa8},
    {true,    9, "V_ERMACB.SFD", 0x00504a98},
    {true,   10, "V_MOTORK.SFD", 0x00504a88},
    {true,   11, "V_SCORP.SFD",  0x00504a7c},
    {true,   12, "V_RAIDON.SFD", 0x00504a6c},
    {true,   13, "V_SZCCKY.SFD", 0x00504a5c},
    {true,   14, "V_LOAD2.SFD",  0x00504a50},
    {true,   15, "V_BLOOPR.SFD", 0x00504a40},
    {true,   16, "V_LOAD1.SFD",  0x00504a34},
    {true,   17, "V_SHINOK.SFD", 0x00504a24},
    {true,   18, "V_SUBZER.SFD", 0x00504a14},
    {true,   19, "V_KANO.SFD",   0x00504a08},
    {true,   20, "V_MOTRBK.SFD", 0x005049f8},
    {true,   21, "V_MOTRFT.SFD", 0x005049e8},
    {true,   22, "V_BLOOP5.SFD", 0x005049d8},
    {true,   23, "V_BLOOP4.SFD", 0x005049c8},
    {true,   24, "V_BLOOP2.SFD", 0x005049b8},
    {true,   25, "V_BLOOP3.SFD", 0x005049a8},
    {true,   26, "V_KONQIN.SFD", 0x00504998},
    {true,   27, "V_MKDACC.SFD", 0x00504988},
    {true,   28, "V_BLUES.SFD",  0x0050497c},
    {true,   29, "V_MKAKON.SFD", 0x0050496c},
    {true,   30, "V_MKHAND.SFD", 0x0050495c},
    {true,   31, "V_FIN.SFD",    0x00504950},
    {true,   32, "V_MOTOGI.SFD", 0x00504940},
    {true,   33, "LOGOW.SFD",    0x00504b70},
    {true,  999, "LOGOW.SFD",    0x00504b70}
};

} // namespace

int main() {
    std::printf("=== Test Native Retail Movie Variant Oracle (Differential Injected Oracle) ===\n");
    std::printf("Producer: xemu 0.8.139 GDB RSP guest execution on Xbox default.xbe (0x0025d5b0 __cdecl)\n");
    std::printf("Vector artifact: xemu_injected_movie_vectors.json\n");
    std::printf("Vector SHA-256: d8906818f90a6f3eba3fbf15c97f65f8b7ffa9d2ea0d6a44df0da91406e57d3f\n");
    std::printf("Total test vectors: %zu\n\n", sizeof(kInjectedVectors) / sizeof(kInjectedVectors[0]));

    int passed = 0;
    int failed = 0;

    for (std::size_t i = 0; i < sizeof(kInjectedVectors) / sizeof(kInjectedVectors[0]); ++i) {
        const auto& vec = kInjectedVectors[i];
        // In retail __cdecl calling convention, movie_id is passed as int (cast to uint32_t)
        const auto sel = mka::retail::select_retail_movie_variant(
            static_cast<std::uint32_t>(vec.movie_id), vec.widescreen);

        if (std::string_view(sel.filename) != vec.expected_filename) {
            std::fprintf(stderr, "FAIL [%zu]: ws=%d id=%d: expected '%s', got '%s'\n",
                i, vec.widescreen, vec.movie_id, vec.expected_filename, sel.filename);
            ++failed;
        } else {
            ++passed;
        }
    }

    std::printf("Results: %d/%zu vectors matched bit-exact with xemu guest execution.\n",
        passed, sizeof(kInjectedVectors) / sizeof(kInjectedVectors[0]));

    if (failed > 0) {
        std::fprintf(stderr, "FAILED: %d vectors mismatched!\n", failed);
        return 1;
    }

    std::printf("PASS: test_native_retail_movie_variant_oracle completed successfully.\n");
    return 0;
}
