#include "mfloat_internal.h"
#include "mfloat_coeff_tables.h"
#include "internal/mint_support.h"

#include <limits.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MFLOAT_CONST_LITERAL_BITS 256u
#define MFLOAT_CONST_GUARD_BITS   32u
#define MFLOAT_QFLOAT_EFFECTIVE_BITS 106u
#define MFLOAT_GAMMA_MIN_SHIFT    12u
#define MFLOAT_GAMMA_MAX_SHIFT    18u
#define MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT 40u

typedef struct mfloat_asymp_term_t {
    long num;
    long den;
    int sign;
    long mult;
} mfloat_asymp_term_t;

/* Stack-backed scratch slots avoid hot-path mfloat container allocation. */
typedef struct mfloat_scratch_slot_t {
    mfloat_t value;
    mint_t mantissa;
} mfloat_scratch_slot_t;

static uint64_t mfloat_half_ln_pi1024_storage[] = {
    0x0104443541acf8fdu, 0x8acceacf3466b23fu, 0xb13d5fd4522f4415u, 0xc2bd83bb87a32c13u,
    0x6e134b0233c534f9u, 0x68779816e22f6412u, 0x2a6759cb50729287u, 0x4f0695a10423f48cu,
    0x7773a09a45ed9931u, 0x3d84836a42009d15u, 0xab35e0f741fe6861u, 0xc8b3fb0864b9b2b6u,
    0x5244b33ecc757d13u, 0x2bee63bc9752142bu, 0xad0e65de19f3167bu, 0x6c15e911db12f473u,
    0x9e5aca6d3a8b3235u, 0xadac4e8cc84ea4d0u, 0xdb9e33b006c6473au, 0xbe4e8881c0aa15b4u,
    0x7f12928d63403528u, 0x3976495c0ef97ea8u, 0xb2d8251426f00b93u, 0xbe95cd57e0d39c6fu,
    0xc4d128c5e65b6740u, 0xb477fcc702f86507u, 0x326e6eeed3d33b9du, 0x170cce71550f0eacu,
    0x2c1cda480584adb6u, 0x2611cb54d89bea46u, 0x4c0dc67005d95df5u, 0xe9330bcdbb7e767eu,
    0x7a17abf2ad8d5087u, 0x000024a1a091cf43u
};
static uint64_t mfloat_half_ln_2pi1024_storage[] = {
    0xdc4e73e87fc1eb55u, 0xbf9dee51cea2bfa8u, 0xf9142d0ad1efe862u, 0xa4ab4673adcf059eu,
    0x010fb7b85f75bea1u, 0xa7a001e5731f5e38u, 0xaf99faeceddbbf4au, 0xd72a1f85077464b2u,
    0xc0ca1c6012efd723u, 0x92519308be8162e8u, 0xb97df6a00074f2b6u, 0xb5de6f4e7b3f8973u,
    0xc1e9ca44b4ab4b6cu, 0x65e335a6245eb8a1u, 0x601531a6693b6cf7u, 0x6110e4bf390d8f5fu,
    0x1dccd68ee0aa4435u, 0xa8650f8b6190e43cu, 0xf8128ef5b9bee922u, 0xc497952db4dafb98u,
    0x282bf1495817dc2eu, 0x385fb7b5dfefb255u, 0x7edf89c21b0d9a5eu, 0x45a5a495344c1296u,
    0x21e9cc1f619d8d70u, 0xeb7ca499dc3b2951u, 0x3696f3d9bb385dcau, 0x5a6eec172ad5c6d3u,
    0xa1afe4faafe41715u, 0x8557484be75ff803u, 0x62d377b1a8c4cf6au, 0x4808f3ec23e344d1u,
    0x694d252f24005106u, 0x00003acfe390c97du
};

static struct _mint_t mfloat_half_ln_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_half_ln_pi1024_storage };
static struct _mint_t mfloat_half_ln_2pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_half_ln_2pi1024_storage };

static struct _mfloat_t mfloat_half_ln_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .immortal = true, .mantissa = &mfloat_half_ln_pi1024_mint };
static struct _mfloat_t mfloat_half_ln_2pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .immortal = true, .mantissa = &mfloat_half_ln_2pi1024_mint };

static uint64_t mfloat_erf_half1024_storage[] = {
    0x87161b412a55c3abu, 0x4c1ff8ea000648f4u, 0x741f953df347a052u, 0x5deb1ae019d33f11u,
    0xc17cad0c73d514aeu, 0x95f0b20f67e7d79au, 0x8b28a77b510204f3u, 0xfcbc17670e70397cu,
    0xfaff42807dd93532u, 0xd212ac5ddb50e8b9u, 0x8a3f383f53eabb74u, 0x3b401ac295b83ee6u,
    0x583ffd36bba9677du, 0x874615a4ef6ae62bu, 0x62e9007e0f5ea0aau, 0x61fbbb8dc502286au,
    0x9590c834259b7af2u, 0x2d6c0d1bddca92b2u, 0x3ad8ac028f6c517au, 0x7eecdc46b98c1784u,
    0xec8c9ccde476f052u, 0x80785f4a543809edu, 0xc37d35784c6d9ae1u, 0x20856dec4ee57393u,
    0xe809f1a31a27a94cu, 0x0853f7ae0c76e915u
};
static uint64_t mfloat_erfinv_half1024_storage[] = {
    0x10a390918d219ee9u, 0x7d62ce2513aa37dbu, 0xe0e56c47b22c17dcu, 0x95afaa512dec8f46u,
    0x83263f151663a152u, 0x7d9653744c5753c4u, 0xf8fd88430d760ac6u, 0x5ff19067db19c420u,
    0xda4951d5dacfa186u, 0x448e861d096cfeebu, 0xf0122a299f0af25au, 0xc2b3941566634b6du,
    0x3ea3d760675e6f6cu, 0xefeb120a8dd03a01u, 0x43c3194aac49da8fu, 0xb9094edbacb78e49u,
    0x375fdfc4ce25b46cu, 0xa27225bde6c89b78u, 0x9a87fb6914feddf7u, 0x4df387458b103b43u,
    0x998dd1b84cb13fcfu, 0x60182ab4cd4b10b8u, 0xb18aa9bc649686d8u, 0x7c5015058d557398u,
    0x73533cdca0fbc883u, 0x03d0c3f76498013du
};
static uint64_t mfloat_tetragamma1_1024_storage[] = {
    0xa59d40f261b7ed43u, 0x84a75658bf7e670bu, 0x4c098f7f23622517u, 0x4109e69c7409b0c0u,
    0x277deb12e46c1c6eu, 0x34a905be8cdbaa94u, 0xe164e7aad59875deu, 0x0a0f425428ea020fu,
    0x1f6856683cb71fecu, 0xae88bec82adb6f12u, 0x2374fb5c9f4ab6d7u, 0x1c934c0422ebd0d4u,
    0x0b808f4742ef109du, 0x9ee513807d9a92a1u, 0x9faf8019782b7d7cu, 0x39fef9a48c857bb0u,
    0x176806b615b25533u, 0x9fbae3c1f863d45eu, 0x0c8baab51a95328cu, 0x0f4a50361804cc41u,
    0x5c6827329d7d9673u, 0x2ee8a4e3d5896bfbu, 0x8d8c05bed8ba49a0u, 0xc1b8b8ae2cf3483fu,
    0x0099dd0027803109u
};
static uint64_t mfloat_gammainv_min1024_storage[] = {
    0xe32521bb908f498bu, 0xa687bb4aad126e2cu, 0xa28c65e51cb4223bu, 0xb81bc96c183c6a0fu,
    0x153a9ba3d00c712au, 0x2500e296633b79cbu, 0x316025e3b026785du, 0x930746ce8ba2ad6eu,
    0x80b940314970ae35u, 0x94b46eec1ca45d98u, 0x0fefb390b041d79au, 0x065d07e93cee01b5u,
    0x55f192dfff9f81cbu, 0x7cda497be88691d4u, 0x5f9ccac8086e8a63u, 0x769406f4af041520u,
    0x0f32c76cc342ba13u, 0x74aee8d8851566d4u, 0x00000001c56dc82au
};
static uint64_t mfloat_gammainv_argmin1024_storage[] = {
    0x4f8e942609d8744du, 0x8358f2e344781b01u, 0x684d1904aa33eab9u, 0x73d49af79d24eda7u,
    0xd8f6805ed506a337u, 0x7840b99380f6e926u, 0xe4fe63c014a6f4b2u, 0x47f492ebce4f1f1au,
    0x056ce2893b81c8c6u, 0x73362780ac30294eu, 0xe3364fed43e66490u, 0xc82f1ed2b8609a07u,
    0x5f15787d05f9748au, 0xb982976989379da6u, 0x73ff25892935271fu, 0x1e15ff9c12474612u,
    0x83c220ffee24d4bbu, 0x5f48637884c0e9f1u, 0xc8865e0a4f06b153u, 0x2d86356be3f6e1a9u,
    0x0000000000000176u
};
static uint64_t mfloat_gammainv_31024_storage[] = {
    0x0483d29f7568ac17u, 0x599a5b94c398704eu, 0xe23ae0e66f947d8bu, 0x034ef126740793bdu,
    0x6822b6a3ca312d0bu, 0x8c94d3d16984fb29u, 0xd4d2c616bd7cf637u, 0xdf2490dfe629549au,
    0x780591659ef90d11u, 0xa7c91e0f66776402u, 0xccdbc96a4118c7a7u, 0x72b4d8af0e51f92eu,
    0xdf9d298a5ca489cbu, 0xde5162612d4fa96du, 0x4dbcb33e2a74d8efu, 0xabaf8c2ee9d1bee0u,
    0x19e22322a40b4eb0u, 0x5ea537ab4aa6b29cu, 0x432a1238f0db4e25u, 0xfeb3dfb066d04c4du,
    0x9bb4b36f59d6310eu, 0x2a899b8fd0d0b26bu, 0x0c21fb08de275625u, 0xa904ef5522bb6fd6u,
    0x00d9f9c61b6829e7u
};
static uint64_t mfloat_lambert_w0_11024_storage[] = {
    0x888b74335eba2693u, 0x39c0252142cbd962u, 0x24dcaac705f1cf1eu, 0x370eb2d731d085f7u,
    0xc96ea2fc5379054eu, 0x7b10cf6e78a653d4u, 0x2aa6d9024026ffeau, 0x8ec11680b15b3681u,
    0xd43d63b48b236670u, 0x6ef7d966c5e563f9u, 0x26ab1d9d28c7b784u, 0xd86e6e2dac0588e1u,
    0x34f8df610979b07au, 0x3dec188d659c50a7u, 0x4681a46abb13cfebu, 0xd3f6370795b90e1eu,
    0xd5df5733a297bb27u, 0x49665c0ea1760e00u, 0x8dd63f36423ba821u, 0x535db2d6be9c1869u,
    0xe48bf682b33c04e8u, 0x0324fdc0d4a38de0u, 0x385ba1bb3a15c6a2u, 0x97abf76a98a1b70au,
    0x00244c135f1d2caeu
};
static uint64_t mfloat_neg_tenth_1024_storage[] = {
    0xcccccccccccccccdu, 0xccccccccccccccccu, 0xccccccccccccccccu, 0xccccccccccccccccu,
    0x000000000000000cu
};
static uint64_t mfloat_lambert_wm1_m0_1_1024_storage[] = {
    0xfc2e172c365ca56fu, 0x20a1a8569e18ab19u, 0x84dfabf81bdcfd6fu, 0x0937c3534743f72eu,
    0x1c9469bd1bbf631bu, 0xa0e313da6331a144u, 0xb97dee74e281d291u, 0xd3a8e23c58f6f7e9u,
    0xcca9f7f8a1aa14a6u, 0xef56d028a2944a30u, 0xd899f4b96afaec38u, 0x4ce0919f66f54369u,
    0x789fd12ba02afddeu, 0xfb36be1ec65aa710u, 0x1ce4f0366aa7bca1u, 0xe4f00f35e0fdd1b6u
};
static uint64_t mfloat_sin1_1024_storage[] = {
    0x16f2c072401fd2a9u, 0x4ebae439fc95c651u, 0x83e805382794144cu, 0x903aeaaf0f978321u,
    0x8b591c1dcf87da4au, 0xee0b26400144a253u, 0x39feefe3b4fbdf81u, 0x4efd42a97a60839du,
    0xfedcfeb6db9a349fu, 0x43cdfa62dfb4d194u, 0x9ab53ed64afb3ebau, 0x4a80a28c63a01d13u,
    0x0b47b30f4630cab0u, 0x326188016aec9ba7u, 0x89ec0c8756c4ca72u, 0x933a45ca2734475du,
    0x0000000000000002u, 0x11e811b3af6f3800u, 0x0f63e44595e18346u, 0xe07323acc1b03578u,
    0x6eb22982e9275ee7u, 0x3c8ca9a421398f22u, 0x21389aa94c96289fu, 0x03e30b6e3a6bb75fu,
    0xcf46c07adaccd98du, 0x4d3128675820b949u, 0x1aab1aefe8675ff4u, 0xadac1351e9da8613u,
    0x5ecff8f6e6a2bbb2u, 0xb652feb6324defd8u, 0x2888997a8c5a6f7du, 0x4f484e2879e1944fu,
    0x5523c2433b810637u, 0x00000000000006bbu
};
static uint64_t mfloat_cos1_1024_storage[] = {
    0xda3ba10c77b78f23u, 0xb68a518216650988u, 0x09ad58f9ddcee9beu, 0x2b27076bfcb1ead1u,
    0x76e35e553d1445f5u, 0x0b9d8c423254af8cu, 0x345076f6e3512997u, 0x216cc463c6c93d6eu,
    0x537e052ccb3cc557u, 0x0a3b998872aa2ac5u, 0x177843f2fb8f200eu, 0xfafaf3b943df20f3u,
    0x4f54dd4695ac23c7u, 0x18d78acbffe39b68u, 0x8b3bb0ff17153e6fu, 0x00fde6cde9d1c9d5u,
    0x0000000000000000u, 0xeb5cc98b4e3924b0u, 0xd220afb56b4ea1e3u, 0xbf8b66667641f894u,
    0xb59165486681554fu, 0xaa9503f93f60bd26u, 0xfce87057542071cbu, 0x7610d7f9f7b684eau,
    0xb50f801f6102753au, 0x606fa2352b463757u, 0x9344041db8202049u, 0x3f3450e3b8ff99bcu,
    0x6a94430a52d0e9e4u, 0x2300240b760e6fa9u, 0x2373a894f96c3b7fu, 0x2466d976871bd29au,
    0xa51407da8345c91cu, 0x0000000000000008u
};

static struct _mint_t mfloat_erf_half1024_mint = { .sign = 1, .length = 26, .capacity = 26, .storage = mfloat_erf_half1024_storage };
static struct _mint_t mfloat_erfinv_half1024_mint = { .sign = 1, .length = 26, .capacity = 26, .storage = mfloat_erfinv_half1024_storage };
static struct _mint_t mfloat_tetragamma1_1024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_tetragamma1_1024_storage };
static struct _mint_t mfloat_gammainv_min1024_mint = { .sign = 1, .length = 19, .capacity = 19, .storage = mfloat_gammainv_min1024_storage };
static struct _mint_t mfloat_gammainv_argmin1024_mint = { .sign = 1, .length = 21, .capacity = 21, .storage = mfloat_gammainv_argmin1024_storage };
static struct _mint_t mfloat_gammainv_31024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_gammainv_31024_storage };
static struct _mint_t mfloat_lambert_w0_11024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_lambert_w0_11024_storage };
static struct _mint_t mfloat_sin1_1024_mint = { .sign = 1, .length = 34u, .capacity = 34u, .storage = mfloat_sin1_1024_storage };
static struct _mint_t mfloat_cos1_1024_mint = { .sign = 1, .length = 34u, .capacity = 34u, .storage = mfloat_cos1_1024_storage };
static struct _mint_t mfloat_neg_tenth_1024_mint = { .sign = 1, .length = 5u, .capacity = 5u, .storage = mfloat_neg_tenth_1024_storage };
static struct _mint_t mfloat_lambert_wm1_m0_1_1024_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = mfloat_lambert_wm1_m0_1_1024_storage };

static struct _mfloat_t mfloat_erf_half1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1660, .precision = 1024u, .immortal = true, .mantissa = &mfloat_erf_half1024_mint };
static struct _mfloat_t mfloat_erfinv_half1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1659, .precision = 1024u, .immortal = true, .mantissa = &mfloat_erfinv_half1024_mint };
static struct _mfloat_t mfloat_tetragamma1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1590, .precision = 1024u, .immortal = true, .mantissa = &mfloat_tetragamma1_1024_mint };
static struct _mfloat_t mfloat_gammainv_min1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1185, .precision = 1024u, .immortal = true, .mantissa = &mfloat_gammainv_min1024_mint };
static struct _mfloat_t mfloat_gammainv_argmin1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1288, .precision = 1024u, .immortal = true, .mantissa = &mfloat_gammainv_argmin1024_mint };
static struct _mfloat_t mfloat_gammainv_31024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1590, .precision = 1024u, .immortal = true, .mantissa = &mfloat_gammainv_31024_mint };
static struct _mfloat_t mfloat_lambert_w0_11024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1590, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lambert_w0_11024_mint };
static struct _mfloat_t mfloat_sin1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2123, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sin1_1024_mint };
static struct _mfloat_t mfloat_cos1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2116, .precision = 1024u, .immortal = true, .mantissa = &mfloat_cos1_1024_mint };
static struct _mfloat_t mfloat_neg_tenth_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -263, .precision = 1024u, .immortal = true, .mantissa = &mfloat_neg_tenth_1024_mint };
static struct _mfloat_t mfloat_lambert_wm1_m0_1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1022, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lambert_wm1_m0_1_1024_mint };

