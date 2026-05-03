#include "mfloat_internal.h"
#include "internal/mfloat_coeff_tables.h"
#include "internal/mint_layout.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static struct _mfloat_t mfloat_zero_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_one_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_half_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_tenth_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ten_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_nan_static = {
    .kind = MFLOAT_KIND_NAN,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_inf_static = {
    .kind = MFLOAT_KIND_POSINF,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ninf_static = {
    .kind = MFLOAT_KIND_NEGINF,
    .sign = -1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .flags = MFLOAT_FLAG_IMMORTAL,
    .mantissa = NULL
};

static uint64_t mfloat_pi1024_storage[] = {
    0x3d35b9718e0cf535u, 0x9ede1b041f40a3e8u, 0x0a5545d68b8d4371u, 0xc5a5d4214278efa7u,
    0x1da034b3f62b2fb8u, 0xaf1edb0c0ffbae1cu, 0x066a4b776d5c5b6cu, 0x26f2e4a1b7fb424eu,
    0x2ac694de72f028ffu, 0xe76b027cfdfe66edu, 0xb65ed0f4dd5ac6a3u, 0x8a8bb965e3d9abe9u,
    0xe527b386ffa1ca2du, 0xf9726bf74082a81fu, 0x8cff46ca9a52c8abu, 0xc8d4711b3d85cad1u,
    0x99ebf06caba47b91u, 0xba734d22c7f51fa4u, 0x77e0b31b4906c38au, 0x8b67b8400f97142cu,
    0x63fcc9250cca3d9cu, 0x33f4b5d3e4822f89u, 0xd6fdbdc70d7f6b51u, 0xfdad617feb96de80u,
    0xcfd8de89885d34c6u, 0x3848bc90b6aecc4bu, 0xe286e9fc26adadaau, 0x48636605614dbe4bu,
    0x809bbdf2a33679a7u, 0x73644a29410f31c6u, 0xf98e804177d4c762u, 0x839a252049c1114cu,
    0x18469898cc51701bu, 0x00001921fb54442du
};
static uint64_t mfloat_e1024_storage[] = {
    0x033254b0cb54c1c7u, 0xbe57aef5c19813a0u, 0x4aa859e0bea7863cu, 0x4e7e6e78bcaee1b6u,
    0xc2498c03e9e71ec5u, 0x382c220ba0f2036eu, 0xf7a172c7491a654bu, 0x55efee3358d37eb0u,
    0x9b6ffc4c02d87c91u, 0xb2f9be400b5359bdu, 0x9a7d4aac598d5ae5u, 0xa938dd06579dd3ecu,
    0xe1577ff3ec4900f6u, 0x74fd23017594ab3du, 0xcd7344a9d6dba9dfu, 0x0dae75dd3c5aea8fu,
    0x1c7f772d5b56ec20u, 0x9f6c3a2115297659u, 0x4f2f578156305664u, 0xbce6a6159949e907u,
    0xfd0e43be2b1426d5u, 0xd16efc54d13b5e7du, 0xf926b309e18e1c1cu, 0xa35e76aae26bcfeau,
    0xcdda10ac6caaa7bdu, 0xacac24867ea3ebe0u, 0x8c2f5a7be3dababfu, 0x8ebb1ed0364055d8u,
    0x67df2fa5fc6c6c61u, 0x867f799273b9c493u, 0xa6d2b53c26c8228cu, 0xa79e3b1738b079c5u,
    0x695355fb8ac404e7u, 0x000015bf0a8b1457u
};
static uint64_t mfloat_gamma1024_storage[] = {
    0x07157049d78f1759u, 0xb0cb412d6a55c813u, 0xc781589028601cd8u, 0x939e94e76e4d99dfu,
    0x0956eda56cf63b6bu, 0x9259e420fe33c158u, 0x525f7907b4aa6dffu, 0x41bc162158d7f9c7u,
    0xd5d6ab34ba2e9dbbu, 0xb2c3ea6afdcf66a6u, 0x9c2706d90390affbu, 0x86a4c0f0f2b650a2u,
    0x8d7b054c736113cau, 0x7ecbd38ffe30586du, 0x89727d82448a5db2u, 0xe40c19d18ba0a7fcu,
    0xb427e3f0a19639f5u, 0xfa976d53f9c398f9u, 0x71d1a58550a8f38eu, 0x5dc8979ab0bc2f58u,
    0xfe190fb1f09c609eu, 0xee1bf4a87a87798bu, 0x6aab830275322dadu, 0xcdb3e2a5b4559e26u,
    0xb5ccf6f9efce0552u, 0xebfa9637ae1e3321u, 0xd682390ed19cf5d2u, 0x15f44415aba44c84u,
    0x7c3bb4192732d884u, 0xbfef6392d67e80eau, 0x30064300f7cd1c26u, 0xb2d5a873b30ebd97u,
    0x31e9346f8fe04054u, 0x000024f119f8df6cu
};
static uint64_t mfloat_sqrt_pi1024_storage[] = {
    0x4dd5b0b494a84e99u, 0xe2880092eeb7fc68u, 0xae6871f47474f728u, 0x42a41a74227f42a3u,
    0x7aae307974a2e3b5u, 0x76ecb0cfffbf574au, 0x9591e11b8be9ea26u, 0x7745fb2db2f56be8u,
    0xd68b0eccb4c4effdu, 0x1f75783760dfc140u, 0xcae5bb5523255143u, 0xfb59caff25ca248cu,
    0x7bd95fdce0968675u, 0xe3850d5ac2d20f90u, 0x398503060b2d278bu, 0xbccc9a4092cd1364u,
    0xa26f98db0102ed04u, 0xfdec59b7ca2d74b3u, 0x8e39e9867fed6ebau, 0x6db6048dd0729e22u,
    0x71387b27023d028fu, 0xbecea42e2c5c5e0du, 0x989b8b1c0bc49345u, 0xfa9b140caaa28446u,
    0x6ceb3ed54eb79196u, 0x085d372ebf7c4274u, 0x0d61454912430d29u, 0x31dd1db148511b77u,
    0x7c76eb3639d85078u, 0x51d1bb5dbff5be50u, 0xec94b728402f4fa8u, 0xe4e0ff8e48551bd8u,
    0xdaa9e70ec1483576u, 0x00000716fe246d3bu
};
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
static uint64_t mfloat_sqrt2_1024_storage[] = {
    0xa6c912abcd7d473du, 0x17116c2a40cbb896u, 0x4ffcd3051a73eb80u, 0x1fd65860c4575948u,
    0xede8e7a76aa772acu, 0x258e1238dd48bbd6u, 0x7b76641560957c6eu, 0x5ca8f7b5b0779173u,
    0xf46912e9d6daa8e7u, 0x15b1606967bb85a2u, 0xbd898ab34086a034u, 0x2ae8b92e295be293u,
    0x2b5c3167727c07b6u, 0x27ecb679944c4e70u, 0xb6b563c0b3636abeu, 0xfb12dc6d12b74f95u,
    0xd0efdf4d3a02cebau, 0x0ca4a81394ab6d8fu, 0xe582eeaa4a089904u, 0x3c84df52f120f836u,
    0x7e9dccb2a634331fu, 0xc4e33c6d5a8a38bbu, 0x8458a460abc722f7u, 0xc337bcab1bc91168u,
    0xf1f4e53059c6011bu, 0xa2768d2202e8742au, 0xc7b4a780487363dfu, 0x3db390f74a85e439u,
    0x8a2c3a8b1fe6fdc8u, 0x399154afc83043abu, 0xba84ced17ac85833u, 0xabe9f1d6f60ba893u,
    0xe6484597d89b3754u, 0x00000b504f333f9du
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

static struct _mint_t mfloat_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_pi1024_storage };
static struct _mint_t mfloat_e1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_e1024_storage };
static struct _mint_t mfloat_gamma1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_gamma1024_storage };
static struct _mint_t mfloat_sqrt_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt_pi1024_storage };
static struct _mint_t mfloat_half_ln_pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_half_ln_pi1024_storage };
static struct _mint_t mfloat_sqrt2_1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt2_1024_storage };
static struct _mint_t mfloat_half_ln_2pi1024_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_half_ln_2pi1024_storage };