static uint64_t mfloat_ln2_1024_storage[] = {
    0x607f4ca11fb5bfb9u, 0x2da2d97c50f3fd5cu, 0x8655fa1872f20e3au, 0xaf5dfa6bd3830324u,
    0xb72ce87b19d6548cu, 0xb256fa0ec7657f74u, 0xeb9ea9bc3b136603u, 0x51acbda11317c387u,
    0x53e96ca16224ae8cu, 0x427573b291169b82u, 0x0ed2eae35c138214u, 0x8559552fb4afa1b1u,
    0xbe7b876206debac9u, 0xd8a0d175b8baafa2u, 0xf40f343267298b62u, 0xbc9e3b39803f2f6au,
    0x0b17217f7d1cf79au
};
static struct _mint_t mfloat_ln2_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_ln2_1024_storage };
static struct _mfloat_t mfloat_ln2_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1084, .precision = 1088u, .immortal = true, .mantissa = &mfloat_ln2_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_0_1024_storage[] = {
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x0000000000000015u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_0_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_0_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_0_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1032, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_0_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_1_1024_storage[] = {
    0xb05b05b05b05b05bu, 0x5b05b05b05b05b05u, 0x05b05b05b05b05b0u, 0xb05b05b05b05b05bu,
    0x5b05b05b05b05b05u, 0x05b05b05b05b05b0u, 0xb05b05b05b05b05bu, 0x5b05b05b05b05b05u,
    0x05b05b05b05b05b0u, 0xb05b05b05b05b05bu, 0x5b05b05b05b05b05u, 0x05b05b05b05b05b0u,
    0xb05b05b05b05b05bu, 0x5b05b05b05b05b05u, 0x05b05b05b05b05b0u, 0xb05b05b05b05b05bu,
    0x0000000000000005u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_1_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_1_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1035, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_1_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_2_1024_storage[] = {
    0xd00d00d00d00d00du, 0x0d00d00d00d00d00u, 0x00d00d00d00d00d0u, 0xd00d00d00d00d00du,
    0x0d00d00d00d00d00u, 0x00d00d00d00d00d0u, 0xd00d00d00d00d00du, 0x0d00d00d00d00d00u,
    0x00d00d00d00d00d0u, 0xd00d00d00d00d00du, 0x0d00d00d00d00d00u, 0x00d00d00d00d00d0u,
    0xd00d00d00d00d00du, 0x0d00d00d00d00d00u, 0x00d00d00d00d00d0u, 0xd00d00d00d00d00du
};
static struct _mint_t mfloat_lgamma_asymptotic_term_2_1024_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = mfloat_lgamma_asymptotic_term_2_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_2_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1034, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_2_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_3_1024_storage[] = {
    0x7027027027027027u, 0x2702702702702702u, 0x0270270270270270u, 0x7027027027027027u,
    0x2702702702702702u, 0x0270270270270270u, 0x7027027027027027u, 0x2702702702702702u,
    0x0270270270270270u, 0x7027027027027027u, 0x2702702702702702u, 0x0270270270270270u,
    0x7027027027027027u, 0x2702702702702702u, 0x0270270270270270u, 0x7027027027027027u,
    0x0000000000000002u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_3_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_3_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_3_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1036, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_3_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_4_1024_storage[] = {
    0x3a9ce01b951e2b19u, 0x6e5478ac63fc8d5cu, 0xb18ff23570ea7380u, 0xd5c3a9ce01b951e2u,
    0x3806e5478ac63fc8u, 0x1e2b18ff23570ea7u, 0xfc8d5c3a9ce01b95u, 0xea73806e5478ac63u,
    0xb951e2b18ff23570u, 0xc63fc8d5c3a9ce01u, 0x570ea73806e5478au, 0xe01b951e2b18ff23u,
    0x78ac63fc8d5c3a9cu, 0xf23570ea73806e54u, 0xa9ce01b951e2b18fu, 0xe5478ac63fc8d5c3u,
    0x0000000000000006u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_4_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_4_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_4_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1037, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_4_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_5_1024_storage[] = {
    0xb0d9993c7c81f6abu, 0xab0d9993c7c81f6au, 0x6ab0d9993c7c81f6u, 0xf6ab0d9993c7c81fu,
    0x1f6ab0d9993c7c81u, 0x81f6ab0d9993c7c8u, 0xc81f6ab0d9993c7cu, 0x7c81f6ab0d9993c7u,
    0xc7c81f6ab0d9993cu, 0x3c7c81f6ab0d9993u, 0x93c7c81f6ab0d999u, 0x993c7c81f6ab0d99u,
    0x9993c7c81f6ab0d9u, 0xd9993c7c81f6ab0du, 0x0d9993c7c81f6ab0u, 0xb0d9993c7c81f6abu,
    0x0000000000001f6au
};
static struct _mint_t mfloat_lgamma_asymptotic_term_5_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_5_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_5_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1046, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_5_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_6_1024_storage[] = {
    0x9069069069069069u, 0x6906906906906906u, 0x0690690690690690u, 0x9069069069069069u,
    0x6906906906906906u, 0x0690690690690690u, 0x9069069069069069u, 0x6906906906906906u,
    0x0690690690690690u, 0x9069069069069069u, 0x6906906906906906u, 0x0690690690690690u,
    0x9069069069069069u, 0x6906906906906906u, 0x0690690690690690u, 0x9069069069069069u,
    0x0000000000000006u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_6_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_6_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_6_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1034, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_6_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_7_1024_storage[] = {
    0x36587a9cbee10325u, 0x2547698badcff214u, 0x1436587a9cbee103u, 0x032547698badcff2u,
    0xf21436587a9cbee1u, 0xe1032547698badcfu, 0xcff21436587a9cbeu, 0xbee1032547698badu,
    0xadcff21436587a9cu, 0x9cbee1032547698bu, 0x8badcff21436587au, 0x7a9cbee103254769u,
    0x698badcff2143658u, 0x587a9cbee1032547u, 0x47698badcff21436u, 0x36587a9cbee10325u,
    0x000000000000f214u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_7_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_7_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_7_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1045, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_7_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_8_1024_storage[] = {
    0x3ffd0c3b7f4b1c0fu, 0xfd0c3b7f4b1c0f03u, 0x0c3b7f4b1c0f033fu, 0x3b7f4b1c0f033ffdu,
    0x7f4b1c0f033ffd0cu, 0x4b1c0f033ffd0c3bu, 0x1c0f033ffd0c3b7fu, 0x0f033ffd0c3b7f4bu,
    0x033ffd0c3b7f4b1cu, 0x3ffd0c3b7f4b1c0fu, 0xfd0c3b7f4b1c0f03u, 0x0c3b7f4b1c0f033fu,
    0x3b7f4b1c0f033ffdu, 0x7f4b1c0f033ffd0cu, 0x4b1c0f033ffd0c3bu, 0x1c0f033ffd0c3b7fu,
    0x00000000000b7f4bu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_8_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_8_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_8_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1046, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_8_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_9_1024_storage[] = {
    0xcc4b30fa981d22c5u, 0xe02303e73db7b739u, 0x2c5307d2a9c088ecu, 0x739cc4b30fa981d2u,
    0x8ece02303e73db7bu, 0x1d22c5307d2a9c08u, 0xb7b739cc4b30fa98u, 0xc088ece02303e73du,
    0xa981d22c5307d2a9u, 0x73db7b739cc4b30fu, 0x2a9c088ece02303eu, 0x30fa981d22c5307du,
    0x03e73db7b739cc4bu, 0x07d2a9c088ece023u, 0xc4b30fa981d22c53u, 0x02303e73db7b739cu,
    0x00000000002c8eceu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_9_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_9_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_9_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1045, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_9_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_10_1024_storage[] = {
    0x74f53910c8b38017u, 0xd3d4e44322ce005au, 0x4f53910c8b380169u, 0x3d4e44322ce005a7u,
    0xf53910c8b380169du, 0xd4e44322ce005a74u, 0x53910c8b380169d3u, 0x4e44322ce005a74fu,
    0x3910c8b380169d3du, 0xe44322ce005a74f5u, 0x910c8b380169d3d4u, 0x44322ce005a74f53u,
    0x10c8b380169d3d4eu, 0x4322ce005a74f539u, 0x0c8b380169d3d4e4u, 0x322ce005a74f5391u,
    0x00000000001ace44u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_10_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_10_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_10_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1041, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_10_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_11_1024_storage[] = {
    0xee25d73c0f9473b7u, 0x952525cccc1aab67u, 0xe25d73c0f9473b68u, 0x52525cccc1aab67eu,
    0x25d73c0f9473b689u, 0x2525cccc1aab67eeu, 0x5d73c0f9473b6895u, 0x525cccc1aab67ee2u,
    0xd73c0f9473b68952u, 0x25cccc1aab67ee25u, 0x73c0f9473b689525u, 0x5cccc1aab67ee25du,
    0x3c0f9473b6895252u, 0xcccc1aab67ee25d7u, 0xc0f9473b68952525u, 0xccc1aab67ee25d73u,
    0x0000000139b2525cu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_11_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_11_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_11_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1049, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_11_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_12_1024_storage[] = {
    0xd0369d0369d0369du, 0x9d0369d0369d0369u, 0x69d0369d0369d036u, 0x369d0369d0369d03u,
    0x0369d0369d0369d0u, 0xd0369d0369d0369du, 0x9d0369d0369d0369u, 0x69d0369d0369d036u,
    0x369d0369d0369d03u, 0x0369d0369d0369d0u, 0xd0369d0369d0369du, 0x9d0369d0369d0369u,
    0x69d0369d0369d036u, 0x369d0369d0369d03u, 0x0369d0369d0369d0u, 0xd0369d0369d0369du,
    0x0000000000224469u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_12_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_12_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_12_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1034, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_12_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_13_1024_storage[] = {
    0x16bbd7a20371a00du, 0x08591aa839b7978du, 0xe36729b25c5fa4a1u, 0x7eb3feddd849691fu,
    0x7c166824cae1c4abu, 0xbb303c552e462423u, 0xa8e11d45d070a4f4u, 0x6295ea98882f413cu,
    0xe26c112cf758c6f5u, 0xf65dae6ffd8f0cecu, 0x7538bc7f07b1b4f9u, 0x00d40954332d9ebeu,
    0x78d16bbd7a20371au, 0x4a108591aa839b79u, 0x91fe36729b25c5fau, 0x4ab7eb3feddd8496u,
    0x00000011a198ae1cu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_13_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_13_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_13_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1045, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_13_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_14_1024_storage[] = {
    0xd3560ae5824f57b9u, 0x1a732bb72c01a608u, 0xc23470867b93f711u, 0xb8d3d44632c2134du,
    0x5f69edd1a0f72a08u, 0x9eca7b8da74d3da9u, 0x24f57b958b0211a5u, 0xc01a608d3560ae58u,
    0xb93f7111a732bb72u, 0x2c2134dc23470867u, 0x0f72a08b8d3d4463u, 0x74d3da95f69edd1au,
    0xb0211a59eca7b8dau, 0x560ae5824f57b958u, 0x732bb72c01a608d3u, 0x3470867b93f7111au,
    0x00002a3441134dc2u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_14_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_14_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_14_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1050, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_14_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_15_1024_storage[] = {
    0x5de2030b4d5de203u, 0x030b4d5de2030b4du, 0x4d5de2030b4d5de2u, 0xe2030b4d5de2030bu,
    0x0b4d5de2030b4d5du, 0x5de2030b4d5de203u, 0x030b4d5de2030b4du, 0x4d5de2030b4d5de2u,
    0xe2030b4d5de2030bu, 0x0b4d5de2030b4d5du, 0x5de2030b4d5de203u, 0x030b4d5de2030b4du,
    0x4d5de2030b4d5de2u, 0xe2030b4d5de2030bu, 0x0b4d5de2030b4d5du, 0x5de2030b4d5de203u,
    0x0000744226c50b4du
};
static struct _mint_t mfloat_lgamma_asymptotic_term_15_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_15_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_15_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1047, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_15_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_16_1024_storage[] = {
    0x76f31219dbcc4867u, 0x676f31219dbcc486u, 0x8676f31219dbcc48u, 0x48676f31219dbcc4u,
    0xc48676f31219dbccu, 0xcc48676f31219dbcu, 0xbcc48676f31219dbu, 0xdbcc48676f31219du,
    0x9dbcc48676f31219u, 0x19dbcc48676f3121u, 0x219dbcc48676f312u, 0x1219dbcc48676f31u,
    0x31219dbcc48676f3u, 0xf31219dbcc48676fu, 0x6f31219dbcc48676u, 0x76f31219dbcc4867u,
    0x000002da5341ec86u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_16_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_16_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_16_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1037, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_16_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_17_1024_storage[] = {
    0xfc6c267745431a2du, 0xe12dc7ba4a6aa1f7u, 0x35182f51bb16ff4cu, 0x68a4590b8c069d56u,
    0x19605c010dedcb3cu, 0x67ac76153e4043b9u, 0xfceb1c8eb54b098eu, 0x6023df2a525a036eu,
    0x2b2f9f59aa78764du, 0x801460faed7d9dd5u, 0x89684b6284ee4a32u, 0x6f9bd78c3ebf39d0u,
    0xec4c938f344120feu, 0xc19adfa948717376u, 0xa2301e4fc1e87e3cu, 0x809357125d858d36u,
    0x8a233b3c8cddaba9u, 0x0000000000000028u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_17_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_17_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_17_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1060, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_17_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_18_1024_storage[] = {
    0x8127350b88127351u, 0x27350b88127350b8u, 0x350b88127350b881u, 0x0b88127350b88127u,
    0x88127350b8812735u, 0x127350b88127350bu, 0x7350b88127350b88u, 0x50b88127350b8812u,
    0xb88127350b881273u, 0x8127350b88127350u, 0x27350b88127350b8u, 0x350b88127350b881u,
    0x0b88127350b88127u, 0x88127350b8812735u, 0x127350b88127350bu, 0x7350b88127350b88u,
    0x000a1bbcde4ea012u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_18_1024_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = mfloat_lgamma_asymptotic_term_18_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_18_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1037, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_18_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_19_1024_storage[] = {
    0x6b02e295ec854487u, 0x86b02e295ec85448u, 0x486b02e295ec8544u, 0x4486b02e295ec854u,
    0x54486b02e295ec85u, 0x854486b02e295ec8u, 0xc854486b02e295ecu, 0xec854486b02e295eu,
    0x5ec854486b02e295u, 0x95ec854486b02e29u, 0x295ec854486b02e2u, 0xe295ec854486b02eu,
    0x2e295ec854486b02u, 0x02e295ec854486b0u, 0xb02e295ec854486bu, 0x6b02e295ec854486u,
    0x00b7bc07a8c85448u, 0x0000000000000168u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_19_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_19_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_19_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1053, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_19_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_20_1024_storage[] = {
    0x55812a5a765b9ebbu, 0xddd9593d0ae7da3du, 0x8753cdb5d5c8db31u, 0xfa958e81363d15c7u,
    0x1a06e6f094aa68bfu, 0xf22882d87f82c891u, 0x65b9ebafa849d261u, 0xae7da3d55812a5a7u,
    0x5c8db31ddd9593d0u, 0x63d15c78753cdb5du, 0x4aa68bffa958e813u, 0xf82c8911a06e6f09u,
    0x849d261f22882d87u, 0x812a5a765b9ebafau, 0xd9593d0ae7da3d55u, 0x53cdb5d5c8db31ddu,
    0x8cd6f8f1f755c787u, 0x00000000000001bcu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_20_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_20_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_20_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1048, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_20_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_21_1024_storage[] = {
    0x3225b901be2f0a8bu, 0xde05a0284fa7ec42u, 0x3a2bef5d5ff69ccau, 0xd471d2e43bfbca17u,
    0xa8b30da65eab49b3u, 0xc423225b901be2f0u, 0xccade05a0284fa7eu, 0xa173a2bef5d5ff69u,
    0x9b3d471d2e43bfbcu, 0x2f0a8b30da65eab4u, 0xa7ec423225b901beu, 0xf69ccade05a0284fu,
    0xfbca173a2bef5d5fu, 0xab49b3d471d2e43bu, 0x1be2f0a8b30da65eu, 0x84fa7ec423225b90u,
    0xebb143bb94de5a02u, 0x00000000000004bbu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_21_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_21_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_21_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1044, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_21_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_22_1024_storage[] = {
    0x91d026165a2274c3u, 0x2cbe9913537a35d2u, 0x51a96a799ae41ed3u, 0x0b755e5ef9e85b58u,
    0xb6baf77ca18c5dc4u, 0xe68a8cf8b27e7257u, 0xfcef03974288213eu, 0x3b44f3db0ada6febu,
    0xcd1f6e1b31960cabu, 0x24e07d3c7ad0c104u, 0x8ec97dd76943bdfeu, 0x930602fc54152445u,
    0x37086eb6200909a4u, 0x291d026165a2274cu, 0x32cbe9913537a35du, 0x851a96a799ae41edu,
    0x99bfad785f6c85b5u, 0x0000000000007170u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_22_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_22_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_22_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1043, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_22_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_23_1024_storage[] = {
    0x1fff7a0a35f1dea3u, 0x0fd13056a02ae43eu, 0xdc151ab5f46c69b3u, 0x71b338ad9d6f2571u,
    0x464904c38fd3be1du, 0xab0fa853e987a59du, 0x5104ec6c40d6a2b8u, 0x307f7c97fcc366f7u,
    0xf1dea2e1877cc0d9u, 0x2ae43e1fff7a0a35u, 0x6c69b30fd13056a0u, 0x6f2571dc151ab5f4u,
    0xd3be1d71b338ad9du, 0x87a59d464904c38fu, 0xd6a2b8ab0fa853e9u, 0xc366f75104ec6c40u,
    0xc56c480b5a7c97fcu, 0x000000005cf04a02u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_23_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_23_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_23_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1053, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_23_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_24_1024_storage[] = {
    0x2bdaa31141b3b0d7u, 0x8e2b610c81d2167bu, 0x5699917e7ef88f1eu, 0x8c4506cec35d3fcdu,
    0x8432074859ecaf6au, 0x45f9fbe23c7a38adu, 0x1b3b0d74ff355a66u, 0x1d2167b2bdaa3114u,
    0xef88f1e8e2b610c8u, 0x35d3fcd5699917e7u, 0x9ecaf6a8c4506cecu, 0xc7a38ad843207485u,
    0xf355a6645f9fbe23u, 0xdaa31141b3b0d74fu, 0x2b610c81d2167b2bu, 0x99917e7ef88f1e8eu,
    0xf3ce358f90064d56u, 0x00000000014befddu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_24_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_24_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_24_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1041, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_24_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_25_1024_storage[] = {
    0x69a537bbc232e73bu, 0x883c904971b21924u, 0x076e79befa8d1117u, 0xe2666cdd91e59ec7u,
    0x3af41c5cc3cf144fu, 0x2469a537bbc232e7u, 0x17883c904971b219u, 0xc7076e79befa8d11u,
    0x4fe2666cdd91e59eu, 0xe73af41c5cc3cf14u, 0x192469a537bbc232u, 0x1117883c904971b2u,
    0x9ec7076e79befa8du, 0x144fe2666cdd91e5u, 0x32e73af41c5cc3cfu, 0xb2192469a537bbc2u,
    0xe550213369924971u, 0x0000000283be0395u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_25_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_25_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_25_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1044, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_25_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_26_1024_storage[] = {
    0x20324269f2d13ab1u, 0x3a4f4b5b78ad0197u, 0xc19bdc46c38c0e45u, 0x26536bcb98efade1u,
    0x34d4730b402eafd8u, 0x554121caa5d4e033u, 0x6629d7873d87a585u, 0xd149d7be55bc0fccu,
    0x6062357ac41cadceu, 0x6f16e36b8efe4f9fu, 0x676439a588c6c468u, 0xdb5ead95a82d0fe1u,
    0xc75507f8adddf7f1u, 0x231e832502fffebcu, 0x2690066ba7710d48u, 0x0256ec758797bf48u,
    0x7dd999790760b0ceu, 0x00000a8ebfe48da1u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_26_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_26_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_26_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1048, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_26_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_27_1024_storage[] = {
    0x96cbec2ef3ba1b5fu, 0x8b14782409b737c7u, 0xbec2ef3ba1b5ef12u, 0x4782409b737c796cu,
    0x2ef3ba1b5ef128b1u, 0x2409b737c796cbecu, 0x3ba1b5ef128b1478u, 0x9b737c796cbec2efu,
    0x1b5ef128b1478240u, 0x37c796cbec2ef3bau, 0xef128b14782409b7u, 0x796cbec2ef3ba1b5u,
    0x28b14782409b737cu, 0xcbec2ef3ba1b5ef1u, 0x14782409b737c796u, 0xc2ef3ba1b5ef128bu,
    0xfb1724c95ab56cbeu, 0x0000bf582a433556u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_27_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_27_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_27_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1046, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_27_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_28_1024_storage[] = {
    0x42c8e9a5361285edu, 0x992f11af905a88f8u, 0xf252c54240804ab3u, 0x79d2469747e290e2u,
    0xa6e2cb593daf4267u, 0xda1dca0df4aee0f7u, 0xb5a757b7eff6a98cu, 0xc5b72d1703ade3a1u,
    0x23a694d84a17b310u, 0xbc46be416a23e10bu, 0x4b150902012ace64u, 0x491a5d1f8a438bc9u,
    0x8b2d64f6bd099de7u, 0x772837d2bb83de9bu, 0x9d5edfbfdaa63368u, 0xdcb45c0eb78e86d6u,
    0xd0e7770e67104316u, 0x000e940b3723e6c7u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_28_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_28_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_28_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1044, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_28_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_29_1024_storage[] = {
    0x076e9f0132ada8c3u, 0x9bdab5deb80c7b17u, 0x426b3a8da6144e33u, 0x3c4969e5994b92fdu,
    0x656521774c906268u, 0x49dcd442d894f4cbu, 0x5eebe74cdb187f07u, 0xf197f6dcf067c4afu,
    0xab08244f6f91553du, 0x09e03cd87d7b40b7u, 0x46dac2b7d5f1bcb9u, 0x6f469f27269783eeu,
    0x791fcdbe610e59dfu, 0x619aaa6b15cb7924u, 0x3e81cd5cee1ba1e0u, 0x18c9bd60a7d4b5c7u,
    0x3b4f451835e1174bu, 0x150689672663f8ccu, 0x000000000000989au
};
static struct _mint_t mfloat_lgamma_asymptotic_term_29_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_29_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_29_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1065, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_29_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_30_1024_storage[] = {
    0x30f63528917c80b3u, 0xb30f63528917c80bu, 0x0b30f63528917c80u, 0x80b30f63528917c8u,
    0xc80b30f63528917cu, 0x7c80b30f63528917u, 0x17c80b30f6352891u, 0x917c80b30f635289u,
    0x8917c80b30f63528u, 0x28917c80b30f6352u, 0x528917c80b30f635u, 0x3528917c80b30f63u,
    0x63528917c80b30f6u, 0xf63528917c80b30fu, 0x0f63528917c80b30u, 0x30f63528917c80b3u,
    0xb563efb6570e400bu, 0x03573b230396b33fu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_30_1024_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = mfloat_lgamma_asymptotic_term_30_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_30_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1037, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_30_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_31_1024_storage[] = {
    0x00bfbbab6a665615u, 0xbfbbab6a66561511u, 0xbbab6a6656151100u, 0xab6a6656151100bfu,
    0x6a6656151100bfbbu, 0x6656151100bfbbabu, 0x56151100bfbbab6au, 0x151100bfbbab6a66u,
    0x1100bfbbab6a6656u, 0x00bfbbab6a665615u, 0xbfbbab6a66561511u, 0xbbab6a6656151100u,
    0xab6a6656151100bfu, 0x6a6656151100bfbbu, 0x6656151100bfbbabu, 0x56151100bfbbab6au,
    0x2dce139c786c6a66u, 0x29b6552aeca37097u, 0x0000000000000500u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_31_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_31_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_31_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1047, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_31_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_32_1024_storage[] = {
    0x1a0073ac314af8c7u, 0xe609af2ecd0c6123u, 0xa0073ac314af8c6cu, 0x609af2ecd0c61231u,
    0x0073ac314af8c6ceu, 0x09af2ecd0c61231au, 0x073ac314af8c6ce6u, 0x9af2ecd0c61231a0u,
    0x73ac314af8c6ce60u, 0xaf2ecd0c61231a00u, 0x3ac314af8c6ce609u, 0xf2ecd0c61231a007u,
    0xac314af8c6ce609au, 0x2ecd0c61231a0073u, 0xc314af8c6ce609afu, 0xecd0c61231a0073au,
    0x02af139676609af2u, 0x4038c6823c029cc8u, 0x00000000007fae52u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_32_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_32_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_32_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1053, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_32_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_33_1024_storage[] = {
    0xa65618f6933b59ebu, 0xcc8b970d80209adfu, 0x65618f6933b59eb1u, 0xc8b970d80209adfau,
    0x5618f6933b59eb1cu, 0x8b970d80209adfa6u, 0x618f6933b59eb1ccu, 0xb970d80209adfa65u,
    0x18f6933b59eb1cc8u, 0x970d80209adfa656u, 0x8f6933b59eb1cc8bu, 0x70d80209adfa6561u,
    0xf6933b59eb1cc8b9u, 0x0d80209adfa65618u, 0x6933b59eb1cc8b97u, 0xd80209adfa65618fu,
    0x652da1ac906f8970u, 0xcde8fd42524351ceu, 0x000000000000d8cau
};
static struct _mint_t mfloat_lgamma_asymptotic_term_33_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_33_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_33_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1039, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_33_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_34_1024_storage[] = {
    0xdbe0f4ffd97d3e71u, 0x6b40f5c26e26cc88u, 0x9c4c47cbc50fe324u, 0x434d4fb2316bccd0u,
    0x94415f9010bf42feu, 0x140db0130700faedu, 0x124334fd1cffa8d8u, 0xcc2a0034b2edebd1u,
    0xf7f310e28789cb74u, 0xb290477912ae39f3u, 0x7b4219a16c7bf001u, 0x2d4755ce22b1cc7bu,
    0xc4d92954bb4a4f1au, 0x02592c5f0df08778u, 0xc68674846994e1e7u, 0x628a941e1b048897u,
    0xa7fee9b6beb2614eu, 0x5e8025fa36930530u, 0x0000000061baddfau
};
static struct _mint_t mfloat_lgamma_asymptotic_term_34_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_34_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_34_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1047, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_34_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_35_1024_storage[] = {
    0xfe1d95b668abc3b9u, 0xa69951737ce42c86u, 0xda3836312c495f54u, 0x71e6de3e50956a48u,
    0x530982d262efb16au, 0x1453f2d6510b8e80u, 0x083af329621783eeu, 0x4cfa5f77b9c4d4d7u,
    0x7c2cfd22550e87b5u, 0xa05e7c791cf3e091u, 0x36d302208eeb629cu, 0xe8a46175ae16e4bdu,
    0x0cf1af5ec3f6c46cu, 0x15bdcaef3b463a6eu, 0xc165fa0c66adcf67u, 0x4cbcc0aecaa7bd9eu,
    0xd9658951ddd34102u, 0x24e763a3c5acde64u, 0x02eb4cd886fec94bu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_35_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_35_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_35_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1067, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_35_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_36_1024_storage[] = {
    0x012b404ad012b405u, 0x2b404ad012b404adu, 0x404ad012b404ad01u, 0x4ad012b404ad012bu,
    0xd012b404ad012b40u, 0x12b404ad012b404au, 0xb404ad012b404ad0u, 0x04ad012b404ad012u,
    0xad012b404ad012b4u, 0x012b404ad012b404u, 0x2b404ad012b404adu, 0x404ad012b404ad01u,
    0x4ad012b404ad012bu, 0xd012b404ad012b40u, 0x12b404ad012b404au, 0xb404ad012b404ad0u,
    0x2fe906abb0f66012u, 0x3920b45f50fd61d7u, 0x0000000bcff6120eu
};
static struct _mint_t mfloat_lgamma_asymptotic_term_36_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_36_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_36_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1038, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_36_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_37_1024_storage[] = {
    0xfcd070de181ef293u, 0x2c99d3daae4ebbf5u, 0x8f966a0a77b1b88cu, 0x25c6336d7447e855u,
    0xef293003a4114b52u, 0xebbf5fcd070de181u, 0x1b88c2c99d3daae4u, 0x7e8558f966a0a77bu,
    0x14b5225c6336d744u, 0xde181ef293003a41u, 0xdaae4ebbf5fcd070u, 0x0a77b1b88c2c99d3u,
    0x6d7447e8558f966au, 0x03a4114b5225c633u, 0xcd070de181ef2930u, 0xc99d3daae4ebbf5fu,
    0x73dfa8ff907948c2u, 0xab673e371e77208cu, 0x00003282abec3834u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_37_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_37_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_37_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1041, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_37_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_38_1024_storage[] = {
    0x3c3ac6d4730066c7u, 0xae61fed40c654311u, 0xcf7fd13a8fef8793u, 0x794b50ddf5d79f20u,
    0x5c87484409075f18u, 0x0a95feeeb9da1106u, 0x3343fe415262d64du, 0x2ac8d99562755698u,
    0x2918e0ed20742e10u, 0x776680a20830ecc6u, 0xb41b35948afc5867u, 0xeea44df79b168abbu,
    0xf591d9792afeb7d4u, 0x447f89eae67a3855u, 0x8f5ccd91419dcd2fu, 0xfe3271a69e84561cu,
    0x4767c95d95faac26u, 0xa978134fc5fbd6dfu, 0x0e3e6a8d1ceca9d6u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_38_1024_mint = { .sign = 1, .length = 19u, .capacity = 19u, .storage = mfloat_lgamma_asymptotic_term_38_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_38_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1048, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_38_1024_mint };

static uint64_t mfloat_lgamma_asymptotic_term_39_1024_storage[] = {
    0xe415f0ce0c21a7c3u, 0x92d487454d077d08u, 0x260e3658804675e8u, 0x70acc115da5d3b56u,
    0xd149b37a65768dc0u, 0xa9c1affea7acff41u, 0x82a0ae2822e71556u, 0x2fc6ebd06659e753u,
    0x9e0aba9ab6b7f749u, 0x4faaea911473502eu, 0xf7875bbdd8488a99u, 0xed907a4f05bcf4eeu,
    0x22d0a485de3ffac4u, 0x0c3818943b945205u, 0xa7294bc82b224cf1u, 0x8fd5216997792624u,
    0x59c1a0da6c9fcaceu, 0x288b6e7f7fab43fcu, 0xb7fead15a196f6ddu, 0x00000000000043b7u
};
static struct _mint_t mfloat_lgamma_asymptotic_term_39_1024_mint = { .sign = 1, .length = 20u, .capacity = 20u, .storage = mfloat_lgamma_asymptotic_term_39_1024_storage };
static struct _mfloat_t mfloat_lgamma_asymptotic_term_39_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1059, .precision = 1024u, .immortal = true, .mantissa = &mfloat_lgamma_asymptotic_term_39_1024_mint };

static const mfloat_t * const mfloat_lgamma_asymptotic_1024[40] = {
    &mfloat_lgamma_asymptotic_term_0_1024_static,
    &mfloat_lgamma_asymptotic_term_1_1024_static,
    &mfloat_lgamma_asymptotic_term_2_1024_static,
    &mfloat_lgamma_asymptotic_term_3_1024_static,
    &mfloat_lgamma_asymptotic_term_4_1024_static,
    &mfloat_lgamma_asymptotic_term_5_1024_static,
    &mfloat_lgamma_asymptotic_term_6_1024_static,
    &mfloat_lgamma_asymptotic_term_7_1024_static,
    &mfloat_lgamma_asymptotic_term_8_1024_static,
    &mfloat_lgamma_asymptotic_term_9_1024_static,
    &mfloat_lgamma_asymptotic_term_10_1024_static,
    &mfloat_lgamma_asymptotic_term_11_1024_static,
    &mfloat_lgamma_asymptotic_term_12_1024_static,
    &mfloat_lgamma_asymptotic_term_13_1024_static,
    &mfloat_lgamma_asymptotic_term_14_1024_static,
    &mfloat_lgamma_asymptotic_term_15_1024_static,
    &mfloat_lgamma_asymptotic_term_16_1024_static,
    &mfloat_lgamma_asymptotic_term_17_1024_static,
    &mfloat_lgamma_asymptotic_term_18_1024_static,
    &mfloat_lgamma_asymptotic_term_19_1024_static,
    &mfloat_lgamma_asymptotic_term_20_1024_static,
    &mfloat_lgamma_asymptotic_term_21_1024_static,
    &mfloat_lgamma_asymptotic_term_22_1024_static,
    &mfloat_lgamma_asymptotic_term_23_1024_static,
    &mfloat_lgamma_asymptotic_term_24_1024_static,
    &mfloat_lgamma_asymptotic_term_25_1024_static,
    &mfloat_lgamma_asymptotic_term_26_1024_static,
    &mfloat_lgamma_asymptotic_term_27_1024_static,
    &mfloat_lgamma_asymptotic_term_28_1024_static,
    &mfloat_lgamma_asymptotic_term_29_1024_static,
    &mfloat_lgamma_asymptotic_term_30_1024_static,
    &mfloat_lgamma_asymptotic_term_31_1024_static,
    &mfloat_lgamma_asymptotic_term_32_1024_static,
    &mfloat_lgamma_asymptotic_term_33_1024_static,
    &mfloat_lgamma_asymptotic_term_34_1024_static,
    &mfloat_lgamma_asymptotic_term_35_1024_static,
    &mfloat_lgamma_asymptotic_term_36_1024_static,
    &mfloat_lgamma_asymptotic_term_37_1024_static,
    &mfloat_lgamma_asymptotic_term_38_1024_static,
    &mfloat_lgamma_asymptotic_term_39_1024_static,
};


static mfloat_t *mfloat_clone_prec(const mfloat_t *src, size_t precision);
static int mfloat_round_to_precision(mfloat_t *mfloat, size_t precision);
static int mfloat_compute_pi(mfloat_t *dst, size_t precision);
static int mfloat_compute_e(mfloat_t *dst, size_t precision);
static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision);
static int mfloat_compute_sqrt_pi(mfloat_t *dst, size_t precision);
static int mfloat_compute_half_ln_pi(mfloat_t *dst, size_t precision);
static void mfloat_scratch_init_slot(mfloat_scratch_slot_t *slot, size_t precision);
static void mfloat_scratch_reset_slot(mfloat_scratch_slot_t *slot, size_t precision);
static void mfloat_scratch_release_slot(mfloat_scratch_slot_t *slot);
static int mfloat_scratch_copy(mfloat_t *dst, const mfloat_t *src);
static int mfloat_is_below_neg_bits(const mfloat_t *mfloat, long bits);
static int mfloat_get_exact_long_value(const mfloat_t *mfloat, long *out);
static int mfloat_set_from_const_mint_local(mfloat_t *dst, const mint_t *src, long exponent2);
static int mfloat_make_const_rational_local(mfloat_t *dst, const mint_t *num, const mint_t *den, size_t precision);
static int mfloat_copy_lgamma_asymptotic_term_local(mfloat_t *dst, size_t index, size_t precision);
static int mfloat_mul_euler_gamma_coeff_local(mfloat_t *mfloat, size_t index, size_t precision);
static int mfloat_equals_exact_long(const mfloat_t *mfloat, long value);
static int mfloat_is_exact_half(const mfloat_t *mfloat, short sign);
static int mfloat_equals_const_value(const mfloat_t *mfloat, const mfloat_t *constant);
static int mfloat_is_near_const_value(const mfloat_t *mfloat, const mfloat_t *constant, long bits);
static size_t mfloat_transcendental_work_prec(size_t precision);
static size_t mfloat_cap_work_prec(size_t work_prec);
static int mfloat_reduce_trig_argument(const mfloat_t *src,
                                       size_t precision,
                                       mfloat_t **r_out,
                                       int *quadrant_out);
static int mfloat_finish_result(mfloat_t *dst, mfloat_t *src, size_t precision);

static void mfloat_scratch_init_slot(mfloat_scratch_slot_t *slot, size_t precision)
{
    memset(slot, 0, sizeof(*slot));
    slot->value.kind = MFLOAT_KIND_FINITE;
    slot->value.precision = precision;
    slot->value.mantissa = &slot->mantissa;
}

static void mfloat_scratch_reset_slot(mfloat_scratch_slot_t *slot, size_t precision)
{
    slot->value.kind = MFLOAT_KIND_FINITE;
    slot->value.sign = 0;
    slot->value.exponent2 = 0;
    slot->value.precision = precision;
    slot->value.mantissa = &slot->mantissa;
    slot->mantissa.sign = 0;
    slot->mantissa.length = 0;
}

static void mfloat_scratch_release_slot(mfloat_scratch_slot_t *slot)
{
    free(slot->mantissa.storage);
    slot->mantissa.storage = NULL;
    slot->mantissa.sign = 0;
    slot->mantissa.length = 0;
    slot->mantissa.capacity = 0;
}

static int mfloat_scratch_copy(mfloat_t *dst, const mfloat_t *src)
{
    if (!dst || !src || !dst->mantissa || !src->mantissa)
        return -1;
    if (mint_copy_value(dst->mantissa, src->mantissa) != 0)
        return -1;
    dst->kind = src->kind;
    dst->sign = src->sign;
    dst->exponent2 = src->exponent2;
    return 0;
}

static int mfloat_set_from_const_mint_local(mfloat_t *dst, const mint_t *src, long exponent2)
{
    mint_t *tmp;
    int rc = -1;

    if (!dst || !src)
        return -1;
    tmp = mi_clone(src);
    if (!tmp)
        return -1;
    rc = mfloat_set_from_signed_mint(dst, tmp, exponent2);
    mi_free(tmp);
    return rc;
}

static int mfloat_make_const_rational_local(mfloat_t *dst, const mint_t *num, const mint_t *den, size_t precision)
{
    mfloat_t *n = NULL;
    mfloat_t *d = NULL;
    int rc = -1;

    if (!dst || !num || !den)
        return -1;
    n = mf_new_prec(precision);
    d = mf_new_prec(precision);
    if (!n || !d ||
        mfloat_set_from_const_mint_local(n, num, 0) != 0 ||
        mfloat_set_from_const_mint_local(d, den, 0) != 0 ||
        mf_div(n, d) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, n);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(n);
    mf_free(d);
    return rc;
}

static int mfloat_copy_lgamma_asymptotic_term_local(mfloat_t *dst, size_t index, size_t precision)
{
    if (!dst || index >= sizeof(mfloat_lgamma_asymptotic_terms) / sizeof(mfloat_lgamma_asymptotic_terms[0]))
        return -1;
    if (precision <= 512u && mfloat_lgamma_asymptotic_1024[index])
        return mfloat_set_from_immortal_internal(dst, mfloat_lgamma_asymptotic_1024[index], precision);
    return mfloat_make_const_rational_local(dst,
                                            mfloat_lgamma_asymptotic_terms[index].num,
                                            mfloat_lgamma_asymptotic_terms[index].den,
                                            precision);
}

static int mfloat_mul_euler_gamma_coeff_local(mfloat_t *mfloat, size_t index, size_t precision)
{
    mfloat_t *factor = NULL;
    int rc = -1;

    if (!mfloat || index >= sizeof(mfloat_euler_gamma_coeffs) / sizeof(mfloat_euler_gamma_coeffs[0]))
        return -1;
    factor = mf_new_prec(precision);
    if (!factor ||
        mfloat_make_const_rational_local(factor,
                                         mfloat_euler_gamma_coeffs[index].num,
                                         mfloat_euler_gamma_coeffs[index].den,
                                         precision) != 0)
        goto cleanup;
    rc = mf_mul(mfloat, factor);

cleanup:
    mf_free(factor);
    return rc;
}

static int mfloat_get_exact_long_value(const mfloat_t *mfloat, long *out)
{
    long mant_long;
    unsigned long magnitude;
    long value;

    if (!mfloat || !out || !mfloat_is_finite(mfloat))
        return 0;
    if (mf_is_zero(mfloat)) {
        *out = 0;
        return 1;
    }
    if (mfloat->exponent2 < 0)
        return 0;
    if (!mi_fits_long(mfloat->mantissa) || !mi_get_long(mfloat->mantissa, &mant_long))
        return 0;
    if (mant_long < 0)
        return 0;
    magnitude = (unsigned long)mant_long;
    if (mfloat->exponent2 >= (long)(sizeof(unsigned long) * CHAR_BIT))
        return 0;
    if (mfloat->sign < 0) {
        if (mfloat->exponent2 > 0 &&
            magnitude > ((((unsigned long)LONG_MAX) + 1u) >> mfloat->exponent2))
            return 0;
        value = (long)(magnitude << mfloat->exponent2);
        if (value > 0)
            return 0;
        *out = value;
        return 1;
    }
    if (mfloat->exponent2 > 0 && magnitude > ((unsigned long)LONG_MAX >> mfloat->exponent2))
        return 0;
    *out = (long)(magnitude << mfloat->exponent2);
    return 1;
}

static int mfloat_equals_exact_long(const mfloat_t *mfloat, long value)
{
    long actual;

    return mfloat_get_exact_long_value(mfloat, &actual) && actual == value;
}

static int mfloat_is_exact_half(const mfloat_t *mfloat, short sign)
{
    long mantissa_value;

    if (!mfloat || !mfloat_is_finite(mfloat) || mfloat->sign != sign || mfloat->exponent2 != -1)
        return 0;
    if (!mi_fits_long(mfloat->mantissa) || !mi_get_long(mfloat->mantissa, &mantissa_value))
        return 0;
    return mantissa_value == 1;
}

static int mfloat_equals_const_value(const mfloat_t *mfloat, const mfloat_t *constant)
{
    mfloat_t *tmp = NULL;
    int ok = 0;

    if (!mfloat || !constant)
        return 0;
    tmp = mfloat_clone_immortal_prec_internal(constant, mfloat->precision);
    if (!tmp)
        return 0;
    ok = mf_eq(mfloat, tmp);
    mf_free(tmp);
    return ok;
}

static int mfloat_is_near_const_value(const mfloat_t *mfloat, const mfloat_t *constant, long bits)
{
    mfloat_t *tmp = NULL;
    int ok = 0;

    if (!mfloat || !constant)
        return 0;
    tmp = mfloat_clone_immortal_prec_internal(constant, mfloat->precision);
    if (!tmp)
        return 0;
    if (mf_sub(tmp, mfloat) == 0 && mf_abs(tmp) == 0)
        ok = mfloat_is_below_neg_bits(tmp, bits);
    mf_free(tmp);
    return ok;
}

static int mfloat_set_from_binary_seed(mfloat_t *dst, const mfloat_t *seed, size_t precision)
{
    mfloat_t *tmp = NULL;
    int rc = -1;

    if (!dst || !seed || precision == 0 || precision > seed->precision)
        return -1;
    tmp = mfloat_clone_prec(seed, seed->precision);
    if (!tmp)
        return -1;
    if (mfloat_round_to_precision(tmp, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(tmp);
    return rc;
}

static mfloat_t *mfloat_new_from_long_prec(long value, size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if (mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_from_qfloat_prec(qfloat_t value, size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if (mf_set_qfloat(mfloat, value) != 0 || mf_set_precision(mfloat, precision) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= MF_PI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_PI, precision)
            : mfloat_compute_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_euler_gamma_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    /* Internal series code needs Euler's constant at the actual working precision. */
    if (!mfloat)
        return NULL;
    if ((precision <= MF_EULER_MASCHERONI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_EULER_MASCHERONI, precision)
            : mfloat_compute_euler_gamma(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_sqrt_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= MF_SQRT_PI->precision
            ? mfloat_set_from_binary_seed(mfloat, MF_SQRT_PI, precision)
            : mfloat_compute_sqrt_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static mfloat_t *mfloat_new_half_ln_pi_prec(size_t precision)
{
    mfloat_t *mfloat = mf_new_prec(precision);

    if (!mfloat)
        return NULL;
    if ((precision <= mfloat_half_ln_pi1024_static.precision
            ? mfloat_set_from_binary_seed(mfloat, &mfloat_half_ln_pi1024_static, precision)
            : mfloat_compute_half_ln_pi(mfloat, precision)) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

static int mfloat_div_long_inplace(mfloat_t *mfloat, long value)
{
    mint_t *num = NULL, *q = NULL;
    size_t shift_bits;
    size_t odd_bits;
    long exponent2;
    long rem = 0;
    unsigned long magnitude;
    unsigned long odd_part;
    unsigned int shift_twos = 0u;

    if (!mfloat || value == 0 || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat)) {
        if (mfloat->kind == MFLOAT_KIND_NAN)
            return 0;
        return mf_set_double(mfloat, ((value < 0) ^ (mfloat->sign < 0)) ? -INFINITY : INFINITY);
    }
    if (mf_is_zero(mfloat))
        return 0;
    if (value == 1)
        return 0;
    if (value == -1) {
        mfloat->sign = (short)-mfloat->sign;
        return 0;
    }

    magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1ul
                          : (unsigned long)value;
    odd_part = magnitude;
    while ((odd_part & 1ul) == 0ul) {
        odd_part >>= 1;
        shift_twos++;
    }

    if (shift_twos > 0u)
        mfloat->exponent2 -= (long)shift_twos;
    if (odd_part == 1ul) {
        if (value < 0)
            mfloat->sign = (short)-mfloat->sign;
        return 0;
    }

    num = mi_clone(mfloat->mantissa);
    q = mi_new();
    if (!num || !q)
        goto cleanup;

    odd_bits = (size_t)(sizeof(unsigned long) * CHAR_BIT - __builtin_clzl(odd_part));
    shift_bits = mfloat->precision + odd_bits + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(num, (long)shift_bits) != 0)
        goto cleanup;
    if (mi_div_long(num, (long)odd_part, &rem) != 0)
        goto cleanup;
    if (mint_set_magnitude_u64(q, (uint64_t)(rem < 0 ? -rem : rem), 1) != 0)
        goto cleanup;
    if (mi_mul_long(q, 2) != 0)
        goto cleanup;
    if (mi_cmp_long(q, (long)odd_part) >= 0) {
        if (mi_inc(num) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, num) != 0)
        goto cleanup;

    exponent2 = mfloat->exponent2 - (long)shift_bits;
    if (value < 0)
        mfloat->sign = (short)-mfloat->sign;
    mfloat->exponent2 = exponent2;
    {
        int rc = mfloat_normalise(mfloat);
        mi_free(num);
        mi_free(q);
        return rc;
    }

cleanup:
    mi_free(num);
    mi_free(q);
    return -1;
}

static int mfloat_compute_sqrt_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = mfloat_new_pi_prec(precision);
    int rc = -1;

    if (!pi)
        return -1;
    if (mf_sqrt(pi) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_compute_half_ln_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = mfloat_new_pi_prec(precision);
    int rc = -1;

    if (!pi)
        return -1;
    if (mf_log(pi) != 0 || mfloat_div_long_inplace(pi, 2) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_mul_long_inplace(mfloat_t *mfloat, long value)
{
    return mf_mul_long(mfloat, value);
}

static int mfloat_add_signed_rational_multiple(mfloat_t *sum,
                                               const mfloat_t *factor,
                                               long num,
                                               long den,
                                               int sign,
                                               long mult,
                                               size_t precision)
{
    mfloat_t *term = NULL;
    int rc = -1;

    if (!sum || !factor || den == 0 || mult == 0)
        return -1;
    term = mfloat_clone_prec(factor, precision);
    if (!term)
        goto cleanup;
    if (mult != 1 && mf_mul_long(term, mult) != 0)
        goto cleanup;
    if (num != 1 && mf_mul_long(term, num) != 0)
        goto cleanup;
    if (mfloat_div_long_inplace(term, den) != 0)
        goto cleanup;
    if (sign < 0 && mf_neg(term) != 0)
        goto cleanup;
    rc = mf_add(sum, term);

cleanup:
    mf_free(term);
    return rc;
}

static int mfloat_add_half_ln_2pi(mfloat_t *sum, size_t precision)
{
    mfloat_t *half_ln_2pi = NULL;
    int rc = -1;

    if (!sum)
        goto cleanup;
    half_ln_2pi = mfloat_clone_immortal_prec_internal(&mfloat_half_ln_2pi1024_static, precision);
    if (!half_ln_2pi)
        goto cleanup;
    if (mf_add(sum, half_ln_2pi) != 0)
        goto cleanup;
    rc = 0;

cleanup:
    mf_free(half_ln_2pi);
    return rc;
}

static int mfloat_copy_cached_lgamma_asymptotic_term(mfloat_t *dst, size_t index, size_t precision)
{
    if (!dst || index >= MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT)
        return -1;
    return mfloat_copy_lgamma_asymptotic_term_local(dst, index, precision);
}

static int mfloat_is_near_integer_pole(const mfloat_t *x)
{
    double xd;

    if (!x || !mfloat_is_finite(x))
        return 0;
    xd = mf_to_double(x);
    return xd <= 0.0 && fabs(xd - nearbyint(xd)) < 1e-15;
}

static int mfloat_lgamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *logx = NULL, *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    logx = mfloat_clone_prec(x, precision);
    sum = mfloat_clone_prec(x, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!logx || !sum || !xi)
        goto cleanup;
    if (mf_log(logx) != 0)
        goto cleanup;
    if (mf_sub(sum, MF_HALF) != 0)
        goto cleanup;
    if (mf_mul(sum, logx) != 0 || mf_sub(sum, x) != 0)
        goto cleanup;
    if (mfloat_add_half_ln_2pi(sum, precision) != 0)
        goto cleanup;

    if (mf_div(xi, x) != 0)
        goto cleanup;
    xi2 = mf_clone(xi);
    xpow = mf_new_prec(precision);
    term = mf_new_prec(precision);
    if (!xi2 || !xpow || !term)
        goto cleanup;
    if (mf_mul(xi2, xi) != 0)
        goto cleanup;

    if (mfloat_copy_cached_lgamma_asymptotic_term(term,
                                                  MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT - 1u,
                                                  precision) != 0)
        goto cleanup;
    for (size_t i = MFLOAT_LGAMMA_ASYMPTOTIC_TERM_COUNT - 1u; i-- > 0u;) {
        if (mf_mul(term, xi2) != 0 ||
            mfloat_copy_cached_lgamma_asymptotic_term(xpow, i, precision) != 0 ||
            mf_add(term, xpow) != 0)
            goto cleanup;
    }
    if (mf_mul(term, xi) != 0 || mf_add(sum, term) != 0)
        goto cleanup;

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(logx);
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_digamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 12, -1, 1},
        {1, 120, 1, 1},
        {1, 252, -1, 1},
        {1, 240, 1, 1},
        {1, 132, -1, 1},
        {691, 32760, 1, 1},
        {1, 12, -1, 1},
        {3617, 8160, 1, 1}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(x, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_log(sum) != 0 || mf_div(xi, x) != 0)
        goto cleanup;

    term = mf_clone(xi);
    if (!term || mfloat_div_long_inplace(term, 2) != 0 || mf_sub(sum, term) != 0)
        goto cleanup;
    mf_free(term);
    term = NULL;

    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0)
        goto cleanup;
    xpow = mf_clone(xi2);
    if (!xpow)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_trigamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 6, 1, 1},
        {1, 30, -1, 1},
        {1, 42, 1, 1},
        {1, 30, -1, 1},
        {5, 66, 1, 1},
        {691, 2730, -1, 1},
        {7, 6, 1, 1},
        {3617, 510, -1, 1}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL, *term = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_div(sum, x) != 0 || mf_div(xi, x) != 0)
        goto cleanup;

    term = mf_clone(xi);
    if (!term || mf_mul(term, xi) != 0 || mfloat_div_long_inplace(term, 2) != 0 ||
        mf_add(sum, term) != 0)
        goto cleanup;
    mf_free(term);
    term = NULL;

    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0)
        goto cleanup;
    xpow = mf_clone(xi2);
    if (!xpow || mf_mul(xpow, xi) != 0)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    mf_free(term);
    return rc;
}

static int mfloat_tetragamma_asymptotic(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    static const mfloat_asymp_term_t terms[] = {
        {1, 6, 1, 3},
        {1, 30, -1, 5},
        {1, 42, 1, 7},
        {1, 30, -1, 9},
        {5, 66, 1, 11},
        {691, 2730, -1, 13},
        {7, 6, 1, 15},
        {3617, 510, -1, 17},
        {43867, 798, 1, 19},
        {174611, 330, -1, 21},
        {854513, 138, 1, 23},
        {236364091, 2730, -1, 25},
        {8553103, 6, 1, 27},
        {23749461029LL, 870, -1, 29},
        {8615841276005LL, 14322, 1, 31},
        {7709321041217LL, 510, -1, 33},
        {2577687858367LL, 6, 1, 35}
    };
    mfloat_t *sum = NULL, *xi = NULL, *xi2 = NULL, *xpow = NULL;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    xi = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !xi)
        goto cleanup;
    if (mf_div(sum, x) != 0 || mf_div(xi, x) != 0 || mf_mul(sum, xi) != 0)
        goto cleanup;

    xpow = mf_clone(sum);
    if (!xpow || mf_mul(xpow, xi) != 0 || mf_add(sum, xpow) != 0)
        goto cleanup;
    xi2 = mf_clone(xi);
    if (!xi2 || mf_mul(xi2, xi) != 0 || mf_mul(xpow, xi) != 0)
        goto cleanup;

    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (mfloat_add_signed_rational_multiple(sum, xpow,
                                                terms[i].num, terms[i].den,
                                                terms[i].sign, terms[i].mult,
                                                precision) != 0)
            goto cleanup;
        if (mf_mul(xpow, xi2) != 0)
            goto cleanup;
    }
    if (mf_neg(sum) != 0)
        goto cleanup;

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(xi);
    mf_free(xi2);
    mf_free(xpow);
    return rc;
}

static int mfloat_eval_reflected_gamma_family(mfloat_t *dst,
                                              const mfloat_t *x,
                                              size_t precision,
                                              int order)
{
    mfloat_t *one_minus_x = NULL, *pix = NULL, *sinpix = NULL, *cospix = NULL;
    mfloat_t *poly = NULL, *tmp = NULL;
    int rc = -1;

    one_minus_x = mfloat_clone_prec(MF_ONE, precision);
    pix = mfloat_new_pi_prec(precision);
    if (!one_minus_x || !pix)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_mul(pix, x) != 0)
        goto cleanup;

    if (order == 0) {
        if (mf_digamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        cospix = mf_clone(pix);
        if (!sinpix || !cospix || mf_sin(sinpix) != 0 || mf_cos(cospix) != 0 ||
            mf_div(cospix, sinpix) != 0 || mf_mul(cospix, pix) != 0 ||
            mf_sub(one_minus_x, cospix) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, one_minus_x);
        if (rc == 0)
            dst->precision = precision;
        goto cleanup;
    }

    if (order == 1) {
        if (mf_trigamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        if (!sinpix || mf_sin(sinpix) != 0 || mf_mul(sinpix, sinpix) != 0)
            goto cleanup;
        tmp = mf_clone(pix);
        if (!tmp || mf_mul(tmp, pix) != 0 || mf_div(tmp, sinpix) != 0 ||
            mf_sub(tmp, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, tmp);
        if (rc == 0)
            dst->precision = precision;
        goto cleanup;
    }

    if (order == 2) {
        if (mf_tetragamma(one_minus_x) != 0)
            goto cleanup;
        sinpix = mf_clone(pix);
        cospix = mf_clone(pix);
        poly = mf_clone(pix);
        tmp = mf_clone(pix);
        if (!sinpix || !cospix || !poly || !tmp ||
            mf_sin(sinpix) != 0 || mf_cos(cospix) != 0)
            goto cleanup;
        if (mf_mul(tmp, pix) != 0 || mf_mul(tmp, pix) != 0 || mf_mul_long(tmp, 2) != 0)
            goto cleanup;
        if (mf_mul(poly, cospix) != 0 || mf_mul(poly, tmp) != 0)
            goto cleanup;
        if (mf_mul(sinpix, sinpix) != 0 || mf_mul(sinpix, sinpix) != 0 ||
            mf_div(poly, sinpix) != 0 || mf_add(poly, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_copy_value(dst, poly);
        if (rc == 0)
            dst->precision = precision;
    }

cleanup:
    mf_free(one_minus_x);
    mf_free(pix);
    mf_free(sinpix);
    mf_free(cospix);
    mf_free(poly);
    mf_free(tmp);
    return rc;
}

static int mfloat_set_factorial(mfloat_t *dst, long n, size_t precision)
{
    mfloat_t *value = NULL;
    int rc = -1;

    if (!dst || n < 0)
        return -1;
    value = mfloat_clone_prec(MF_ONE, precision);
    if (!value)
        return -1;
    for (long k = 2; k <= n; ++k) {
        if (mf_mul_long(value, k) != 0)
            goto cleanup;
    }
    rc = mfloat_copy_value(dst, value);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(value);
    return rc;
}

static int mfloat_compute_two_over_sqrt_pi(mfloat_t *dst, size_t precision)
{
    mfloat_t *pi = NULL;
    int rc = -1;

    if (!dst)
        return -1;
    pi = mfloat_new_pi_prec(precision);
    if (!pi || mf_sqrt(pi) != 0 || mf_inv(pi) != 0 || mf_mul_long(pi, 2) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pi);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(pi);
    return rc;
}

static int mfloat_ei_series(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *piece = NULL, *abs_x = NULL, *log_abs_x = NULL, *gamma = NULL;
    int rc = -1;

    if (!dst || !x)
        return -1;
    if (mf_is_zero(x))
        return mf_set_double(dst, -INFINITY);

    sum = mfloat_clone_prec(MF_ZERO, precision);
    term = mfloat_clone_prec(x, precision);
    abs_x = mfloat_clone_prec(x, precision);
    if (!sum || !term || !abs_x || mf_abs(abs_x) != 0)
        goto cleanup;
    log_abs_x = mf_clone(abs_x);
    gamma = mfloat_new_euler_gamma_prec(precision);
    if (!log_abs_x || !gamma || mf_log(log_abs_x) != 0 ||
        mf_add(sum, gamma) != 0 || mf_add(sum, log_abs_x) != 0)
        goto cleanup;

    for (long k = 1; k < 4096; ++k) {
        piece = mf_clone(term);
        if (!piece || mfloat_div_long_inplace(piece, k) != 0 || mf_add(sum, piece) != 0)
            goto cleanup;
        mf_free(piece);
        piece = NULL;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
        if (mf_mul(term, x) != 0 || mfloat_div_long_inplace(term, k + 1) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(piece);
    mf_free(abs_x);
    mf_free(log_abs_x);
    mf_free(gamma);
    return rc;
}

static int mfloat_e1_series_positive(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *piece = NULL, *log_x = NULL, *gamma = NULL;
    int rc = -1;

    if (!dst || !x)
        return -1;
    if (mf_le(x, MF_ZERO))
        return mf_set_double(dst, NAN);

    sum = mfloat_clone_prec(MF_ZERO, precision);
    term = mfloat_clone_prec(x, precision);
    if (!sum || !term || mf_neg(term) != 0)
        goto cleanup;
    log_x = mfloat_clone_prec(x, precision);
    gamma = mfloat_new_euler_gamma_prec(precision);
    if (!log_x || !gamma || mf_log(log_x) != 0 ||
        mf_sub(sum, gamma) != 0 || mf_sub(sum, log_x) != 0)
        goto cleanup;

    for (long k = 1; k < 4096; ++k) {
        piece = mf_clone(term);
        if (!piece || mfloat_div_long_inplace(piece, k) != 0 || mf_sub(sum, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)precision + 32l)) {
            mf_free(piece);
            piece = NULL;
            break;
        }
        mf_free(piece);
        piece = NULL;
        if (mf_mul(term, x) != 0 || mf_neg(term) != 0 || mfloat_div_long_inplace(term, k + 1) != 0)
            goto cleanup;
    }

    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(piece);
    mf_free(log_x);
    mf_free(gamma);
    return rc;
}

static int mfloat_gammainc_series_P(mfloat_t *dst,
                                    const mfloat_t *s,
                                    const mfloat_t *x,
                                    size_t precision)
{
    mfloat_t *lgamma_s = NULL, *log_x = NULL, *pref = NULL;
    mfloat_t *sum = NULL, *term = NULL, *nq = NULL, *spn = NULL, *tol = NULL;
    int rc = -1;

    lgamma_s = mfloat_clone_prec(s, precision);
    log_x = mfloat_clone_prec(x, precision);
    if (!lgamma_s || !log_x)
        goto cleanup;
    if (mf_lgamma(lgamma_s) != 0 || mf_log(log_x) != 0)
        goto cleanup;

    pref = mf_clone(log_x);
    if (!pref || mf_mul(pref, s) != 0 || mf_sub(pref, x) != 0 || mf_sub(pref, lgamma_s) != 0 ||
        mf_exp(pref) != 0)
        goto cleanup;

    sum = mfloat_clone_prec(MF_ONE, precision);
    term = mfloat_clone_prec(MF_ONE, precision);
    if (!sum || !term || mf_div(sum, s) != 0 || mf_div(term, s) != 0)
        goto cleanup;
    tol = mfloat_clone_prec(MF_ONE, precision);
    if (!tol || mf_ldexp(tol, -(int)precision) != 0)
        goto cleanup;

    for (int n = 1; n < 2000; ++n) {
        nq = mfloat_new_from_long_prec(n, precision);
        spn = mf_clone(s);
        if (!nq || !spn || mf_add(spn, nq) != 0 || mf_mul(term, x) != 0 ||
            mf_div(term, spn) != 0 || mf_div(term, nq) != 0 || mf_add(sum, term) != 0)
            goto cleanup;
        if (mf_abs(term) != 0)
            goto cleanup;
        if (mf_le(term, tol))
            break;
        mf_free(nq);
        mf_free(spn);
        nq = spn = NULL;
    }

    if (mf_mul(pref, sum) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pref);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(lgamma_s);
    mf_free(log_x);
    mf_free(pref);
    mf_free(sum);
    mf_free(term);
    mf_free(nq);
    mf_free(spn);
    mf_free(tol);
    return rc;
}

static int mfloat_gammainc_cf_Q(mfloat_t *dst,
                                const mfloat_t *s,
                                const mfloat_t *x,
                                size_t precision)
{
    mfloat_t *lgamma_s = NULL, *log_x = NULL, *pref = NULL;
    mfloat_t *tiny = NULL, *zero = NULL, *one = NULL;
    mfloat_t *C = NULL, *D = NULL, *f = NULL;
    mfloat_t *a = NULL, *b = NULL, *delta = NULL;
    mfloat_t *nq = NULL, *tmp = NULL, *smn = NULL;
    int rc = -1;

    lgamma_s = mfloat_clone_prec(s, precision);
    log_x = mfloat_clone_prec(x, precision);
    one = mfloat_clone_prec(MF_ONE, precision);
    zero = mfloat_clone_prec(MF_ZERO, precision);
    tiny = mf_new_prec(precision);
    if (tiny && (mf_set_long(tiny, 1) != 0 || mf_mul_pow10(tiny, -300) != 0)) {
        mf_free(tiny);
        tiny = NULL;
    }
    if (!lgamma_s || !log_x || !one || !zero || !tiny)
        goto cleanup;
    if (mf_lgamma(lgamma_s) != 0 || mf_log(log_x) != 0)
        goto cleanup;

    pref = mf_clone(log_x);
    if (!pref || mf_mul(pref, s) != 0 || mf_sub(pref, x) != 0 || mf_sub(pref, lgamma_s) != 0 ||
        mf_exp(pref) != 0)
        goto cleanup;

    C = mf_clone(tiny);
    D = mf_clone(zero);
    f = mfloat_clone_prec(MF_ONE, precision);
    if (!C || !D || !f)
        goto cleanup;

    for (int n = 1; n < 2000; ++n) {
        nq = mfloat_new_from_long_prec(n, precision);
        smn = mf_clone(s);
        a = mf_clone(nq);
        if (!nq || !smn || !a)
            goto cleanup;
        if (mf_sub(smn, nq) != 0 || mf_mul(a, smn) != 0)
            goto cleanup;

        b = mfloat_clone_prec(x, precision);
        tmp = mf_clone(nq);
        if (!b || !tmp || mf_mul_long(tmp, 2) != 0 || mf_add(b, tmp) != 0 || mf_sub(b, s) != 0)
            goto cleanup;

        if (mf_mul(a, D) != 0 || mf_add(a, b) != 0)
            goto cleanup;
        mf_free(D);
        D = a;
        a = NULL;
        if (mf_eq(D, zero) && mfloat_copy_value(D, tiny) != 0)
            goto cleanup;
        if (mf_inv(D) != 0)
            goto cleanup;

        a = mf_clone(nq);
        if (!a || mfloat_copy_value(a, smn) != 0)
            goto cleanup;
        if (mf_mul(a, nq) != 0 || mf_div(a, C) != 0 || mf_add(a, b) != 0)
            goto cleanup;
        mf_free(C);
        C = a;
        a = NULL;
        if (mf_eq(C, zero) && mfloat_copy_value(C, tiny) != 0)
            goto cleanup;

        delta = mf_clone(C);
        if (!delta || mf_mul(delta, D) != 0 || mf_mul(f, delta) != 0)
            goto cleanup;
        if (mf_sub(delta, one) != 0 || mf_abs(delta) != 0 || mf_le(delta, tiny)) {
            mf_free(a);
            mf_free(b);
            mf_free(delta);
            mf_free(nq);
            mf_free(tmp);
            mf_free(smn);
            a = b = delta = nq = tmp = smn = NULL;
            break;
        }

        mf_free(a);
        mf_free(b);
        mf_free(delta);
        mf_free(nq);
        mf_free(tmp);
        mf_free(smn);
        a = b = delta = nq = tmp = smn = NULL;
    }

    if (mf_mul(pref, f) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, pref);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(lgamma_s);
    mf_free(log_x);
    mf_free(pref);
    mf_free(tiny);
    mf_free(zero);
    mf_free(one);
    mf_free(C);
    mf_free(D);
    mf_free(f);
    mf_free(a);
    mf_free(b);
    mf_free(delta);
    mf_free(nq);
    mf_free(tmp);
    mf_free(smn);
    return rc;
}

static int mfloat_round_to_precision(mfloat_t *mfloat, size_t precision)
{
    return mfloat_round_to_precision_internal(mfloat, precision);
}

static int mfloat_is_below_neg_bits(const mfloat_t *mfloat, long bits)
{
    size_t mant_bits;
    long top_bit;

    if (!mfloat || mf_is_zero(mfloat))
        return 1;

    mant_bits = mf_get_mantissa_bits(mfloat);
    top_bit = mfloat->exponent2 + (long)mant_bits - 1l;
    return top_bit < -bits;
}

static long mfloat_estimate_positive_unit_steps(const mfloat_t *value, long threshold)
{
    double x, delta;
    long steps;

    if (!value || !mfloat_is_finite(value))
        return -1;
    x = mf_to_double(value);
    if (!isfinite(x) || x >= (double)threshold)
        return 0;
    delta = (double)threshold - x;
    steps = (long)delta;
    if ((double)steps < delta)
        steps++;
    return steps;
}

static long mfloat_lgamma_asymptotic_threshold(size_t precision)
{
    if (precision <= 256u)
        return 32l;
    if (precision <= 512u)
        return 512l;
    if (precision <= 1024u)
        return 1024l;
    return 128l;
}

static int mfloat_compute_e(mfloat_t *dst, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL;
    long k;
    int rc = -1;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;

    sum = mfloat_clone_prec(MF_ONE, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    if (!sum || !term)
        goto cleanup;

    for (k = 1; k < LONG_MAX; ++k) {
        if (mfloat_div_long_inplace(term, k) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    return rc;
}

static mint_t *mfloat_compute_arctan_inverse_scaled(long q, size_t bits)
{
    mint_t *sum = NULL, *term = NULL, *next = NULL;
    long q2;
    long k;

    if (q <= 1 || bits == 0 || q > 3037000499l)
        return NULL;
    q2 = q * q;

    term = mi_create_2pow((uint64_t)bits);
    if (!term)
        return NULL;
    if (mi_div_long(term, q, NULL) != 0)
        goto fail;

    sum = mi_clone(term);
    if (!sum)
        goto fail;

    for (k = 0; k < LONG_MAX / 2; ++k) {
        long num = 2l * k + 1l;
        long den = 2l * k + 3l;

        next = mi_clone(term);
        if (!next)
            goto fail;
        if (mi_abs(next) != 0)
            goto fail;
        if (mi_mul_long(next, num) != 0 ||
            mi_div_long(next, q2, NULL) != 0 ||
            mi_div_long(next, den, NULL) != 0)
            goto fail;
        if (mi_is_zero(next)) {
            mi_free(next);
            next = NULL;
            break;
        }
        if (!mi_is_negative(term)) {
            if (mi_neg(next) != 0)
                goto fail;
        }
        if (mi_add(sum, next) != 0)
            goto fail;
        mi_free(term);
        term = next;
        next = NULL;
    }

    mi_free(term);
    return sum;

fail:
    mi_free(sum);
    mi_free(term);
    mi_free(next);
    return NULL;
}

static int mfloat_compute_pi(mfloat_t *dst, size_t precision)
{
    mint_t *a = NULL, *b = NULL;
    long exponent2;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    int rc = -1;

    a = mfloat_compute_arctan_inverse_scaled(5, work_prec);
    b = mfloat_compute_arctan_inverse_scaled(239, work_prec);
    if (!a || !b)
        goto cleanup;
    if (mi_mul_long(a, 16) != 0 || mi_mul_long(b, 4) != 0 || mi_sub(a, b) != 0)
        goto cleanup;
    exponent2 = -(long)work_prec;
    if (mfloat_set_from_signed_mint(dst, a, exponent2) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(dst, precision) != 0)
        goto cleanup;
    dst->precision = precision;
    rc = 0;

cleanup:
    mi_free(a);
    mi_free(b);
    return rc;
}

static int mfloat_sin_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *r2 = NULL;
    long n;
    int rc = -1;

    sum = mfloat_clone_prec(x, precision);
    term = mfloat_clone_prec(x, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sum || !term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long a = 2l * n;
        long b = 2l * n + 1l;

        if (mf_mul(term, r2) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, a) != 0 ||
            mfloat_div_long_inplace(term, b) != 0 ||
            mf_neg(term) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(r2);
    return rc;
}

static int mfloat_cos_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *sum = NULL, *term = NULL, *r2 = NULL;
    long n;
    int rc = -1;

    sum = mfloat_clone_prec(MF_ONE, precision);
    term = mfloat_clone_prec(MF_ONE, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sum || !term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long a = 2l * n - 1l;
        long b = 2l * n;

        if (mf_mul(term, r2) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, a) != 0 ||
            mfloat_div_long_inplace(term, b) != 0 ||
            mf_neg(term) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(r2);
    return rc;
}

static int mfloat_sincos_kernel_pair(mfloat_t *sin_dst, mfloat_t *cos_dst,
                                     const mfloat_t *x, size_t precision)
{
    mfloat_t *sin_sum = NULL, *sin_term = NULL, *cos_sum = NULL, *cos_term = NULL, *r2 = NULL;
    long n;
    int rc = -1;
    size_t out_prec = precision - MFLOAT_CONST_GUARD_BITS;

    sin_sum = mfloat_clone_prec(x, precision);
    sin_term = mfloat_clone_prec(x, precision);
    cos_sum = mfloat_clone_prec(MF_ONE, precision);
    cos_term = mfloat_clone_prec(MF_ONE, precision);
    r2 = mfloat_clone_prec(x, precision);
    if (!sin_sum || !sin_term || !cos_sum || !cos_term || !r2)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long sin_a = 2l * n;
        long sin_b = 2l * n + 1l;
        long cos_a = 2l * n - 1l;
        long cos_b = 2l * n;
        int sin_small, cos_small;

        if (mf_mul(sin_term, r2) != 0 ||
            mfloat_div_long_inplace(sin_term, sin_a) != 0 ||
            mfloat_div_long_inplace(sin_term, sin_b) != 0 ||
            mf_neg(sin_term) != 0)
            goto cleanup;
        if (mf_add(sin_sum, sin_term) != 0)
            goto cleanup;

        if (mf_mul(cos_term, r2) != 0 ||
            mfloat_div_long_inplace(cos_term, cos_a) != 0 ||
            mfloat_div_long_inplace(cos_term, cos_b) != 0 ||
            mf_neg(cos_term) != 0)
            goto cleanup;
        if (mf_add(cos_sum, cos_term) != 0)
            goto cleanup;

        sin_small = mfloat_is_below_neg_bits(sin_term, (long)precision + 8l);
        cos_small = mfloat_is_below_neg_bits(cos_term, (long)precision + 8l);
        if (sin_small && cos_small)
            break;
    }

    if (mfloat_round_to_precision(sin_sum, out_prec) != 0 ||
        mfloat_round_to_precision(cos_sum, out_prec) != 0)
        goto cleanup;
    if (mfloat_copy_value(sin_dst, sin_sum) != 0 || mfloat_copy_value(cos_dst, cos_sum) != 0)
        goto cleanup;
    sin_dst->precision = out_prec;
    cos_dst->precision = out_prec;
    rc = 0;

cleanup:
    mf_free(sin_sum);
    mf_free(sin_term);
    mf_free(cos_sum);
    mf_free(cos_term);
    mf_free(r2);
    return rc;
}

typedef int (*mfloat_trig_kernel_fn)(mfloat_t *dst, const mfloat_t *x, size_t precision);

typedef struct mfloat_trig_dispatch_t {
    mfloat_trig_kernel_fn kernel;
    bool negate_result;
} mfloat_trig_dispatch_t;

static int mfloat_apply_trig_dispatch(mfloat_t *dst, const mfloat_t *x, size_t precision,
                                      const mfloat_trig_dispatch_t dispatch[4], int quadrant)
{
    const mfloat_trig_dispatch_t *entry;
    int rc = -1;

    entry = &dispatch[quadrant & 3];
    rc = entry->kernel(dst, x, precision);
    if (rc == 0 && entry->negate_result)
        rc = mf_neg(dst);
    return rc;
}

static int mfloat_abs_eq(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || !mfloat_is_finite(a) || !mfloat_is_finite(b))
        return 0;
    if (mf_is_zero(a) || mf_is_zero(b))
        return mf_is_zero(a) && mf_is_zero(b);
    return a->exponent2 == b->exponent2 && mi_cmp(a->mantissa, b->mantissa) == 0;
}

static int mfloat_sincos_pair(mfloat_t *sin_dst, mfloat_t *cos_dst, const mfloat_t *x, size_t precision)
{
    mfloat_t *r = NULL;
    int quadrant;
    int rc = -1;
    mfloat_t *sin_tmp = NULL, *cos_tmp = NULL;

    if (!sin_dst || !cos_dst || !x)
        return -1;
    if (mfloat_reduce_trig_argument(x, precision, &r, &quadrant) != 0)
        goto cleanup;
    sin_tmp = mfloat_clone_prec(MF_ZERO, precision);
    cos_tmp = mfloat_clone_prec(MF_ZERO, precision);
    if (!sin_tmp || !cos_tmp)
        goto cleanup;
    if (mfloat_sincos_kernel_pair(sin_tmp, cos_tmp, r, precision) != 0)
        goto cleanup;
    switch (quadrant & 3) {
    case 0:
        break;
    case 1:
        if (mfloat_copy_value(sin_dst, cos_tmp) != 0 || mfloat_copy_value(cos_dst, sin_tmp) != 0)
            goto cleanup;
        cos_dst->precision = sin_tmp->precision;
        sin_dst->precision = cos_tmp->precision;
        if (mf_neg(cos_dst) != 0)
            goto cleanup;
        rc = 0;
        goto cleanup;
    case 2:
        if (mf_neg(sin_tmp) != 0 || mf_neg(cos_tmp) != 0)
            goto cleanup;
        break;
    case 3:
        if (mfloat_copy_value(sin_dst, cos_tmp) != 0 || mfloat_copy_value(cos_dst, sin_tmp) != 0)
            goto cleanup;
        cos_dst->precision = sin_tmp->precision;
        sin_dst->precision = cos_tmp->precision;
        if (mf_neg(sin_dst) != 0)
            goto cleanup;
        rc = 0;
        goto cleanup;
    }
    if (mfloat_copy_value(sin_dst, sin_tmp) != 0 || mfloat_copy_value(cos_dst, cos_tmp) != 0)
        goto cleanup;
    sin_dst->precision = sin_tmp->precision;
    cos_dst->precision = cos_tmp->precision;
    rc = 0;

cleanup:
    mf_free(r);
    mf_free(sin_tmp);
    mf_free(cos_tmp);
    return rc;
}

static int mfloat_sinhcosh_pair(mfloat_t *sinh_dst, mfloat_t *cosh_dst, const mfloat_t *x, size_t precision)
{
    mfloat_scratch_slot_t slots[5];
    mfloat_t *ax, *ex, *inv_ex, *sinh_tmp, *cosh_tmp;
    int rc = -1;
    size_t i;
    int negate_sinh = 0;

    if (!sinh_dst || !cosh_dst || !x)
        return -1;
    for (i = 0; i < 5u; ++i)
        mfloat_scratch_init_slot(&slots[i], precision);
    ax = &slots[0].value;
    ex = &slots[1].value;
    inv_ex = &slots[2].value;
    sinh_tmp = &slots[3].value;
    cosh_tmp = &slots[4].value;

    if (mfloat_scratch_copy(ax, x) != 0)
        goto cleanup;
    if (ax->sign < 0) {
        negate_sinh = 1;
        if (mf_abs(ax) != 0)
            goto cleanup;
    }
    if (mfloat_scratch_copy(ex, ax) != 0)
        goto cleanup;
    if (mf_exp(ex) != 0)
        goto cleanup;
    if (mfloat_scratch_copy(inv_ex, ex) != 0 || mf_inv(inv_ex) != 0)
        goto cleanup;
    if (mfloat_scratch_copy(sinh_tmp, ex) != 0 || mfloat_scratch_copy(cosh_tmp, ex) != 0)
        goto cleanup;
    if (mf_sub(sinh_tmp, inv_ex) != 0 || mfloat_div_long_inplace(sinh_tmp, 2) != 0)
        goto cleanup;
    if (negate_sinh && mf_neg(sinh_tmp) != 0)
        goto cleanup;
    if (mf_add(cosh_tmp, inv_ex) != 0 || mfloat_div_long_inplace(cosh_tmp, 2) != 0)
        goto cleanup;
    if (mfloat_copy_value(sinh_dst, sinh_tmp) != 0 || mfloat_copy_value(cosh_dst, cosh_tmp) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    for (i = 0; i < 5u; ++i)
        mfloat_scratch_release_slot(&slots[i]);
    return rc;
}

int mf_sincos(const mfloat_t *x, mfloat_t *sin_out, mfloat_t *cos_out)
{
    size_t sin_prec, cos_prec, work_prec;
    mfloat_t *sin_tmp = NULL, *cos_tmp = NULL;
    int rc = -1;

    if (!sin_out || !cos_out || !x || sin_out == cos_out)
        return -1;
    if (x->kind == MFLOAT_KIND_NAN) {
        if (mf_set_double(sin_out, NAN) != 0 || mf_set_double(cos_out, NAN) != 0)
            return -1;
        return 0;
    }
    if (!mfloat_is_finite(x)) {
        if (mf_set_double(sin_out, NAN) != 0 || mf_set_double(cos_out, NAN) != 0)
            return -1;
        return 0;
    }

    sin_prec = sin_out->precision;
    cos_prec = cos_out->precision;
    work_prec = sin_prec > cos_prec ? sin_prec : cos_prec;
    work_prec = mfloat_transcendental_work_prec(work_prec);

    sin_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    cos_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!sin_tmp || !cos_tmp)
        goto cleanup;

    if (mfloat_sincos_pair(sin_tmp, cos_tmp, x, work_prec) != 0)
        goto cleanup;
    if (mfloat_finish_result(sin_out, sin_tmp, sin_prec) != 0 ||
        mfloat_finish_result(cos_out, cos_tmp, cos_prec) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mf_free(sin_tmp);
    mf_free(cos_tmp);
    return rc;
}

int mf_sinhcosh(const mfloat_t *x, mfloat_t *sinh_out, mfloat_t *cosh_out)
{
    size_t sinh_prec, cosh_prec, work_prec;
    mfloat_t *sinh_tmp = NULL, *cosh_tmp = NULL;
    int rc = -1;

    if (!x || !sinh_out || !cosh_out || sinh_out == cosh_out)
        return -1;
    if (x->kind == MFLOAT_KIND_NAN) {
        if (mf_set_double(sinh_out, NAN) != 0 || mf_set_double(cosh_out, NAN) != 0)
            return -1;
        return 0;
    }
    if (x->kind == MFLOAT_KIND_POSINF || x->kind == MFLOAT_KIND_NEGINF) {
        if (mf_set_double(sinh_out, x->kind == MFLOAT_KIND_POSINF ? INFINITY : -INFINITY) != 0 ||
            mf_set_double(cosh_out, INFINITY) != 0)
            return -1;
        return 0;
    }

    sinh_prec = sinh_out->precision;
    cosh_prec = cosh_out->precision;
    work_prec = sinh_prec > cosh_prec ? sinh_prec : cosh_prec;
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(work_prec));

    sinh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    cosh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!sinh_tmp || !cosh_tmp)
        goto cleanup;
    if (mfloat_sinhcosh_pair(sinh_tmp, cosh_tmp, x, work_prec) != 0)
        goto cleanup;
    if (mfloat_finish_result(sinh_out, sinh_tmp, sinh_prec) != 0 ||
        mfloat_finish_result(cosh_out, cosh_tmp, cosh_prec) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mf_free(sinh_tmp);
    mf_free(cosh_tmp);
    return rc;
}

static int mfloat_atan_kernel(mfloat_t *dst, const mfloat_t *x, size_t precision)
{
    mfloat_scratch_slot_t slots[4];
    mfloat_t *sum, *term, *r2, *piece;
    long n;
    int rc = -1;
    size_t i;

    for (i = 0; i < 4u; ++i)
        mfloat_scratch_init_slot(&slots[i], precision);
    sum = &slots[0].value;
    term = &slots[1].value;
    r2 = &slots[2].value;
    piece = &slots[3].value;

    if (mfloat_scratch_copy(sum, x) != 0 ||
        mfloat_scratch_copy(term, x) != 0 ||
        mfloat_scratch_copy(r2, x) != 0)
        goto cleanup;
    if (mf_mul(r2, x) != 0)
        goto cleanup;

    for (n = 1; n < LONG_MAX / 2; ++n) {
        long denom = 2l * n + 1l;

        if (mf_mul(term, r2) != 0 || mf_neg(term) != 0)
            goto cleanup;
        if (mfloat_scratch_copy(piece, term) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(sum, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)precision + 8l))
            break;
    }

    if (mfloat_round_to_precision(sum, precision - MFLOAT_CONST_GUARD_BITS) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision - MFLOAT_CONST_GUARD_BITS;

cleanup:
    for (i = 0; i < 4u; ++i)
        mfloat_scratch_release_slot(&slots[i]);
    return rc;
}

static int mfloat_reduce_trig_argument(const mfloat_t *src,
                                       size_t precision,
                                       mfloat_t **r_out,
                                       int *quadrant_out)
{
    mfloat_t *r = NULL, *half_pi = NULL, *kterm = NULL;
    double xd, half_pid;
    long k;

    if (!src || !r_out || !quadrant_out)
        return -1;

    r = mfloat_clone_prec(src, precision);
    half_pi = mfloat_new_pi_prec(precision);
    if (!r || !half_pi)
        goto fail;
    if (mfloat_div_long_inplace(half_pi, 2) != 0)
        goto fail;

    xd = mf_to_double(r);
    half_pid = mf_to_double(half_pi);
    k = (long)nearbyint(xd / half_pid);

    kterm = mf_clone(half_pi);
    if (!kterm)
        goto fail;
    if (mf_mul_long(kterm, k) != 0)
        goto fail;
    if (mf_sub(r, kterm) != 0)
        goto fail;

    *quadrant_out = (int)(((k % 4l) + 4l) % 4l);
    *r_out = r;
    mf_free(half_pi);
    mf_free(kterm);
    return 0;

fail:
    mf_free(r);
    mf_free(half_pi);
    mf_free(kterm);
    return -1;
}

static int mfloat_finish_result(mfloat_t *dst, mfloat_t *src, size_t precision)
{
    int rc;

    if (!dst || !src)
        return -1;
    if (mfloat_round_to_precision(src, precision) != 0)
        return -1;
    rc = mfloat_copy_value(dst, src);
    if (rc == 0)
        dst->precision = precision;
    return rc;
}

static size_t mfloat_transcendental_work_prec(size_t precision)
{
    size_t work_prec = precision + 64u;

    if (work_prec < precision + MFLOAT_CONST_GUARD_BITS)
        work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    return work_prec;
}

static size_t mfloat_cap_work_prec(size_t work_prec)
{
    return work_prec > 1024u ? 1024u : work_prec;
}

static int mfloat_get_small_positive_integer(const mfloat_t *mfloat, long *out)
{
    double value;
    mfloat_t *check = NULL;
    int ok = 0;

    if (!mfloat || !out || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    value = mf_to_double(mfloat);
    if (!(value >= 1.0 && value <= (double)LONG_MAX) || floor(value) != value)
        return 0;
    check = mf_create_long((long)value);
    if (!check)
        return 0;
    ok = mf_eq(mfloat, check);
    if (ok)
        *out = (long)value;
    mf_free(check);
    return ok;
}

static int mfloat_get_small_positive_half_integer(const mfloat_t *mfloat, long *out_n)
{
    mfloat_t *twice = NULL;
    long twice_n = 0;
    int ok = 0;

    if (!mfloat || !out_n || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    twice = mfloat_clone_prec(mfloat, mfloat->precision);
    if (!twice)
        return 0;
    if (mf_mul_long(twice, 2) != 0)
        goto cleanup;
    if (!mfloat_get_small_positive_integer(twice, &twice_n) || (twice_n & 1L) == 0 || twice_n < 1)
        goto cleanup;
    *out_n = (twice_n - 1) / 2;
    ok = 1;

cleanup:
    mf_free(twice);
    return ok;
}

static int mfloat_get_small_positive_half_step(const mfloat_t *mfloat, long *out_twice_n)
{
    mfloat_t *twice = NULL;
    long twice_n = 0;
    int ok = 0;

    if (!mfloat || !out_twice_n || !mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return 0;
    twice = mfloat_clone_prec(mfloat, mfloat->precision);
    if (!twice)
        return 0;
    if (mf_mul_long(twice, 2) != 0)
        goto cleanup;
    if (!mfloat_get_small_positive_integer(twice, &twice_n) || twice_n < 1)
        goto cleanup;
    *out_twice_n = twice_n;
    ok = 1;

cleanup:
    mf_free(twice);
    return ok;
}

static int mfloat_build_small_halfstep_gamma_parts(long twice_n, size_t precision,
                                                   mfloat_t *num, mfloat_t *den, int *pi_half)
{
    long m;

    if (twice_n < 1 || !num || !den || !pi_half)
        return -1;
    if ((twice_n & 1L) == 0) {
        m = twice_n / 2;
        *pi_half = 0;
        if (m <= 1)
            return 0;
        return mfloat_set_factorial(num, m - 1, precision);
    }

    m = (twice_n - 1) / 2;
    *pi_half = 1;
    if (m == 0)
        return 0;
    if (mfloat_set_factorial(num, 2 * m, precision) != 0 || mfloat_set_factorial(den, m, precision) != 0)
        return -1;
    return mf_ldexp(den, 2 * (int)m);
}

static int mfloat_try_small_exact_beta(mfloat_t *dst, const mfloat_t *a, const mfloat_t *b, size_t precision)
{
    long twice_a, twice_b, twice_sum;
    int pi_half_a, pi_half_b, pi_half_sum, pi_factor;
    mfloat_t *num_a = NULL, *den_a = NULL, *num_b = NULL, *den_b = NULL;
    mfloat_t *num_sum = NULL, *den_sum = NULL, *value = NULL, *pi = NULL;
    int rc = -1;

    if (!mfloat_get_small_positive_half_step(a, &twice_a) || !mfloat_get_small_positive_half_step(b, &twice_b))
        return 1;
    if (twice_a > LONG_MAX - twice_b)
        return 1;
    twice_sum = twice_a + twice_b;

    num_a = mfloat_clone_prec(MF_ONE, precision);
    den_a = mfloat_clone_prec(MF_ONE, precision);
    num_b = mfloat_clone_prec(MF_ONE, precision);
    den_b = mfloat_clone_prec(MF_ONE, precision);
    num_sum = mfloat_clone_prec(MF_ONE, precision);
    den_sum = mfloat_clone_prec(MF_ONE, precision);
    value = mfloat_clone_prec(MF_ONE, precision);
    if (!num_a || !den_a || !num_b || !den_b || !num_sum || !den_sum || !value)
        goto cleanup;
    if (mfloat_build_small_halfstep_gamma_parts(twice_a, precision, num_a, den_a, &pi_half_a) != 0 ||
        mfloat_build_small_halfstep_gamma_parts(twice_b, precision, num_b, den_b, &pi_half_b) != 0 ||
        mfloat_build_small_halfstep_gamma_parts(twice_sum, precision, num_sum, den_sum, &pi_half_sum) != 0)
        goto cleanup;

    if (mf_mul(value, num_a) != 0 || mf_mul(value, num_b) != 0 || mf_mul(value, den_sum) != 0 ||
        mf_div(value, den_a) != 0 || mf_div(value, den_b) != 0 || mf_div(value, num_sum) != 0)
        goto cleanup;

    pi_factor = pi_half_a + pi_half_b - pi_half_sum;
    if (pi_factor == 2) {
        pi = mfloat_new_pi_prec(precision);
        if (!pi || mf_mul(value, pi) != 0)
            goto cleanup;
    } else if (pi_factor != 0) {
        goto cleanup;
    }

    rc = mfloat_finish_result(dst, value, precision);

cleanup:
    mf_free(num_a);
    mf_free(den_a);
    mf_free(num_b);
    mf_free(den_b);
    mf_free(num_sum);
    mf_free(den_sum);
    mf_free(value);
    mf_free(pi);
    return rc;
}

static int mfloat_compute_euler_gamma(mfloat_t *dst, size_t precision)
{
    static const unsigned powers[] = {
        2u, 4u, 6u, 8u, 10u, 12u, 14u, 16u,
        18u, 20u, 22u, 24u, 26u, 28u, 30u, 32u
    };
    mfloat_t *sum = NULL, *term = NULL, *ln2 = NULL, *tmp = NULL, *corr = NULL;
    size_t work_prec = precision + MFLOAT_CONST_GUARD_BITS;
    unsigned shift_bits = (unsigned)((precision + 47u) / 34u);
    unsigned long n, k;
    int rc = -1;

    if (shift_bits < MFLOAT_GAMMA_MIN_SHIFT)
        shift_bits = MFLOAT_GAMMA_MIN_SHIFT;
    if (shift_bits > MFLOAT_GAMMA_MAX_SHIFT)
        shift_bits = MFLOAT_GAMMA_MAX_SHIFT;
    n = 1ul << shift_bits;

    sum = mfloat_clone_prec(MF_ZERO, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    ln2 = mfloat_clone_immortal_prec_internal(&mfloat_ln2_1024_static, work_prec);
    if (!sum || !term || !ln2)
        goto cleanup;

    for (k = 1; k <= n; ++k) {
        if (mfloat_copy_value(term, MF_ONE) != 0 || mf_set_precision(term, work_prec) != 0 ||
            mfloat_div_long_inplace(term, (long)k) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
    }

    tmp = mf_clone(ln2);
    if (!tmp || mfloat_mul_long_inplace(tmp, (long)shift_bits) != 0)
        goto cleanup;
    if (mf_sub(sum, tmp) != 0)
        goto cleanup;
    mf_free(tmp);
    tmp = NULL;

    tmp = mfloat_clone_prec(MF_ONE, work_prec);
    if (!tmp)
        goto cleanup;
    if (mf_ldexp(tmp, -((int)shift_bits + 1)) != 0)
        goto cleanup;
    if (mf_sub(sum, tmp) != 0)
        goto cleanup;
    mf_free(tmp);
    tmp = NULL;

    for (k = 0; k < sizeof(powers) / sizeof(powers[0]); ++k) {
        corr = mfloat_clone_prec(MF_ONE, work_prec);
        if (!corr)
            goto cleanup;
        if (mf_ldexp(corr, -(int)(shift_bits * powers[k])) != 0)
            goto cleanup;
        if (mfloat_mul_euler_gamma_coeff_local(corr, k, work_prec) != 0)
            goto cleanup;
        if (mf_add(sum, corr) != 0)
            goto cleanup;
        mf_free(corr);
        corr = NULL;
    }

    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(dst, sum);
    if (rc == 0)
        dst->precision = precision;

cleanup:
    mf_free(sum);
    mf_free(term);
    mf_free(ln2);
    mf_free(tmp);
    mf_free(corr);
    return rc;
}

mfloat_t *mf_pi(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_PI->precision
            ? mfloat_set_from_binary_seed(tmp, MF_PI, precision)
            : mfloat_compute_pi(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_e(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_E->precision
            ? mfloat_set_from_binary_seed(tmp, MF_E, precision)
            : mfloat_compute_e(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_euler_mascheroni(void)
{
    size_t precision = mfloat_get_default_precision_internal();
    mfloat_t *tmp = mf_new_prec(precision);

    if (!tmp)
        return NULL;
    if ((precision <= MF_EULER_MASCHERONI->precision
            ? mfloat_set_from_binary_seed(tmp, MF_EULER_MASCHERONI, precision)
            : mfloat_compute_euler_gamma(tmp, precision)) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

mfloat_t *mf_max(void)
{
    return mf_create_double(DBL_MAX);
}

static int mfloat_store_qfloat_result(mfloat_t *mfloat, qfloat_t value)
{
    size_t precision;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (mf_set_qfloat(mfloat, value) != 0)
        return -1;
    return mf_set_precision(mfloat, precision);
}

static int mfloat_apply_qfloat_unary(mfloat_t *mfloat, qfloat_t (*fn)(qfloat_t))
{
    qfloat_t x;

    if (!mfloat || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    return mfloat_store_qfloat_result(mfloat, fn(x));
}

static int mfloat_apply_qfloat_binary(mfloat_t *mfloat,
                                      const mfloat_t *other,
                                      qfloat_t (*fn)(qfloat_t, qfloat_t))
{
    qfloat_t x, y;

    if (!mfloat || !other || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    y = mf_to_qfloat(other);
    return mfloat_store_qfloat_result(mfloat, fn(x, y));
}

static int mfloat_apply_qfloat_ternary(mfloat_t *mfloat,
                                       const mfloat_t *a,
                                       const mfloat_t *b,
                                       qfloat_t (*fn)(qfloat_t, qfloat_t, qfloat_t))
{
    qfloat_t x, y, z;

    if (!mfloat || !a || !b || !fn)
        return -1;
    x = mf_to_qfloat(mfloat);
    y = mf_to_qfloat(a);
    z = mf_to_qfloat(b);
    return mfloat_store_qfloat_result(mfloat, fn(x, y, z));
}

static mfloat_t *mfloat_clone_prec(const mfloat_t *src, size_t precision)
{
    mfloat_t *copy;

    if (!src)
        return NULL;
    copy = mf_new_prec(precision);
    if (!copy)
        return NULL;
    if (mfloat_copy_value(copy, src) != 0) {
        mf_free(copy);
        return NULL;
    }
    copy->precision = precision;
    return copy;
}

int mf_exp(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *ln2 = NULL, *kln2 = NULL, *r = NULL, *sum = NULL, *term = NULL;
    double xd, ln2d;
    long k;
    int squarings = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mf_is_zero(mfloat)) {
        precision = mfloat->precision;
        if (mfloat_copy_value(mfloat, MF_ONE) != 0)
            return -1;
        mfloat->precision = precision;
        return 0;
    }
    if (mfloat_equals_exact_long(mfloat, 1)) {
        precision = mfloat->precision;
        if (mfloat_set_from_immortal_internal(mfloat, MF_E, precision) != 0)
            return -1;
        mfloat->precision = precision;
        return 0;
    }

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    ln2 = mfloat_clone_immortal_prec_internal(&mfloat_ln2_1024_static, work_prec);
    if (!x || !ln2)
        goto cleanup;

    xd = mf_to_double(x);
    if (isnan(xd)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (isinf(xd)) {
        rc = mf_set_double(mfloat, xd > 0.0 ? INFINITY : 0.0);
        goto cleanup;
    }

    ln2d = mf_to_double(ln2);
    k = (long)nearbyint(xd / ln2d);

    kln2 = mf_clone(ln2);
    if (!kln2)
        goto cleanup;
    if (mf_mul_long(kln2, k) != 0)
        goto cleanup;

    r = mf_clone(x);
    if (!r || mf_sub(r, kln2) != 0)
        goto cleanup;

    xd = fabs(mf_to_double(r));
    while (xd > 0.125 && squarings < 8) {
        if (mf_ldexp(r, -1) != 0)
            goto cleanup;
        xd *= 0.5;
        squarings++;
    }

    sum = mfloat_clone_prec(MF_ONE, work_prec);
    term = mfloat_clone_prec(MF_ONE, work_prec);
    if (!sum || !term)
        goto cleanup;

    for (long n = 1; n < LONG_MAX; ++n) {
        if (mf_mul(term, r) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(term, n) != 0)
            goto cleanup;
        if (mf_add(sum, term) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(term, (long)work_prec + 8l))
            break;
    }

    for (int i = 0; i < squarings; ++i) {
        if (mf_mul(sum, sum) != 0)
            goto cleanup;
    }

    if (mf_ldexp(sum, (int)k) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(sum, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(mfloat, sum);
    if (rc == 0)
        mfloat->precision = precision;

cleanup:
    mf_free(x);
    mf_free(ln2);
    mf_free(kln2);
    mf_free(r);
    mf_free(sum);
    mf_free(term);
    return rc;
}

int mf_log(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    size_t mant_bits;
    long exp2;
    mint_t *mant = NULL;
    mfloat_scratch_slot_t slots[7];
    mfloat_t *m, *u, *u2, *y, *term, *piece, *ln2;
    double md;
    int rc = -1;
    size_t i;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat->sign < 0)
        return mf_set_double(mfloat, NAN);
    if (mf_is_zero(mfloat))
        return mf_set_double(mfloat, -INFINITY);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_log);
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision) + 32u);
    for (i = 0; i < 7u; ++i)
        mfloat_scratch_init_slot(&slots[i], work_prec);
    m = &slots[0].value;
    u = &slots[1].value;
    u2 = &slots[2].value;
    y = &slots[3].value;
    term = &slots[4].value;
    piece = &slots[5].value;
    ln2 = &slots[6].value;
    mant_bits = mi_bit_length(mfloat->mantissa);
    exp2 = mfloat->exponent2 + (long)mant_bits - 1l;
    mant = mi_clone(mfloat->mantissa);
    if (!mant)
        goto cleanup;
    if (mfloat_set_from_signed_mint(m, mant, -((long)mant_bits - 1l)) != 0)
        goto cleanup;

    md = mf_to_double(m);
    if (!(md > 0.0))
        goto cleanup;
    if (md < M_SQRT1_2) {
        if (mf_mul_long(m, 2) != 0)
            goto cleanup;
        exp2 -= 1;
    } else if (md > M_SQRT2) {
        if (mf_ldexp(m, -1) != 0)
            goto cleanup;
        exp2 += 1;
    }

    md = mf_to_double(m);
    if (!(md > 0.0))
        goto cleanup;
    if (mfloat_scratch_copy(u, m) != 0 || mfloat_scratch_copy(term, m) != 0)
        goto cleanup;
    if (mf_add_long(u, -1) != 0)
        goto cleanup;
    if (mf_add_long(term, 1) != 0)
        goto cleanup;
    if (mf_div(u, term) != 0)
        goto cleanup;
    mfloat_scratch_reset_slot(&slots[4], work_prec);

    if (mfloat_scratch_copy(y, u) != 0 || mfloat_scratch_copy(term, u) != 0 ||
        mfloat_scratch_copy(u2, u) != 0)
        goto cleanup;
    if (mf_mul(u2, u) != 0)
        goto cleanup;

    for (long k = 1; k < LONG_MAX / 2; ++k) {
        long denom = 2l * k + 1l;

        if (mf_mul(term, u2) != 0)
            goto cleanup;
        if (mfloat_scratch_copy(piece, term) != 0)
            goto cleanup;
        if (mfloat_div_long_inplace(piece, denom) != 0)
            goto cleanup;
        if (mf_add(y, piece) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(piece, (long)work_prec + 8l))
            break;
    }

    if (mf_mul_long(y, 2) != 0)
        goto cleanup;

    mfloat_scratch_reset_slot(&slots[0], work_prec);

    if (exp2 != 0) {
        if (mfloat_scratch_copy(ln2, &mfloat_ln2_1024_static) != 0)
            goto cleanup;
        if (mf_mul_long(ln2, exp2) != 0)
            goto cleanup;
        if (mf_add(y, ln2) != 0)
            goto cleanup;
    }

    if (mfloat_round_to_precision(y, precision) != 0)
        goto cleanup;
    rc = mfloat_copy_value(mfloat, y);
    if (rc == 0)
        mfloat->precision = precision;

cleanup:
    mi_free(mant);
    for (i = 0; i < 7u; ++i)
        mfloat_scratch_release_slot(&slots[i]);
    return rc;
}

int mf_pow(mfloat_t *mfloat, const mfloat_t *exponent)
{
    mfloat_t *tmp = NULL;
    long exp_long = 0;
    double ed;

    if (!mfloat || !exponent)
        return -1;

    if (mfloat_get_exact_long_value(exponent, &exp_long)) {
        if (exp_long >= INT_MIN && exp_long <= INT_MAX)
            return mf_pow_int(mfloat, (int)exp_long);
    }

    if (mfloat->sign <= 0)
        return mfloat_apply_qfloat_binary(mfloat, exponent, qf_pow);

    tmp = mfloat_clone_prec(exponent, mfloat->precision + MFLOAT_CONST_GUARD_BITS);
    if (!tmp)
        return -1;
    if (mf_log(mfloat) != 0) {
        mf_free(tmp);
        return -1;
    }
    if (mf_mul(mfloat, tmp) != 0) {
        mf_free(tmp);
        return -1;
    }
    ed = mf_to_double(mfloat);
    if (isnan(ed) || isinf(ed)) {
        mf_free(tmp);
        return mf_set_double(mfloat, ed);
    }
    mf_free(tmp);
    return mf_exp(mfloat);
}

mfloat_t *mf_pow10(int exponent10)
{
    qfloat_t q = qf_pow10(exponent10);

    return mf_create_qfloat(q);
}

int mf_sqr(mfloat_t *mfloat)
{
    mfloat_t *tmp;
    int rc;

    if (!mfloat)
        return -1;
    tmp = mf_clone(mfloat);
    if (!tmp)
        return -1;
    rc = mf_mul(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_floor(mfloat_t *mfloat)
{
    return mfloat_apply_qfloat_unary(mfloat, qf_floor);
}

int mf_mul_pow10(mfloat_t *mfloat, int exponent10)
{
    qfloat_t x;

    if (!mfloat)
        return -1;
    x = mf_to_qfloat(mfloat);
    return mfloat_store_qfloat_result(mfloat, qf_mul_pow10(x, exponent10));
}

int mf_hypot(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_qfloat_binary(mfloat, other, qf_hypot);
}

int mf_sin(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *r = NULL;
    int quadrant;
    int rc = -1;
    bool negate_result = false;
    static const mfloat_trig_dispatch_t sin_dispatch[4] = {
        { mfloat_sin_kernel, false },
        { mfloat_cos_kernel, false },
        { mfloat_sin_kernel, true  },
        { mfloat_cos_kernel, true  }
    };

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);
    if (mf_is_zero(mfloat))
        return 0;
    negate_result = mfloat_equals_exact_long(mfloat, -1);
    if (mfloat_equals_exact_long(mfloat, 1) || negate_result) {
        precision = mfloat->precision;
        if (mfloat_set_from_immortal_internal(mfloat, &mfloat_sin1_1024_static, precision) != 0)
            return -1;
        if (negate_result && mf_neg(mfloat) != 0)
            return -1;
        return 0;
    }

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_reduce_trig_argument(mfloat, work_prec, &r, &quadrant) != 0)
        goto cleanup;

    rc = mfloat_apply_trig_dispatch(mfloat, r, work_prec, sin_dispatch, quadrant);
    if (rc == 0)
        rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(r);
    return rc;
}

int mf_cos(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *r = NULL;
    int quadrant;
    int rc = -1;
    static const mfloat_trig_dispatch_t cos_dispatch[4] = {
        { mfloat_cos_kernel, false },
        { mfloat_sin_kernel, true  },
        { mfloat_cos_kernel, true  },
        { mfloat_sin_kernel, false }
    };

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);
    if (mfloat_equals_exact_long(mfloat, 1) || mfloat_equals_exact_long(mfloat, -1)) {
        precision = mfloat->precision;
        if (mfloat_set_from_immortal_internal(mfloat, &mfloat_cos1_1024_static, precision) != 0)
            return -1;
        return 0;
    }

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_reduce_trig_argument(mfloat, work_prec, &r, &quadrant) != 0)
        goto cleanup;

    rc = mfloat_apply_trig_dispatch(mfloat, r, work_prec, cos_dispatch, quadrant);
    if (rc == 0)
        rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(r);
    return rc;
}

int mf_tan(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *s = NULL, *c = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= 512u) {
        c = mf_clone(mfloat);
        if (!c)
            return -1;
        if (mf_sin(mfloat) != 0) {
            mf_free(c);
            return -1;
        }
        rc = mf_cos(c);
        if (rc == 0)
            rc = mf_div(mfloat, c);
        mf_free(c);
        return rc;
    }

    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    s = mfloat_clone_prec(MF_ZERO, work_prec);
    c = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!x || !s || !c)
        goto cleanup;
    if (mfloat_sincos_pair(s, c, x, work_prec) != 0)
        goto cleanup;
    if (mf_div(s, c) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, s, precision);

cleanup:
    mf_free(x);
    mf_free(s);
    mf_free(c);
    return rc;
}

int mf_atan(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *r = NULL, *pi = NULL, *half_pi = NULL, *quarter_pi = NULL;
    double xd;
    int negate = 0;
    int add_quarter_pi = 0;
    int subtract_from_half_pi = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) {
        pi = mfloat_new_pi_prec(mfloat->precision + MFLOAT_CONST_GUARD_BITS);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0) {
            mf_free(pi);
            return -1;
        }
        if (mfloat->kind == MFLOAT_KIND_NEGINF && mf_neg(pi) != 0) {
            mf_free(pi);
            return -1;
        }
        rc = mfloat_copy_value(mfloat, pi);
        mf_free(pi);
        return rc;
    }
    if (mf_is_zero(mfloat))
        return 0;

    precision = mfloat->precision;
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }

    xd = fabs(mf_to_double(x));
    r = mf_clone(x);
    if (!r)
        goto cleanup;

    if (xd > 1.0) {
        if (mf_inv(r) != 0)
            goto cleanup;
        subtract_from_half_pi = 1;
    } else if (xd > 0.4142135623730951) {
        mfloat_t *num = mf_clone(r);
        mfloat_t *den = mf_clone(r);

        if (!num || !den) {
            mf_free(num);
            mf_free(den);
            goto cleanup;
        }
        if (mf_add_long(num, -1) != 0 ||
            mf_add_long(den, 1) != 0 ||
            mf_div(num, den) != 0) {
            mf_free(num);
            mf_free(den);
            goto cleanup;
        }
        mf_free(den);
        mf_free(r);
        r = num;
        add_quarter_pi = 1;
    }

    if (mfloat_atan_kernel(mfloat, r, work_prec) != 0)
        goto cleanup;

    if (add_quarter_pi || subtract_from_half_pi) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi)
            goto cleanup;
        if (add_quarter_pi) {
            quarter_pi = mf_clone(pi);
            if (!quarter_pi || mfloat_div_long_inplace(quarter_pi, 4) != 0)
                goto cleanup;
            if (mf_add(mfloat, quarter_pi) != 0)
                goto cleanup;
        } else {
            half_pi = mf_clone(pi);
            if (!half_pi || mfloat_div_long_inplace(half_pi, 2) != 0)
                goto cleanup;
            if (mf_sub(half_pi, mfloat) != 0)
                goto cleanup;
            if (mfloat_copy_value(mfloat, half_pi) != 0)
                goto cleanup;
            mfloat->precision = work_prec;
        }
    }

    if (negate && mf_neg(mfloat) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(mfloat, precision) != 0)
        goto cleanup;
    mfloat->precision = precision;
    rc = 0;

cleanup:
    mf_free(x);
    mf_free(r);
    mf_free(pi);
    mf_free(half_pi);
    mf_free(quarter_pi);
    return rc;
}

int mf_atan2(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *pi = NULL;
    long xv = 0, yv = 0;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    if (!mfloat_is_finite(mfloat) || !mfloat_is_finite(other))
        return mfloat_apply_qfloat_binary(mfloat, other, qf_atan2);
    if (mfloat_equals_exact_long(mfloat, 1) && mfloat_equals_exact_long(other, -1)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0 || mf_mul_long(pi, 3) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }
    if (mfloat_equals_exact_long(mfloat, -1) && mfloat_equals_exact_long(other, -1)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0 || mf_mul_long(pi, -3) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &yv) &&
        mfloat_get_exact_long_value(other, &xv) &&
        (yv == 1 || yv == -1) && (xv == 1 || xv == -1) && labs(yv) == labs(xv)) {
        precision = mfloat->precision;
        work_prec = mfloat_transcendental_work_prec(precision);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0)
            goto cleanup;
        if (xv < 0) {
            if (yv > 0) {
                if (mf_mul_long(pi, 3) != 0)
                    goto cleanup;
            } else {
                if (mf_mul_long(pi, -3) != 0)
                    goto cleanup;
            }
        } else if (yv < 0) {
            if (mf_neg(pi) != 0)
                goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_atan2);
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));

    y = mfloat_clone_prec(mfloat, work_prec);
    x = mfloat_clone_prec(other, work_prec);
    if (!x || !y)
        goto cleanup;

    if (mfloat_abs_eq(x, y)) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 4) != 0)
            goto cleanup;
        if (x->sign < 0) {
            if (y->sign >= 0) {
                if (mf_mul_long(pi, 3) != 0)
                    goto cleanup;
            } else {
                if (mf_mul_long(pi, -3) != 0)
                    goto cleanup;
            }
        } else if (y->sign < 0) {
            if (mf_neg(pi) != 0)
                goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_is_zero(x)) {
        if (mf_is_zero(y))
            return mf_set_double(mfloat, NAN);
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0)
            goto cleanup;
        if (y->sign < 0 && mf_neg(pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_div(y, x) != 0)
        goto cleanup;
    if (mfloat_round_to_precision(y, precision) != 0)
        goto cleanup;
    y->precision = precision;
    if (mf_atan(y) != 0)
        goto cleanup;

    if (x->sign < 0) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi)
            goto cleanup;
        if (y->sign >= 0) {
            if (mf_add(y, pi) != 0)
                goto cleanup;
        } else {
            if (mf_sub(y, pi) != 0)
                goto cleanup;
        }
    }

    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(pi);
    return rc;
}

int mf_asin(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *y = NULL, *sin_y = NULL, *cos_y = NULL, *delta = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_asin);
    work_prec = precision + (MFLOAT_CONST_GUARD_BITS / 2u);

    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one)
        goto cleanup;

    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }
    if (mf_gt(x, one)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mf_eq(x, one)) {
        mfloat_t *pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 2) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        if (negate && mf_neg(pi) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        mf_free(pi);
        goto cleanup;
    }
    if (mfloat_is_exact_half(mfloat, 1) || mfloat_is_exact_half(mfloat, -1)) {
        mfloat_t *pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 6) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        if (mfloat_is_exact_half(mfloat, -1) && mf_neg(pi) != 0) {
            mf_free(pi);
            goto cleanup;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        mf_free(pi);
        goto cleanup;
    }
    y = mfloat_new_from_qfloat_prec(qf_asin(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;

    for (int iter = 0; iter < 2; ++iter) {
        sin_y = mf_clone(y);
        cos_y = mf_clone(y);
        if (!sin_y || !cos_y)
            goto cleanup;
        if (mfloat_sincos_pair(sin_y, cos_y, y, work_prec) != 0)
            goto cleanup;
        if (mf_sub(sin_y, x) != 0 || mf_div(sin_y, cos_y) != 0)
            goto cleanup;
        delta = sin_y;
        sin_y = NULL;
        if (mf_sub(y, delta) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(delta, (long)work_prec + 8l))
            break;
        mf_free(delta);
        delta = NULL;
        mf_free(cos_y);
        cos_y = NULL;
    }

    if (negate && mf_neg(y) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(y);
    mf_free(sin_y);
    mf_free(cos_y);
    mf_free(delta);
    return rc;
}

int mf_acos(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *tmp = NULL, *half_pi = NULL, *pi = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_acos);

    work_prec = precision;
    if (mfloat_is_exact_half(mfloat, 1) || mfloat_is_exact_half(mfloat, -1)) {
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mfloat_div_long_inplace(pi, 3) != 0)
            goto cleanup;
        if (mfloat_is_exact_half(mfloat, -1)) {
            mfloat_t *two_thirds_pi = mf_clone(pi);
            if (!two_thirds_pi || mf_mul_long(two_thirds_pi, 2) != 0) {
                mf_free(two_thirds_pi);
                goto cleanup;
            }
            mf_free(pi);
            pi = two_thirds_pi;
        }
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }
    tmp = mfloat_clone_prec(mfloat, work_prec);
    pi = mfloat_new_pi_prec(work_prec);
    if (!tmp || !pi)
        goto cleanup;
    if (mf_asin(tmp) != 0 || mfloat_div_long_inplace(pi, 2) != 0)
        goto cleanup;
    half_pi = pi;
    pi = NULL;
    if (mf_sub(half_pi, tmp) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, half_pi, precision);

cleanup:
    mf_free(tmp);
    mf_free(half_pi);
    mf_free(pi);
    return rc;
}

int mf_sinh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *sinh_tmp = NULL, *cosh_tmp = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return 0;

    precision = mfloat->precision;
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));

    sinh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    cosh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!sinh_tmp || !cosh_tmp)
        goto cleanup;
    if (mfloat_sinhcosh_pair(sinh_tmp, cosh_tmp, mfloat, work_prec) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, sinh_tmp, precision);

cleanup:
    mf_free(sinh_tmp);
    mf_free(cosh_tmp);
    return rc;
}

int mf_cosh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *sinh_tmp = NULL, *cosh_tmp = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_set_double(mfloat, INFINITY);

    precision = mfloat->precision;
    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));

    sinh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    cosh_tmp = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!sinh_tmp || !cosh_tmp)
        goto cleanup;
    if (mfloat_sinhcosh_pair(sinh_tmp, cosh_tmp, mfloat, work_prec) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, cosh_tmp, precision);

cleanup:
    mf_free(sinh_tmp);
    mf_free(cosh_tmp);
    return rc;
}

int mf_tanh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *twox = NULL, *num = NULL, *den = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return mfloat_copy_value(mfloat, MF_ONE);
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        rc = mfloat_copy_value(mfloat, MF_ONE);
        if (rc == 0)
            rc = mf_neg(mfloat);
        return rc;
    }

    precision = mfloat->precision;
    work_prec = mfloat_transcendental_work_prec(precision);

    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0)
            goto cleanup;
    }

    twox = mfloat_clone_prec(x, work_prec);
    num = mfloat_clone_prec(MF_ONE, work_prec);
    den = mfloat_clone_prec(MF_ONE, work_prec);
    if (!twox || !num || !den)
        goto cleanup;
    if (mf_mul_long(twox, 2) != 0 || mf_exp(twox) != 0)
        goto cleanup;
    if (mfloat_copy_value(num, twox) != 0 || mf_sub(num, MF_ONE) != 0)
        goto cleanup;
    if (mfloat_copy_value(den, twox) != 0 || mf_add(den, MF_ONE) != 0 ||
        mf_div(num, den) != 0)
        goto cleanup;
    if (negate && mf_neg(num) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, num, precision);

cleanup:
    mf_free(x);
    mf_free(twox);
    mf_free(num);
    mf_free(den);
    return rc;
}

int mf_asinh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *sinh_y = NULL, *cosh_y = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF)
        return 0;

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_asinh);

    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    y = mfloat_new_from_qfloat_prec(qf_asinh(mf_to_qfloat(x)), work_prec);
    sinh_y = mfloat_clone_prec(MF_ZERO, work_prec);
    cosh_y = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!y || !sinh_y || !cosh_y)
        goto cleanup;
    for (int iter = 0; iter < 2; ++iter) {
        if (mfloat_sinhcosh_pair(sinh_y, cosh_y, y, work_prec) != 0)
            goto cleanup;
        if (mf_sub(sinh_y, x) != 0 || mf_div(sinh_y, cosh_y) != 0)
            goto cleanup;
        if (mf_sub(y, sinh_y) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(sinh_y, (long)work_prec + 8l))
            break;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(sinh_y);
    mf_free(cosh_y);
    return rc;
}

int mf_acosh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *y = NULL, *cosh_y = NULL, *sinh_y = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat->sign < 0)
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_acosh);

    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one)
        goto cleanup;
    if (mf_lt(x, one)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mf_eq(x, one)) {
        mf_clear(mfloat);
        rc = 0;
        goto cleanup;
    }
    y = mfloat_new_from_qfloat_prec(qf_acosh(mf_to_qfloat(x)), work_prec);
    cosh_y = mfloat_clone_prec(MF_ZERO, work_prec);
    sinh_y = mfloat_clone_prec(MF_ZERO, work_prec);
    if (!y || !cosh_y || !sinh_y)
        goto cleanup;
    for (int iter = 0; iter < 2; ++iter) {
        if (mf_set_precision(cosh_y, work_prec) != 0 || mf_set_precision(sinh_y, work_prec) != 0)
            goto cleanup;
        if (mfloat_sinhcosh_pair(sinh_y, cosh_y, y, work_prec) != 0)
            goto cleanup;
        if (mf_sub(cosh_y, x) != 0 || mf_div(cosh_y, sinh_y) != 0)
            goto cleanup;
        if (mf_sub(y, cosh_y) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(cosh_y, (long)work_prec + 8l))
            break;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(y);
    mf_free(cosh_y);
    mf_free(sinh_y);
    return rc;
}

int mf_atanh(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL, *num = NULL, *den = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_atanh);

    work_prec = mfloat_cap_work_prec(mfloat_transcendental_work_prec(precision));
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    num = mfloat_clone_prec(MF_ONE, work_prec);
    den = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one || !num || !den)
        goto cleanup;
    if (x->sign < 0) {
        mfloat_t *absx = mf_clone(x);
        if (!absx)
            goto cleanup;
        if (mf_abs(absx) != 0) {
            mf_free(absx);
            goto cleanup;
        }
        if (mf_ge(absx, one)) {
            mf_free(absx);
            rc = mf_set_double(mfloat, NAN);
            goto cleanup;
        }
        mf_free(absx);
    } else {
        if (mf_ge(x, one)) {
            rc = mf_set_double(mfloat, NAN);
            goto cleanup;
        }
    }
    if (mf_add(num, x) != 0 || mf_sub(den, x) != 0 || mf_div(num, den) != 0)
        goto cleanup;
    if (mf_log(num) != 0 || mfloat_div_long_inplace(num, 2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, num, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    mf_free(num);
    mf_free(den);
    return rc;
}

int mf_gamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *tmp = NULL, *pi = NULL, *den = NULL;
    long n = 0, half_n = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_gamma);
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1)
        return mfloat_set_factorial(mfloat, n - 1, precision);
    if (mfloat_get_small_positive_half_integer(mfloat, &half_n)) {
        tmp = mf_new_prec(precision);
        den = mf_new_prec(precision);
        pi = mfloat_new_sqrt_pi_prec(precision);
        if (!tmp || !den || !pi)
            goto cleanup;
        if (mfloat_set_factorial(tmp, 2 * half_n, precision) != 0 ||
            mfloat_set_factorial(den, half_n, precision) != 0 ||
            mf_div(tmp, den) != 0)
            goto cleanup;
        if (half_n > 0 && mf_ldexp(tmp, -2 * (int)half_n) != 0)
            goto cleanup;
        if (mf_mul(tmp, pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    if (precision > 512u)
        work_prec = precision * 2u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        pi = mfloat_new_pi_prec(work_prec);
        tmp = mfloat_clone_prec(MF_ONE, work_prec);
        if (!pi || !tmp)
            goto cleanup;
        if (mf_sub(tmp, x) != 0 || mf_gamma(tmp) != 0)
            goto cleanup;
        if (mf_mul(pi, x) != 0 || mf_sin(pi) != 0 || mf_mul(pi, tmp) != 0 || mf_inv(pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        goto cleanup;
    }

    if (mf_lgamma(x) != 0 || mf_exp(x) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    mf_free(tmp);
    mf_free(pi);
    mf_free(den);
    return rc;
}

int mf_erf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *x2 = NULL, *sum = NULL, *term = NULL, *coef = NULL;
    int negate = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return mfloat_copy_value(mfloat, MF_ONE);
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        rc = mfloat_copy_value(mfloat, MF_ONE);
        if (rc == 0)
            rc = mf_neg(mfloat);
        return rc;
    }

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erf);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        sum = mfloat_clone_immortal_prec_internal(&mfloat_erf_half1024_static, work_prec);
        if (!sum)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, sum, precision);
        goto cleanup;
    }
    if (mfloat_is_exact_half(mfloat, -1)) {
        sum = mfloat_clone_immortal_prec_internal(&mfloat_erf_half1024_static, work_prec);
        if (sum && mf_neg(sum) != 0) {
            mf_free(sum);
            sum = NULL;
        }
        if (!sum)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, sum, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    x2 = mf_clone(x);
    sum = mf_clone(x);
    term = mf_clone(x);
    coef = mf_new_prec(work_prec);
    if (coef && mfloat_compute_two_over_sqrt_pi(coef, work_prec) != 0) {
        mf_free(coef);
        coef = NULL;
    }
    if (!x || !x2 || !sum || !term || !coef)
        goto cleanup;
    if (x->sign < 0) {
        negate = 1;
        if (mf_abs(x) != 0 || mf_abs(x2) != 0 || mf_abs(sum) != 0 || mf_abs(term) != 0)
            goto cleanup;
    }
    if (mf_mul(x2, x2) != 0)
        goto cleanup;
    for (long n = 0; n < 512; ++n) {
        mfloat_t *next = mf_clone(term);

        if (!next)
            goto cleanup;
        if (mf_mul(next, x2) != 0 || mf_neg(next) != 0 ||
            mf_mul_long(next, 2 * n + 1) != 0 ||
            mfloat_div_long_inplace(next, (n + 1) * (2 * n + 3)) != 0 ||
            mf_add(sum, next) != 0) {
            mf_free(next);
            goto cleanup;
        }
        mf_free(term);
        term = next;
        if (mfloat_is_below_neg_bits(term, (long)work_prec + 8l))
            break;
    }
    if (mf_mul(sum, coef) != 0)
        goto cleanup;
    if (negate && mf_neg(sum) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, sum, precision);

cleanup:
    mf_free(x);
    mf_free(x2);
    mf_free(sum);
    mf_free(term);
    mf_free(coef);
    return rc;
}

int mf_erfc(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_set_long(mfloat, 2);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfc);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        one = mfloat_clone_prec(MF_ONE, work_prec);
        if (one) {
            mfloat_t *erf_half = mfloat_clone_immortal_prec_internal(&mfloat_erf_half1024_static, work_prec);
            if (!erf_half || mf_sub(one, erf_half) != 0) {
                mf_free(one);
                one = NULL;
            }
            mf_free(erf_half);
        }
        if (!one)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, one, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one || mf_erf(x) != 0 || mf_sub(one, x) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, one, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    return rc;
}

int mf_erfinv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *y = NULL, *erfy = NULL, *fp = NULL, *step = NULL;
    mfloat_t *neg_one = NULL, *two_over_sqrt_pi = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfinv);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1)) {
        y = mfloat_clone_immortal_prec_internal(&mfloat_erfinv_half1024_static, work_prec);
        if (!y)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, y, precision);
        goto cleanup;
    }
    if (mfloat_is_exact_half(mfloat, -1)) {
        y = mfloat_clone_immortal_prec_internal(&mfloat_erfinv_half1024_static, work_prec);
        if (y && mf_neg(y) != 0) {
            mf_free(y);
            y = NULL;
        }
        if (!y)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, y, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    neg_one = mfloat_new_from_long_prec(-1, work_prec);
    two_over_sqrt_pi = mf_new_prec(work_prec);
    if (!x || !neg_one || !two_over_sqrt_pi || mfloat_compute_two_over_sqrt_pi(two_over_sqrt_pi, work_prec) != 0 ||
        mf_ge(x, MF_ONE) || mf_le(x, neg_one))
        goto cleanup;
    y = mfloat_new_from_qfloat_prec(qf_erfinv(mf_to_qfloat(x)), work_prec);
    if (!y)
        goto cleanup;

    for (int i = 0; i < 6; ++i) {
        erfy = mf_clone(y);
        fp = mf_clone(y);
        if (!erfy || !fp || mf_erf(erfy) != 0 || mf_sub(erfy, x) != 0)
            goto cleanup;
        if (mf_mul(fp, fp) != 0 || mf_neg(fp) != 0 || mf_exp(fp) != 0 ||
            mf_mul(fp, two_over_sqrt_pi) != 0)
            goto cleanup;
        step = mf_clone(erfy);
        if (!step || mf_div(step, fp) != 0 || mf_sub(y, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(erfy);
        mf_free(fp);
        mf_free(step);
        erfy = fp = step = NULL;
    }
    rc = mfloat_finish_result(mfloat, y, precision);

cleanup:
    mf_free(x);
    mf_free(y);
    mf_free(erfy);
    mf_free(fp);
    mf_free(step);
    mf_free(neg_one);
    mf_free(two_over_sqrt_pi);
    return rc;
}

int mf_erfcinv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *one = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_erfcinv);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_is_exact_half(mfloat, 1))
        return mfloat_set_from_immortal_internal(mfloat, &mfloat_erfinv_half1024_static, precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    if (!x || !one || mf_sub(one, x) != 0 || mf_erfinv(one) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, one, precision);

cleanup:
    mf_free(x);
    mf_free(one);
    return rc;
}

int mf_lgamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *logz = NULL, *pi = NULL, *threshold = NULL, *den = NULL;
    long n = 0;
    long threshold_long;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lgamma);
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        if (n == 1) {
            mf_clear(mfloat);
            return 0;
        }
        tmp = mf_new_prec(precision);
        if (!tmp)
            return -1;
        if (mfloat_set_factorial(tmp, n - 1, precision) != 0 || mf_log(tmp) != 0) {
            mf_free(tmp);
            return -1;
        }
        rc = mfloat_finish_result(mfloat, tmp, precision);
        mf_free(tmp);
        return rc;
    }
    if (mfloat_get_small_positive_half_integer(mfloat, &n)) {
        tmp = mf_new_prec(precision);
        den = mf_new_prec(precision);
        pi = mfloat_new_half_ln_pi_prec(precision);
        if (!tmp || !den || !pi)
            goto cleanup;
        if (mfloat_set_factorial(tmp, 2 * n, precision) != 0 ||
            mfloat_set_factorial(den, n, precision) != 0 ||
            mf_div(tmp, den) != 0)
            goto cleanup;
        if (n > 0 && mf_ldexp(tmp, -2 * (int)n) != 0)
            goto cleanup;
        if (mf_log(tmp) != 0 || mf_add(tmp, pi) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    if (precision > 512u)
        work_prec = precision * 2u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        mfloat_t *one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
        mfloat_t *sin_term = NULL;
        if (!one_minus_x)
            goto cleanup;
        if (mf_sub(one_minus_x, x) != 0)
            goto cleanup;
        if (mf_lgamma(one_minus_x) != 0)
            goto cleanup;
        sin_term = mfloat_new_pi_prec(work_prec);
        if (!sin_term)
            goto cleanup;
        if (mf_mul(sin_term, x) != 0 || mf_sin(sin_term) != 0 || mf_abs(sin_term) != 0 ||
            mf_log(sin_term) != 0)
            goto cleanup;
        pi = mfloat_new_pi_prec(work_prec);
        if (!pi || mf_log(pi) != 0)
            goto cleanup;
        if (mf_sub(pi, sin_term) != 0 || mf_sub(pi, one_minus_x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, pi, precision);
        mf_free(one_minus_x);
        mf_free(sin_term);
        goto cleanup;
    }

    z = x;
    x = NULL;
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    threshold_long = mfloat_lgamma_asymptotic_threshold(precision);
    threshold = mfloat_new_from_long_prec(threshold_long, work_prec);
    tmp = mf_new_prec(work_prec);
    logz = mf_new_prec(work_prec);
    if (!z || !acc || !threshold || !tmp || !logz)
        goto cleanup;
    if (mfloat_copy_value(logz, z) != 0 || mf_log(logz) != 0)
        goto cleanup;
    while (mf_lt(z, threshold)) {
        if (mf_add(acc, logz) != 0)
            goto cleanup;
        if (mf_add_long(z, 1) != 0)
            goto cleanup;
        if (mfloat_copy_value(logz, z) != 0 || mf_log(logz) != 0)
            goto cleanup;
    }
    if (mfloat_lgamma_asymptotic(z, z, work_prec) != 0 || mf_sub(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(logz);
    mf_free(pi);
    mf_free(threshold);
    mf_free(den);
    return rc;
}

int mf_digamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long n = 0;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_digamma);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        x = mf_euler_mascheroni();
        if (x && mf_set_precision(x, precision) == 0)
            (void)mf_neg(x);
        if (!x)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        tmp = mfloat_clone_prec(MF_ZERO, precision);
        z = NULL;
        if (!tmp)
            return -1;
        for (long k = 1; k < n; ++k) {
            z = mfloat_clone_prec(MF_ONE, precision);
            if (!z || mfloat_div_long_inplace(z, k) != 0 || mf_add(tmp, z) != 0)
                goto cleanup;
            mf_free(z);
            z = NULL;
        }
        x = mf_euler_mascheroni();
        if (!x || mf_set_precision(x, precision) != 0 || mf_neg(x) != 0 || mf_add(tmp, x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    if (precision > 384u)
        work_prec += 256u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 0);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_div(tmp, z) != 0 ||
            mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_div(tmp, z) != 0 ||
            mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_digamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_trigamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long n = 0;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_trigamma);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        tmp = mfloat_new_pi_prec(work_prec);
        if (tmp && (mf_mul(tmp, tmp) != 0 || mfloat_div_long_inplace(tmp, 6) != 0)) {
            mf_free(tmp);
            tmp = NULL;
        }
        if (!tmp)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n >= 1) {
        tmp = mfloat_new_pi_prec(work_prec);
        if (!tmp || mf_mul(tmp, tmp) != 0 || mfloat_div_long_inplace(tmp, 6) != 0)
            goto cleanup;
        for (long k = 1; k < n; ++k) {
            x = mfloat_new_from_long_prec(k, work_prec);
            if (!x || mf_mul(x, x) != 0 || mf_inv(x) != 0 || mf_sub(tmp, x) != 0)
                goto cleanup;
            mf_free(x);
            x = NULL;
        }
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 1);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_inv(tmp) != 0 || mf_add(acc, tmp) != 0 ||
            mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_inv(tmp) != 0 || mf_add(acc, tmp) != 0 ||
            mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_trigamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_tetragamma(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *z = NULL, *acc = NULL, *tmp = NULL, *twenty = NULL;
    long steps = -1;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat_is_near_integer_pole(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_tetragamma);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        tmp = mfloat_clone_immortal_prec_internal(&mfloat_tetragamma1_1024_static, precision);
        if (!tmp)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, tmp, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;

    if (mf_lt(x, MF_HALF)) {
        rc = mfloat_eval_reflected_gamma_family(mfloat, x, precision, 2);
        goto cleanup;
    }

    z = mfloat_clone_prec(x, work_prec);
    acc = mfloat_clone_prec(MF_ZERO, work_prec);
    twenty = mfloat_new_from_long_prec(20, work_prec);
    tmp = mf_new_prec(work_prec);
    if (!z || !acc || !twenty || !tmp)
        goto cleanup;
    steps = mfloat_estimate_positive_unit_steps(z, 20);
    if (steps < 0)
        goto cleanup;
    for (long i = 0; i < steps; ++i) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_mul(tmp, z) != 0 || mf_inv(tmp) != 0 ||
            mf_mul_long(tmp, 2) != 0 || mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    while (mf_lt(z, twenty)) {
        if (mfloat_copy_value(tmp, z) != 0 || mf_mul(tmp, z) != 0 ||
            mf_mul(tmp, z) != 0 || mf_inv(tmp) != 0 ||
            mf_mul_long(tmp, 2) != 0 || mf_sub(acc, tmp) != 0 || mf_add_long(z, 1) != 0)
            goto cleanup;
    }
    if (mfloat_tetragamma_asymptotic(z, z, work_prec) != 0 || mf_add(z, acc) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, z, precision);

cleanup:
    mf_free(x);
    mf_free(z);
    mf_free(acc);
    mf_free(tmp);
    mf_free(twenty);
    return rc;
}

int mf_gammainv(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *y = NULL, *logy = NULL, *x = NULL, *lg = NULL, *psi = NULL, *step = NULL;
    mfloat_t *one = NULL, *minval = NULL, *gamma_2_5 = NULL;
    mfloat_t *delta = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat) || mfloat->sign <= 0)
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    gamma_2_5 = mfloat_clone_prec(MF_SQRT_PI, precision);
    if (!gamma_2_5)
        goto cleanup;
    if (mi_mul_long(gamma_2_5->mantissa, 3) != 0 ||
        mfloat_normalise(gamma_2_5) != 0 ||
        mfloat_div_long_inplace(gamma_2_5, 4) != 0)
        goto cleanup;
    if (mf_eq(mfloat, gamma_2_5)) {
        rc = mf_set_double(mfloat, 2.5);
        goto cleanup;
    }
    delta = mf_clone(gamma_2_5);
    if (!delta)
        goto cleanup;
    if (mf_sub(delta, mfloat) != 0 || mf_abs(delta) != 0)
        goto cleanup;
    if (mfloat_is_below_neg_bits(delta, 256l)) {
        rc = mf_set_double(mfloat, 2.5);
        goto cleanup;
    }
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_gammainv);
    work_prec = mfloat_transcendental_work_prec(precision);

    y = mfloat_clone_prec(mfloat, work_prec);
    one = mfloat_clone_prec(MF_ONE, work_prec);
    minval = mfloat_clone_immortal_prec_internal(&mfloat_gammainv_min1024_static, work_prec);
    if (!y || !one || !minval)
        goto cleanup;
    if (mf_lt(y, minval)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }
    if (mfloat_equals_exact_long(mfloat, 3)) {
        rc = mfloat_set_from_immortal_internal(mfloat, &mfloat_gammainv_31024_static, precision);
        goto cleanup;
    }

    logy = mf_clone(y);
    if (!logy || mf_log(logy) != 0)
        goto cleanup;
    if (mf_le(y, one)) {
        x = mfloat_clone_prec(MF_ONE, work_prec);
    } else {
        x = mfloat_new_from_qfloat_prec(qf_gammainv(mf_to_qfloat(y)), work_prec);
        if (!x)
            x = mf_clone(logy);
        if (x && mf_lt(x, MF_ONE) &&
            mfloat_set_from_immortal_internal(x, &mfloat_gammainv_argmin1024_static, work_prec) != 0)
            goto cleanup;
    }
    if (!x)
        goto cleanup;

    for (int i = 0; i < 8; ++i) {
        lg = mf_clone(x);
        psi = mf_clone(x);
        if (!lg || !psi || mf_lgamma(lg) != 0 || mf_digamma(psi) != 0)
            goto cleanup;
        if (mf_sub(lg, logy) != 0)
            goto cleanup;
        step = mf_clone(lg);
        if (!step || mf_div(step, psi) != 0 || mf_sub(x, step) != 0)
            goto cleanup;
        if (mf_gt(y, one) && mf_lt(x, MF_ONE) &&
            mfloat_set_from_immortal_internal(x, &mfloat_gammainv_argmin1024_static, work_prec) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(lg);
        mf_free(psi);
        mf_free(step);
        lg = psi = step = NULL;
    }
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(y);
    mf_free(logy);
    mf_free(x);
    mf_free(lg);
    mf_free(psi);
    mf_free(step);
    mf_free(one);
    mf_free(minval);
    mf_free(gamma_2_5);
    mf_free(delta);
    return rc;
}

int mf_lambert_w0(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *w = NULL, *ew = NULL, *num = NULL, *den = NULL, *step = NULL;
    mfloat_t *one = NULL, *two = NULL, *wplus2 = NULL, *corr = NULL;
    long n = 0;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lambert_w0);
    work_prec = mfloat_transcendental_work_prec(precision);
    if (mfloat_equals_exact_long(mfloat, 1)) {
        w = mfloat_clone_immortal_prec_internal(&mfloat_lambert_w0_11024_static, work_prec);
        if (!w)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, w, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &n) && n == 1) {
        w = mfloat_clone_immortal_prec_internal(&mfloat_lambert_w0_11024_static, work_prec);
        if (!w)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, w, precision);
        goto cleanup;
    }
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    one = mfloat_clone_prec(MF_ONE, work_prec);
    two = mfloat_new_from_long_prec(2, work_prec);
    w = mfloat_new_from_qfloat_prec(qf_lambert_w0(mf_to_qfloat(x)), work_prec);
    if (!w || !one || !two)
        goto cleanup;