static struct _mfloat_t mfloat_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_pi1024_mint };
static struct _mfloat_t mfloat_e1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_e1024_mint };
static struct _mfloat_t mfloat_gamma1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_gamma1024_mint };
static struct _mfloat_t mfloat_sqrt_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2154, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_sqrt_pi1024_mint };
static struct _mfloat_t mfloat_half_ln_pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_half_ln_pi1024_mint };
static struct _mfloat_t mfloat_sqrt2_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_sqrt2_1024_mint };
static struct _mfloat_t mfloat_half_ln_2pi1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_half_ln_2pi1024_mint };

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

static struct _mint_t mfloat_erf_half1024_mint = { .sign = 1, .length = 26, .capacity = 26, .storage = mfloat_erf_half1024_storage };
static struct _mint_t mfloat_erfinv_half1024_mint = { .sign = 1, .length = 26, .capacity = 26, .storage = mfloat_erfinv_half1024_storage };
static struct _mint_t mfloat_tetragamma1_1024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_tetragamma1_1024_storage };
static struct _mint_t mfloat_gammainv_min1024_mint = { .sign = 1, .length = 19, .capacity = 19, .storage = mfloat_gammainv_min1024_storage };
static struct _mint_t mfloat_gammainv_argmin1024_mint = { .sign = 1, .length = 21, .capacity = 21, .storage = mfloat_gammainv_argmin1024_storage };
static struct _mint_t mfloat_gammainv_31024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_gammainv_31024_storage };
static struct _mint_t mfloat_lambert_w0_11024_mint = { .sign = 1, .length = 25, .capacity = 25, .storage = mfloat_lambert_w0_11024_storage };