    for (int i = 0; i < 20; ++i) {
        ew = mf_clone(w);
        num = mf_clone(w);
        den = mf_clone(w);
        wplus2 = mf_clone(w);
        corr = mf_clone(w);
        if (!ew || !num || !den || !wplus2 || !corr || mf_exp(ew) != 0)
            goto cleanup;
        if (mf_mul(num, ew) != 0 || mf_sub(num, x) != 0)
            goto cleanup;
        if (mf_add(den, one) != 0 || mf_mul(den, ew) != 0)
            goto cleanup;
        if (mf_add(wplus2, two) != 0 || mf_mul(wplus2, ew) != 0)
            goto cleanup;
        if (mfloat_copy_value(corr, num) != 0 || mf_div(corr, den) != 0 ||
            mf_mul(corr, wplus2) != 0 || mfloat_div_long_inplace(corr, 2) != 0)
            goto cleanup;
        if (mf_sub(den, corr) != 0)
            goto cleanup;
        step = mf_clone(num);
        if (!step || mf_div(step, den) != 0 || mf_sub(w, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(ew);
        mf_free(num);
        mf_free(den);
        mf_free(step);
        mf_free(wplus2);
        mf_free(corr);
        ew = num = den = step = wplus2 = corr = NULL;
    }
    rc = mfloat_finish_result(mfloat, w, precision);

cleanup:
    mf_free(x);
    mf_free(w);
    mf_free(ew);
    mf_free(num);
    mf_free(den);
    mf_free(step);
    mf_free(one);
    mf_free(two);
    mf_free(wplus2);
    mf_free(corr);
    return rc;
}

int mf_lambert_wm1(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *w = NULL, *ew = NULL, *num = NULL, *den = NULL, *step = NULL, *one = NULL;
    mfloat_t *two = NULL, *thresh = NULL, *minus_one = NULL, *tmp = NULL, *nx = NULL, *l1 = NULL, *minus_l1 = NULL, *l2 = NULL, *econst = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_lambert_wm1);
    if (mfloat_equals_const_value(mfloat, &mfloat_neg_tenth_1024_static) ||
        mfloat_is_near_const_value(mfloat, &mfloat_neg_tenth_1024_static, 256l))
        return mfloat_set_from_immortal_internal(mfloat, &mfloat_lambert_wm1_m0_1_1024_static, precision);
    work_prec = mfloat_transcendental_work_prec(precision);

    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    one = mfloat_clone_prec(MF_ONE, work_prec);
    two = mfloat_new_from_long_prec(2, work_prec);
    minus_one = mfloat_new_from_long_prec(-1, work_prec);
    thresh = mfloat_clone_prec(MF_TENTH, work_prec);
    if (!one || !two || !minus_one || !thresh || mf_mul_long(thresh, -2) != 0)
        goto cleanup;

    if (mf_le(x, thresh)) {
        econst = mfloat_clone_immortal_prec_internal(MF_E, work_prec);
        tmp = mf_clone(x);
        if (!econst || !tmp || mf_mul(tmp, econst) != 0 || mf_add(tmp, one) != 0 ||
            mf_mul(tmp, two) != 0 || mf_sqrt(tmp) != 0)
            goto cleanup;
        w = mf_clone(minus_one);
        if (!w || mf_sub(w, tmp) != 0)
            goto cleanup;
    } else {
        nx = mf_clone(x);
        if (!nx || mf_neg(nx) != 0)
            goto cleanup;
        l1 = mf_clone(nx);
        if (!l1 || mf_log(l1) != 0)
            goto cleanup;
        minus_l1 = mf_clone(l1);
        if (!minus_l1 || mf_neg(minus_l1) != 0)
            goto cleanup;
        l2 = mf_clone(minus_l1);
        if (!l2 || mf_log(l2) != 0)
            goto cleanup;
        w = mf_clone(l1);
        tmp = mf_clone(l2);
        if (!w || !tmp || mf_sub(w, l2) != 0 || mf_div(tmp, l1) != 0 || mf_add(w, tmp) != 0)
            goto cleanup;
    }

    for (int i = 0; i < 24; ++i) {
        ew = mf_clone(w);
        num = mf_clone(w);
        den = mf_clone(w);
        if (!ew || !num || !den || mf_exp(ew) != 0)
            goto cleanup;
        if (mf_mul(num, ew) != 0 || mf_sub(num, x) != 0)
            goto cleanup;
        if (mf_add(den, one) != 0 || mf_mul(den, ew) != 0)
            goto cleanup;
        step = mf_clone(num);
        if (!step || mf_div(step, den) != 0 || mf_sub(w, step) != 0)
            goto cleanup;
        if (mfloat_is_below_neg_bits(step, (long)work_prec + 8l))
            break;
        mf_free(ew);
        mf_free(num);
        mf_free(den);
        mf_free(step);
        ew = num = den = step = NULL;
    }
    rc = mfloat_finish_result(mfloat, w, precision);

cleanup:
    mf_free(x);
    mf_free(w);
    mf_free(ew);
    mf_free(num);
    mf_free(den);
    mf_free(step);
    mf_free(one);
    mf_free(two);
    mf_free(thresh);
    mf_free(minus_one);
    mf_free(tmp);
    mf_free(nx);
    mf_free(l1);
    mf_free(minus_l1);
    mf_free(l2);
    mf_free(econst);
    return rc;
}

int mf_beta(mfloat_t *mfloat, const mfloat_t *other)
{
    long a = 0, b = 0;
    size_t precision;
    mfloat_t *num = NULL, *den = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_beta);