static struct _mfloat_t mfloat_erf_half1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1660, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_erf_half1024_mint };
static struct _mfloat_t mfloat_erfinv_half1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1659, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_erfinv_half1024_mint };
static struct _mfloat_t mfloat_tetragamma1_1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1590, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_tetragamma1_1024_mint };
static struct _mfloat_t mfloat_gammainv_min1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1185, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_gammainv_min1024_mint };
static struct _mfloat_t mfloat_gammainv_argmin1024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1288, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_gammainv_argmin1024_mint };
static struct _mfloat_t mfloat_gammainv_31024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1590, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_gammainv_31024_mint };
static struct _mfloat_t mfloat_lambert_w0_11024_static = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1590, .precision = 1024u, .flags = MFLOAT_FLAG_IMMORTAL, .mantissa = &mfloat_lambert_w0_11024_mint };

static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

const mfloat_t * const MF_ZERO = &mfloat_zero_static;
const mfloat_t * const MF_ONE = &mfloat_one_static;
const mfloat_t * const MF_HALF = &mfloat_half_static;
const mfloat_t * const MF_TENTH = &mfloat_tenth_static;
const mfloat_t * const MF_TEN = &mfloat_ten_static;
const mfloat_t * const MF_PI = &mfloat_pi1024_static;
const mfloat_t * const MF_E = &mfloat_e1024_static;
const mfloat_t * const MF_EULER_MASCHERONI = &mfloat_gamma1024_static;
const mfloat_t * const MF_SQRT2 = &mfloat_sqrt2_1024_static;
const mfloat_t * const MF_SQRT_PI = &mfloat_sqrt_pi1024_static;
const mfloat_t * const MFLOAT_INTERNAL_HALF_LN_PI = &mfloat_half_ln_pi1024_static;
const mfloat_t * const MFLOAT_INTERNAL_HALF_LN_2PI = &mfloat_half_ln_2pi1024_static;
const mfloat_t * const MFLOAT_INTERNAL_ERF_HALF = &mfloat_erf_half1024_static;
const mfloat_t * const MFLOAT_INTERNAL_ERFINV_HALF = &mfloat_erfinv_half1024_static;
const mfloat_t * const MFLOAT_INTERNAL_TETRAGAMMA_1 = &mfloat_tetragamma1_1024_static;
const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_MIN = &mfloat_gammainv_min1024_static;
const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_ARGMIN = &mfloat_gammainv_argmin1024_static;
const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_3 = &mfloat_gammainv_31024_static;
const mfloat_t * const MFLOAT_INTERNAL_LAMBERT_W0_1 = &mfloat_lambert_w0_11024_static;
const mfloat_t * const MF_NAN = &mfloat_nan_static;
const mfloat_t * const MF_INF = &mfloat_inf_static;
const mfloat_t * const MF_NINF = &mfloat_ninf_static;

static void mfloat_init_constants(void)
{
    static int initialised = 0;
    mfloat_t *tmp_tenth = NULL;

    if (initialised)
        return;
    initialised = 1;

    mfloat_zero_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_one_static.mantissa = (mint_t *)MI_ONE;
    mfloat_half_static.mantissa = (mint_t *)MI_ONE;
    mfloat_ten_static.mantissa = (mint_t *)MI_TEN;
    mfloat_nan_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_inf_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_ninf_static.mantissa = (mint_t *)MI_ZERO;

    tmp_tenth = mf_create_string("0.1");
    if (tmp_tenth) {
        mfloat_tenth_static.sign = tmp_tenth->sign;
        mfloat_tenth_static.exponent2 = tmp_tenth->exponent2;
        mfloat_tenth_static.precision = tmp_tenth->precision;
        mfloat_tenth_static.mantissa = tmp_tenth->mantissa;
        tmp_tenth->mantissa = NULL;
        free(tmp_tenth);
    } else {
        mfloat_tenth_static.mantissa = (mint_t *)MI_ZERO;
        mfloat_tenth_static.sign = 0;
        mfloat_tenth_static.exponent2 = 0;
    }
}

__attribute__((constructor))
static void mfloat_init_constants_ctor(void)
{
    mfloat_init_constants();
}

int mfloat_is_immortal(const mfloat_t *mfloat)
{
    return mfloat && (mfloat->flags & MFLOAT_FLAG_IMMORTAL) != 0u;
}

int mfloat_is_finite(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_FINITE;
}

int mfloat_normalise(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return -1;

    if (mi_is_zero(mfloat->mantissa)) {
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mfloat->kind = MFLOAT_KIND_FINITE;
        return 0;
    }

    while (mi_is_even(mfloat->mantissa)) {
        if (mi_shr(mfloat->mantissa, 1) != 0)
            return -1;
        mfloat->exponent2++;
    }

    if (mfloat->sign == 0)
        mfloat->sign = 1;
    return 0;
}

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat = calloc(1, sizeof(*mfloat));

    mfloat_init_constants();

    if (!mfloat)
        return NULL;

    mfloat->mantissa = mi_new();
    if (!mfloat->mantissa) {
        free(mfloat);
        return NULL;
    }

    mfloat->precision = precision_bits > 0 ? precision_bits
                                           : mfloat_default_precision_bits;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->flags = MFLOAT_FLAG_NONE;
    return mfloat;
}