    if (mfloat_get_small_positive_integer(mfloat, &a) &&
        mfloat_get_small_positive_integer(other, &b)) {
        long k;

        num = mfloat_clone_prec(MF_ONE, mfloat_transcendental_work_prec(precision));
        den = mfloat_clone_prec(MF_ONE, mfloat_transcendental_work_prec(precision));
        if (!num || !den)
            goto cleanup;
        for (k = 1; k <= b - 1; ++k) {
            if (mf_mul_long(num, k) != 0)
                goto cleanup;
        }
        for (k = 0; k <= b - 1; ++k) {
            if (mf_mul_long(den, a + k) != 0)
                goto cleanup;
        }
        if (mf_div(num, den) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, num, precision);
        goto cleanup;
    }

    return mfloat_apply_qfloat_binary(mfloat, other, qf_beta);

cleanup:
    mf_free(num);
    mf_free(den);
    return rc;
}

int mf_logbeta(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *a = NULL, *b = NULL, *sum = NULL, *beta = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_logbeta);
    work_prec = mfloat_transcendental_work_prec(precision);
    beta = mf_new_prec(work_prec);
    if (!beta)
        goto cleanup;
    if (mfloat_try_small_exact_beta(beta, mfloat, other, work_prec) == 0) {
        if (mf_log(beta) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, beta, precision);
        goto cleanup;
    }
    work_prec += 128u;
    a = mfloat_clone_prec(mfloat, work_prec);
    b = mfloat_clone_prec(other, work_prec);
    sum = mf_clone(a);
    if (!a || !b || !sum)
        goto cleanup;
    if (mf_add(sum, b) != 0 || mf_lgamma(a) != 0 || mf_lgamma(b) != 0 || mf_lgamma(sum) != 0)
        goto cleanup;
    if (mf_add(a, b) != 0 || mf_sub(a, sum) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, a, precision);

cleanup:
    mf_free(a);
    mf_free(b);
    mf_free(sum);
    mf_free(beta);
    return rc;
}

int mf_binomial(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *a1 = NULL, *b1 = NULL, *amb1 = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_binomial);
    work_prec = mfloat_transcendental_work_prec(precision);
    a1 = mfloat_clone_prec(mfloat, work_prec);
    b1 = mfloat_clone_prec(other, work_prec);
    amb1 = mfloat_clone_prec(mfloat, work_prec);
    if (!a1 || !b1 || !amb1)
        goto cleanup;
    if (mf_add_long(a1, 1) != 0 || mf_add_long(b1, 1) != 0 ||
        mf_sub(amb1, other) != 0 || mf_add_long(amb1, 1) != 0)
        goto cleanup;
    if (mf_lgamma(a1) != 0 || mf_lgamma(b1) != 0 || mf_lgamma(amb1) != 0)
        goto cleanup;
    if (mf_sub(a1, b1) != 0 || mf_sub(a1, amb1) != 0 || mf_exp(a1) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, a1, precision);

cleanup:
    mf_free(a1);
    mf_free(b1);
    mf_free(amb1);
    return rc;
}

int mf_beta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *aa = NULL, *bb = NULL, *one_minus_x = NULL, *logpdf = NULL, *logb = NULL;
    int rc = -1;

    if (!mfloat || !a || !b)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_beta_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    aa = mfloat_clone_prec(a, work_prec);
    bb = mfloat_clone_prec(b, work_prec);
    one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
    logpdf = mfloat_clone_prec(mfloat, work_prec);
    logb = mfloat_clone_prec(a, work_prec);
    if (!x || !aa || !bb || !one_minus_x || !logpdf || !logb)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_log(logpdf) != 0 || mf_sub(aa, MF_ONE) != 0 ||
        mf_mul(logpdf, aa) != 0)
        goto cleanup;
    if (mf_log(one_minus_x) != 0 || mf_sub(bb, MF_ONE) != 0 || mf_mul(one_minus_x, bb) != 0 ||
        mf_add(logpdf, one_minus_x) != 0 || mf_logbeta(logb, b) != 0 || mf_sub(logpdf, logb) != 0 ||
        mf_exp(logpdf) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, logpdf, precision);