static int mfloat_set_double_exact(mfloat_t *mfloat, double value)
{
    union {
        double d;
        uint64_t u;
    } bits;
    uint64_t frac;
    uint64_t mantissa_u64;
    int exp_bits;
    long exponent2;

    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (isinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = value < 0.0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF;
        mfloat->sign = value < 0.0 ? (short)-1 : (short)1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (value == 0.0) {
        mf_clear(mfloat);
        if (signbit(value))
            mfloat->sign = -0;
        return 0;
    }

    bits.d = value;
    frac = bits.u & ((UINT64_C(1) << 52) - 1u);
    exp_bits = (int)((bits.u >> 52) & 0x7ffu);

    if (exp_bits == 0) {
        mantissa_u64 = frac;
        exponent2 = -1074l;
    } else {
        mantissa_u64 = (UINT64_C(1) << 52) | frac;
        exponent2 = (long)exp_bits - 1023l - 52l;
    }

    if (mi_set_ulong(mfloat->mantissa, (unsigned long)mantissa_u64) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = (bits.u >> 63) ? (short)-1 : (short)1;
    mfloat->exponent2 = exponent2;
    return mfloat_normalise(mfloat);
}

int mfloat_copy_value(mfloat_t *dst, const mfloat_t *src)
{
    if (!dst || !src || !dst->mantissa || !src->mantissa)
        return -1;
    if (mi_clear(dst->mantissa), mi_add(dst->mantissa, src->mantissa) != 0)
        return -1;
    dst->kind = src->kind;
    dst->sign = src->sign;
    dst->exponent2 = src->exponent2;
    dst->precision = src->precision;
    return 0;
}

int mfloat_set_from_signed_mint(mfloat_t *dst, mint_t *value, long exponent2)
{
    if (!dst || !value || !dst->mantissa)
        return -1;
    if (mfloat_is_immortal(dst))
        return -1;

    if (mi_is_zero(value)) {
        mf_clear(dst);
        return 0;
    }

    dst->sign = mi_is_negative(value) ? (short)-1 : (short)1;
    if (dst->sign < 0 && mi_abs(value) != 0)
        return -1;

    mi_clear(dst->mantissa);
    if (mi_add(dst->mantissa, value) != 0)
        return -1;
    dst->kind = MFLOAT_KIND_FINITE;
    dst->exponent2 = exponent2;
    return mfloat_normalise(dst);
}

mint_t *mfloat_to_scaled_mint(const mfloat_t *mfloat, long target_exp)
{
    mint_t *value;
    long shift;

    if (!mfloat || !mfloat->mantissa)
        return NULL;
    if (!mfloat_is_finite(mfloat))
        return NULL;

    value = mi_clone(mfloat->mantissa);
    if (!value)
        return NULL;

    shift = mfloat->exponent2 - target_exp;
    if (shift > 0) {
        if (mi_shl(value, shift) != 0) {
            mi_free(value);
            return NULL;
        }
    } else if (shift < 0) {
        if (mi_shr(value, -shift) != 0) {
            mi_free(value);
            return NULL;
        }
    }

    if (mfloat->sign < 0 && mi_neg(value) != 0) {
        mi_free(value);
        return NULL;
    }

    return value;
}

static int mfloat_parse_decimal_components(const char *text,
                                           short *out_sign,
                                           mint_t *digits,
                                           long *out_exp10)
{
    const unsigned char *p = (const unsigned char *)text;
    short sign = 1;
    long frac_digits = 0;
    long exp10 = 0;
    bool seen_digit = false;
    bool seen_dot = false;

    if (!text || !out_sign || !digits || !out_exp10)
        return -1;

    while (isspace(*p))
        ++p;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        ++p;
    }

    if (mi_set_long(digits, 0) != 0)
        return -1;

    while (*p) {
        if (isdigit(*p)) {
            seen_digit = true;
            if (mi_mul_long(digits, 10) != 0 ||
                mi_add_long(digits, (long)(*p - '0')) != 0)
                return -1;
            if (seen_dot)
                frac_digits++;
            ++p;
            continue;
        }
        if (*p == '.' && !seen_dot) {
            seen_dot = true;
            ++p;
            continue;
        }
        break;
    }

    if (!seen_digit)
        return -1;

    if (*p == 'e' || *p == 'E') {
        bool neg_exp = false;
        long parsed = 0;

        ++p;
        if (*p == '+' || *p == '-') {
            neg_exp = (*p == '-');
            ++p;
        }
        if (!isdigit(*p))
            return -1;
        while (isdigit(*p)) {
            if (parsed > (LONG_MAX - 9) / 10)
                return -1;
            parsed = parsed * 10 + (long)(*p - '0');
            ++p;
        }
        exp10 = neg_exp ? -parsed : parsed;
    }

    while (isspace(*p))
        ++p;
    if (*p != '\0')
        return -1;

    *out_sign = sign;
    *out_exp10 = exp10 - frac_digits;
    return 0;
}

static int mfloat_set_from_decimal_parts(mfloat_t *mfloat,
                                         short sign,
                                         mint_t *digits,
                                         long exp10)
{
    mint_t *work = NULL, *factor = NULL, *q = NULL, *r = NULL, *twor = NULL;
    size_t shift_bits;
    int rc = -1;

    if (!mfloat || !digits || !mfloat->mantissa)
        return -1;

    if (mi_is_zero(digits)) {
        mf_clear(mfloat);
        return 0;
    }

    work = mi_clone(digits);
    if (!work)
        goto cleanup;

    if (exp10 >= 0) {
        factor = mi_create_long(5);
        if (!factor || mi_pow(factor, (unsigned long)exp10) != 0)
            goto cleanup;
        if (mi_mul(work, factor) != 0)
            goto cleanup;

        mi_clear(mfloat->mantissa);
        if (mi_add(mfloat->mantissa, work) != 0)
            goto cleanup;
        mfloat->kind = MFLOAT_KIND_FINITE;
        mfloat->sign = sign;
        mfloat->exponent2 = exp10;
        rc = mfloat_normalise(mfloat);
        goto cleanup;
    }

    factor = mi_create_long(5);
    if (!factor || mi_pow(factor, (unsigned long)(-exp10)) != 0)
        goto cleanup;

    shift_bits = mfloat->precision + mi_bit_length(factor) + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(work, (long)shift_bits) != 0)
        goto cleanup;

    q = mi_new();
    r = mi_new();
    if (!q || !r)
        goto cleanup;
    if (mi_divmod(work, factor, q, r) != 0)
        goto cleanup;

    twor = mi_clone(r);
    if (!twor || mi_mul_long(twor, 2) != 0)
        goto cleanup;
    if (mi_cmp(twor, factor) >= 0) {
        if (mi_inc(q) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, q) != 0)
        goto cleanup;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = sign;
    mfloat->exponent2 = exp10 - (long)shift_bits;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(work);
    mi_free(factor);
    mi_free(q);
    mi_free(r);
    mi_free(twor);
    return rc;
}