cleanup:
    mf_free(x);
    mf_free(aa);
    mf_free(bb);
    mf_free(one_minus_x);
    mf_free(logpdf);
    mf_free(logb);
    return rc;
}

int mf_logbeta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL, *aa = NULL, *bb = NULL, *one_minus_x = NULL, *logb = NULL;
    int rc = -1;

    if (!mfloat || !a || !b)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_ternary(mfloat, a, b, qf_logbeta_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    aa = mfloat_clone_prec(a, work_prec);
    bb = mfloat_clone_prec(b, work_prec);
    one_minus_x = mfloat_clone_prec(MF_ONE, work_prec);
    logb = mfloat_clone_prec(a, work_prec);
    if (!x || !aa || !bb || !one_minus_x || !logb)
        goto cleanup;
    if (mf_sub(one_minus_x, x) != 0 || mf_log(x) != 0 || mf_sub(aa, MF_ONE) != 0 || mf_mul(x, aa) != 0)
        goto cleanup;
    if (mf_log(one_minus_x) != 0 || mf_sub(bb, MF_ONE) != 0 || mf_mul(one_minus_x, bb) != 0 ||
        mf_add(x, one_minus_x) != 0 || mf_logbeta(logb, b) != 0 || mf_sub(x, logb) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    mf_free(aa);
    mf_free(bb);
    mf_free(one_minus_x);
    mf_free(logb);
    return rc;
}

int mf_normal_pdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x2 = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_pdf);
    work_prec = mfloat_transcendental_work_prec(precision) + 32u;
    x2 = mfloat_clone_prec(mfloat, work_prec);
    if (!x2)
        goto cleanup;
    if (mf_normal_logpdf(x2) != 0 || mf_exp(x2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x2, precision);

cleanup:
    mf_free(x2);
    return rc;
}

int mf_normal_cdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *t = NULL, *half = NULL, *sqrt2 = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_cdf);
    if (mf_is_zero(mfloat))
        return mfloat_copy_value(mfloat, MF_HALF);
    work_prec = mfloat_transcendental_work_prec(precision);
    t = mfloat_clone_prec(mfloat, work_prec);
    half = mfloat_clone_prec(MF_HALF, work_prec);
    sqrt2 = mfloat_clone_immortal_prec_internal(MF_SQRT2, work_prec);
    if (!t || !half || !sqrt2 || mf_div(t, sqrt2) != 0 ||
        mf_erf(t) != 0 || mf_add(t, MF_ONE) != 0 || mf_mul(t, half) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, t, precision);

cleanup:
    mf_free(t);
    mf_free(half);
    mf_free(sqrt2);
    return rc;
}

int mf_normal_logpdf(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x2 = NULL, *c = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_normal_logpdf);
    work_prec = mfloat_transcendental_work_prec(precision);
    x2 = mfloat_clone_prec(mfloat, work_prec);
    c = mfloat_clone_immortal_prec_internal(&mfloat_half_ln_2pi1024_static, work_prec);
    if (!x2 || !c ||
        mf_mul(x2, x2) != 0 || mfloat_div_long_inplace(x2, 2) != 0 ||
        mf_add(x2, c) != 0 || mf_neg(x2) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x2, precision);