mfloat_t *mf_new(void)
{
    return mf_new_prec(mfloat_default_precision_bits);
}

mfloat_t *mf_new_prec(size_t precision_bits)
{
    mfloat_init_constants();
    return mfloat_alloc(precision_bits);
}

size_t mfloat_get_default_precision_internal(void)
{
    return mfloat_default_precision_bits;
}

static int mfloat_set_from_const_mint_internal(mfloat_t *dst, const mint_t *src, long exponent2)
{
    mint_t *tmp;
    int rc;

    if (!dst || !src)
        return -1;
    tmp = mi_clone(src);
    if (!tmp)
        return -1;
    rc = mfloat_set_from_signed_mint(dst, tmp, exponent2);
    mi_free(tmp);
    return rc;
}

static int mfloat_make_const_rational_internal(mfloat_t *dst,
                                               const mint_t *num,
                                               const mint_t *den,
                                               size_t precision)
{
    mfloat_t *n = NULL;
    mfloat_t *d = NULL;
    int rc = -1;

    if (!dst || !num || !den)
        return -1;
    n = mf_new_prec(precision);
    d = mf_new_prec(precision);
    if (!n || !d ||
        mfloat_set_from_const_mint_internal(n, num, 0) != 0 ||
        mfloat_set_from_const_mint_internal(d, den, 0) != 0 ||
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

mfloat_t *mfloat_clone_immortal_prec_internal(const mfloat_t *src, size_t precision)
{
    mfloat_t *dst;

    if (!src)
        return NULL;
    dst = mf_new_prec(precision);
    if (!dst)
        return NULL;
    if (mfloat_copy_value(dst, src) != 0) {
        mf_free(dst);
        return NULL;
    }
    dst->precision = precision;
    return dst;
}

int mfloat_set_from_immortal_internal(mfloat_t *dst, const mfloat_t *src, size_t precision)
{
    mfloat_t *tmp = mfloat_clone_immortal_prec_internal(src, precision);
    int rc;

    if (!tmp)
        return -1;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;
    mf_free(tmp);
    return rc;
}

int mfloat_copy_lgamma_asymptotic_term_internal(mfloat_t *dst, size_t index, size_t precision)
{
    if (!dst || index >= sizeof(mfloat_lgamma_asymptotic_terms) / sizeof(mfloat_lgamma_asymptotic_terms[0]))
        return -1;
    return mfloat_make_const_rational_internal(dst,
                                               mfloat_lgamma_asymptotic_terms[index].num,
                                               mfloat_lgamma_asymptotic_terms[index].den,
                                               precision);
}

int mfloat_mul_euler_gamma_coeff_internal(mfloat_t *mfloat, size_t index, size_t precision)
{
    mfloat_t *factor = NULL;
    int rc = -1;

    if (!mfloat || index >= sizeof(mfloat_euler_gamma_coeffs) / sizeof(mfloat_euler_gamma_coeffs[0]))
        return -1;
    factor = mf_new_prec(precision);
    if (!factor ||
        mfloat_make_const_rational_internal(factor,
                                            mfloat_euler_gamma_coeffs[index].num,
                                            mfloat_euler_gamma_coeffs[index].den,
                                            precision) != 0)
        goto cleanup;
    rc = mf_mul(mfloat, factor);

cleanup:
    mf_free(factor);
    return rc;
}

mfloat_t *mf_create_long(long value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_double(double value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_double(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_qfloat(qfloat_t value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_qfloat(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_clone(const mfloat_t *mfloat)
{
    mfloat_t *copy;

    if (!mfloat)
        return NULL;
    copy = mf_new_prec(mfloat->precision);
    if (!copy)
        return NULL;
    if (mfloat_copy_value(copy, mfloat) != 0) {
        mf_free(copy);
        return NULL;
    }
    return copy;
}

void mf_free(mfloat_t *mfloat)
{
    if (!mfloat)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_free(mfloat->mantissa);
    free(mfloat);
}

void mf_clear(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_clear(mfloat->mantissa);
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = 0;
    mfloat->exponent2 = 0;
}

int mf_set_precision(mfloat_t *mfloat, size_t precision_bits)
{
    if (!mfloat || precision_bits == 0)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    mfloat->precision = precision_bits;
    return 0;
}

int mf_set_default_precision(size_t precision_bits)
{
    if (precision_bits == 0)
        return -1;
    mfloat_default_precision_bits = precision_bits;
    return 0;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->precision : 0;
}

int mf_set_long(mfloat_t *mfloat, long value)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (value == 0) {
        mf_clear(mfloat);
        return 0;
    }

    if (mi_set_long(mfloat->mantissa, value < 0 ? -value : value) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = value < 0 ? (short)-1 : (short)1;
    mfloat->exponent2 = 0;
    return mfloat_normalise(mfloat);
}

int mf_set_double(mfloat_t *mfloat, double value)
{
    return mfloat_set_double_exact(mfloat, value);
}

int mf_set_qfloat(mfloat_t *mfloat, qfloat_t value)
{
    mfloat_t *tmp = NULL;
    int rc;

    if (!mfloat)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    if (qf_isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isposinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isneginf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        return 0;
    }

    rc = mfloat_set_double_exact(mfloat, value.hi);
    if (rc != 0)
        return rc;

    if (value.lo == 0.0)
        return 0;

    tmp = mf_create_double(value.lo);
    if (!tmp)
        return -1;
    if (mf_set_precision(tmp, mfloat->precision) != 0) {
        mf_free(tmp);
        return -1;
    }

    rc = mf_add(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_set_string(mfloat_t *mfloat, const char *text)
{
    mint_t *digits = NULL;
    short sign = 1;
    long exp10 = 0;
    int rc;

    if (!mfloat || !text)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (text[0] == 'N' && text[1] == 'A' && text[2] == 'N' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == 'I' && text[1] == 'N' && text[2] == 'F' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == '-' && text[1] == 'I' && text[2] == 'N' && text[3] == 'F' && text[4] == '\0') {
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }

    digits = mi_new();
    if (!digits)
        return -1;

    rc = mfloat_parse_decimal_components(text, &sign, digits, &exp10);
    if (rc == 0)
        rc = mfloat_set_from_decimal_parts(mfloat, sign, digits, exp10);

    mi_free(digits);
    return rc;
}

bool mf_is_zero(const mfloat_t *mfloat)
{
    if (!mfloat_is_finite(mfloat))
        return false;
    return !mfloat || mfloat->sign == 0 || !mfloat->mantissa ||
           mi_is_zero(mfloat->mantissa);
}

short mf_get_sign(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->sign : 0;
}

long mf_get_exponent2(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->exponent2 : 0;
}

size_t mf_get_mantissa_bits(const mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return 0;
    return mi_bit_length(mfloat->mantissa);
}

bool mf_get_mantissa_u64(const mfloat_t *mfloat, uint64_t *out)
{
    long value;

    if (!mfloat || !out || !mfloat->mantissa || mi_is_negative(mfloat->mantissa))
        return false;
    if (!mfloat_is_finite(mfloat))
        return false;
    if (mi_bit_length(mfloat->mantissa) > (sizeof(long) * 8u - 1u))
        return false;
    if (!mi_get_long(mfloat->mantissa, &value) || value < 0)
        return false;
    *out = (uint64_t)value;
    return true;
}