cleanup:
    mf_free(x2);
    mf_free(c);
    return rc;
}

int mf_productlog(mfloat_t *mfloat)
{
    return mf_lambert_w0(mfloat);
}

int mf_gammainc_lower(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *p = NULL, *g = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_lower);
    work_prec = mfloat_transcendental_work_prec(precision);
    p = mfloat_clone_prec(mfloat, work_prec);
    g = mfloat_clone_prec(mfloat, work_prec);
    if (!p || !g || mf_gammainc_P(p, other) != 0 || mf_gamma(g) != 0 || mf_mul(p, g) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, p, precision);

cleanup:
    mf_free(p);
    mf_free(g);
    return rc;
}

int mf_gammainc_upper(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    mfloat_t *q = NULL, *g = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_upper);
    work_prec = mfloat_transcendental_work_prec(precision);
    q = mfloat_clone_prec(mfloat, work_prec);
    g = mfloat_clone_prec(mfloat, work_prec);
    if (!q || !g || mf_gammainc_Q(q, other) != 0 || mf_gamma(g) != 0 || mf_mul(q, g) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, q, precision);

cleanup:
    mf_free(q);
    mf_free(g);
    return rc;
}

int mf_gammainc_P(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    long s_long = 0;
    mfloat_t *s = NULL, *x = NULL, *tmp = NULL, *s_plus_one = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_P);
    work_prec = mfloat_transcendental_work_prec(precision);
    s = mfloat_clone_prec(mfloat, work_prec);
    x = mfloat_clone_prec(other, work_prec);
    if (!s || !x)
        goto cleanup;

    if (mf_le(x, MF_ZERO)) {
        mf_clear(mfloat);
        rc = 0;
        goto cleanup;
    }
    if (mf_le(s, MF_ZERO)) {
        rc = mf_set_double(mfloat, NAN);
        goto cleanup;
    }

    if (mfloat_is_finite(mfloat) &&
        mfloat->sign > 0 &&
        mfloat->exponent2 == 0 &&
        mi_fits_long(mfloat->mantissa) &&
        mi_get_long(mfloat->mantissa, &s_long) &&
        s_long == 1) {
        tmp = mfloat_clone_prec(other, work_prec);
        if (!tmp)
            goto cleanup;
        if (mf_neg(tmp) != 0 || mf_exp(tmp) != 0)
            goto cleanup;
        if (mfloat_copy_value(x, MF_ONE) != 0 || mf_set_precision(x, work_prec) != 0)
            goto cleanup;
        if (mf_sub(x, tmp) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    s_plus_one = mf_clone(s);
    if (!s_plus_one || mf_add_long(s_plus_one, 1) != 0)
        goto cleanup;
    if (mf_lt(x, s_plus_one)) {
        if (mfloat_gammainc_series_P(x, s, x, work_prec) != 0)
            goto cleanup;
    } else {
        tmp = mf_clone(s);
        if (!tmp || mfloat_gammainc_cf_Q(tmp, s, x, work_prec) != 0)
            goto cleanup;
        if (mfloat_copy_value(x, MF_ONE) != 0 || mf_set_precision(x, work_prec) != 0 || mf_sub(x, tmp) != 0)
            goto cleanup;
    }
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(s);
    mf_free(x);
    mf_free(tmp);
    mf_free(s_plus_one);
    return rc;
}

int mf_gammainc_Q(mfloat_t *mfloat, const mfloat_t *other)
{
    size_t precision, work_prec;
    long s_long = 0;
    mfloat_t *p = NULL, *x = NULL;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_binary(mfloat, other, qf_gammainc_Q);
    if (mfloat_equals_exact_long(mfloat, 1) && mfloat_equals_exact_long(other, 1)) {
        x = mf_e();
        if (!x || mf_set_precision(x, precision) != 0 || mf_inv(x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    if (mfloat_get_exact_long_value(mfloat, &s_long) && s_long == 1) {
        work_prec = mfloat_transcendental_work_prec(precision);
        x = mfloat_clone_prec(other, work_prec);
        if (!x || mf_neg(x) != 0 || mf_exp(x) != 0)
            goto cleanup;
        rc = mfloat_finish_result(mfloat, x, precision);
        goto cleanup;
    }
    work_prec = mfloat_transcendental_work_prec(precision);
    p = mfloat_clone_prec(mfloat, work_prec);
    if (!p || mf_gammainc_P(p, other) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, MF_ONE) != 0 || mf_set_precision(mfloat, work_prec) != 0 || mf_sub(mfloat, p) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision(mfloat, precision);

cleanup:
    mf_free(p);
    mf_free(x);
    return rc;
}

int mf_ei(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mf_clear(mfloat), 0;

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_ei);
    work_prec = mfloat_transcendental_work_prec(precision);
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x)
        goto cleanup;
    rc = mfloat_ei_series(x, x, work_prec);
    if (rc == 0)
        rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    return rc;
}

int mf_e1(mfloat_t *mfloat)
{
    size_t precision, work_prec;
    mfloat_t *x = NULL;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat->kind == MFLOAT_KIND_NEGINF || !mfloat_is_finite(mfloat))
        return mf_set_double(mfloat, NAN);

    precision = mfloat->precision;
    if (precision <= MFLOAT_QFLOAT_EFFECTIVE_BITS)
        return mfloat_apply_qfloat_unary(mfloat, qf_e1);
    work_prec = mfloat_transcendental_work_prec(precision) + 64u;
    x = mfloat_clone_prec(mfloat, work_prec);
    if (!x || mf_le(x, MF_ZERO))
        goto cleanup;
    if (mfloat_e1_series_positive(x, x, work_prec) != 0)
        goto cleanup;
    rc = mfloat_finish_result(mfloat, x, precision);

cleanup:
    mf_free(x);
    return rc;
}
