#include "mfloat_internal.h"
#include "internal/mint_internal.h"

/* Small exact immortal mantissas. */
static uint64_t mfloat_one_storage[] = { 1u };
static uint64_t mfloat_ten_storage[] = { 10u };

static struct _mint_t mfloat_zero_mint = {
    .sign = 0,
    .length = 0,
    .capacity = 0,
    .storage = NULL
};

static struct _mint_t mfloat_one_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mfloat_one_storage
};

static struct _mint_t mfloat_ten_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mfloat_ten_storage
};

const mfloat_t MF_ZERO_VALUE = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_zero_mint
};

const mfloat_t MF_ONE_VALUE = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_one_mint
};

const mfloat_t MF_HALF_VALUE = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_one_mint
};

const mfloat_t MF_TEN_VALUE = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_ten_mint
};

const mfloat_t MF_NAN_VALUE = {
    .kind = MFLOAT_KIND_NAN,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_zero_mint
};

const mfloat_t MF_INF_VALUE = {
    .kind = MFLOAT_KIND_POSINF,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_zero_mint
};

const mfloat_t MF_NINF_VALUE = {
    .kind = MFLOAT_KIND_NEGINF,
    .sign = -1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .immortal = true,
    .mantissa = &mfloat_zero_mint
};

/* High-precision immortal seeds for named inexact constants. */
static uint64_t mfloat_pi_storage[] = {
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
static uint64_t mfloat_e_storage[] = {
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
static uint64_t mfloat_gamma_storage[] = {
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
static uint64_t mfloat_phi_storage[] = {
    0x80eb2fae46559566u, 0x6c15fdbec758d20cu, 0xe77de3909a767837u, 0x030d58e0e5778f5eu,
    0x5ca01a738790b046u, 0x63417a7c0819055fu, 0xac4054e85669dcf4u, 0x37254333b48a2fafu,
    0xee551115613474b3u, 0x7cd16a628a78d914u, 0x2229506ea06e465cu, 0x6987a242db8d477bu,
    0x0755201c59a96cddu, 0x4ec33a7d0a8420cfu, 0x474bad1bc0f580dau, 0x491d39e5c8b24721u,
    0x526d943d72279997u, 0x57372f39d4dc8f9du, 0xec152ff6283b82f2u, 0xa7bcea38e2559f69u,
    0x5a198ddfb9e3c86cu, 0xfddf0b13719dc682u, 0xb8b41656e86667efu, 0xebee3326992e8e29u,
    0xbc3bf42c1ceb77fdu, 0x9af1b78356bd74b8u, 0x0180160eb25d2079u, 0x8c13f80c43784942u,
    0x93dbf81a3822dadfu, 0x0c74a93b3f858a9eu, 0x13928fc363508e86u, 0x6e41a084113b5f9du,
    0x53e0af9ce60302e7u, 0x00000cf1bbcdcbfau
};
static uint64_t mfloat_sqrt_pi_storage[] = {
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
static uint64_t mfloat_sqrt2_storage[] = {
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
static uint64_t mfloat_sqrt3_storage[] = {
    0x45f0d84b7f33bd3cu, 0xc61231aeb30e7da3u, 0xd673164192e76868u, 0x69a91294d3934d78u,
    0x0eeea05534dda9a9u, 0x46989d9c3d7006bdu, 0x91db33c09efa105bu, 0xafe8ca1737ea0fceu,
    0xe7a0c4779bbe96adu, 0x0c57b2e087c03547u, 0x1088a5361b5955b8u, 0x63498624ea47fe6eu,
    0xb259ced697cfbb93u, 0xb217bb71647bedf6u, 0x16c21a0ca5558b26u, 0x9e06ab34b501cecbu,
    0xd8f60dd7ac784221u, 0x981282ef80a63856u, 0x79bb452e3e5c86aau, 0xc4c9aafbf88e1f12u,
    0x4955ab940b6677e2u, 0xff2bb94a1379872cu, 0xd84feb75f799d4d4u, 0x6db980db5faa9d7bu,
    0x61d736f2f6f1dea1u, 0xcd1c1dcf0917309cu, 0x0f634686699d00d6u, 0x147c3e6267926d1du,
    0x8aeded4c98557091u, 0x2d3712485e7ecaf7u, 0xd23cc63905324372u, 0xc1dc492ec1a6629eu,
    0x5539d92ba16b83c5u, 0x00000ddb3d742c26u
};
static uint64_t mfloat_tenth_storage[] = {
    0xcccccccccccccccdu, 0xccccccccccccccccu, 0xccccccccccccccccu, 0xccccccccccccccccu,
    0x000000000000000cu
};

static struct _mint_t mfloat_pi_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_pi_storage };
static struct _mint_t mfloat_e_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_e_storage };
static struct _mint_t mfloat_gamma_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_gamma_storage };
static struct _mint_t mfloat_phi_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_phi_storage };
static struct _mint_t mfloat_sqrt_pi_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt_pi_storage };
static struct _mint_t mfloat_sqrt2_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt2_storage };
static struct _mint_t mfloat_sqrt3_mint = { .sign = 1, .length = 34, .capacity = 34, .storage = mfloat_sqrt3_storage };
static struct _mint_t mfloat_tenth_mint = { .sign = 1, .length = 5u, .capacity = 5u, .storage = mfloat_tenth_storage };

static uint64_t MF_2PI_storage[] = {
    0xd348b1fd47e9267bu, 0x2cc6d241b0e2ae9cu, 0xee1003e5c50b1df8u, 0x324943328f6722d9u,
    0x2d74f9208be258ffu, 0x6f71c35fdad44cfdu, 0x585ffae5b7a035bfu, 0x37a262174d31bf6bu,
    0x2f242dabb312f3f6u, 0xba7f09ab6b6a8e12u, 0xd98158536f92f8a1u, 0xef7ca8cd9e69d218u,
    0x128a5043cc71a026u, 0xa0105df531d89cd9u, 0x8948127044533e63u, 0xa62633145c06e0e6u,
    0x06487ed5110b4611u
};
static struct _mint_t MF_2PI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_2PI_storage };

static uint64_t MF_PI_2_storage[] = {
    0xd348b1fd47e9267bu, 0x2cc6d241b0e2ae9cu, 0xee1003e5c50b1df8u, 0x324943328f6722d9u,
    0x2d74f9208be258ffu, 0x6f71c35fdad44cfdu, 0x585ffae5b7a035bfu, 0x37a262174d31bf6bu,
    0x2f242dabb312f3f6u, 0xba7f09ab6b6a8e12u, 0xd98158536f92f8a1u, 0xef7ca8cd9e69d218u,
    0x128a5043cc71a026u, 0xa0105df531d89cd9u, 0x8948127044533e63u, 0xa62633145c06e0e6u,
    0x06487ed5110b4611u
};
static struct _mint_t MF_PI_2_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_PI_2_storage };

static uint64_t MF_PI_4_storage[] = {
    0xd348b1fd47e9267bu, 0x2cc6d241b0e2ae9cu, 0xee1003e5c50b1df8u, 0x324943328f6722d9u,
    0x2d74f9208be258ffu, 0x6f71c35fdad44cfdu, 0x585ffae5b7a035bfu, 0x37a262174d31bf6bu,
    0x2f242dabb312f3f6u, 0xba7f09ab6b6a8e12u, 0xd98158536f92f8a1u, 0xef7ca8cd9e69d218u,
    0x128a5043cc71a026u, 0xa0105df531d89cd9u, 0x8948127044533e63u, 0xa62633145c06e0e6u,
    0x06487ed5110b4611u
};
static struct _mint_t MF_PI_4_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_PI_4_storage };

static uint64_t MF_3PI_4_storage[] = {
    0x79da15f7d7bb7371u, 0x865476c512a80bd6u, 0xca300bb14f2159e8u, 0x96dbc997ae35688du,
    0x885eeb61a3a70afdu, 0x4e554a1f907ce6f7u, 0x091ff0b126e0a13eu, 0xa6e72645e7953e42u,
    0x8d6c89031938dbe2u, 0x2f7d1d02423faa36u, 0x8c8408fa4eb8e9e5u, 0xce75fa68db3d764au,
    0x379ef0cb6554e074u, 0xe03119df9589d68bu, 0x9bd83750ccf9bb2au, 0xf272993d1414a2b3u,
    0x12d97c7f3321d234u
};
static struct _mint_t MF_3PI_4_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_3PI_4_storage };

static uint64_t MF_PI_6_storage[] = {
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x460ed51b536ddf95u, 0x10918579683937bcu, 0x8155a87b16427f59u,
    0x30c5998bf342e77eu, 0x746a180ba8321544u, 0x9ed047fce7066a6eu, 0x5d54e879f8047a9eu,
    0xa2dd81f1197a9e47u, 0x985923a441945484u, 0x8a96239e48e12c2eu, 0xcac75c494c3f62cfu,
    0x9fb8bbcd337c2cbcu, 0x8b86b0510978033eu, 0x56b27f197cb7bcc1u, 0x70ac3405b19a884du,
    0x2dd99707ab3d688bu, 0x860a91c16b9b2c23u
};
static struct _mint_t MF_PI_6_mint = { .sign = 1, .length = 34u, .capacity = 34u, .storage = MF_PI_6_storage };

static uint64_t MF_PI_3_storage[] = {
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u, 0x5555555555555555u,
    0x5555555555555555u, 0x460ed51b536ddf95u, 0x10918579683937bcu, 0x8155a87b16427f59u,
    0x30c5998bf342e77eu, 0x746a180ba8321544u, 0x9ed047fce7066a6eu, 0x5d54e879f8047a9eu,
    0xa2dd81f1197a9e47u, 0x985923a441945484u, 0x8a96239e48e12c2eu, 0xcac75c494c3f62cfu,
    0x9fb8bbcd337c2cbcu, 0x8b86b0510978033eu, 0x56b27f197cb7bcc1u, 0x70ac3405b19a884du,
    0x2dd99707ab3d688bu, 0x860a91c16b9b2c23u
};
static struct _mint_t MF_PI_3_mint = { .sign = 1, .length = 34u, .capacity = 34u, .storage = MF_PI_3_storage };

static uint64_t MF_2_PI_storage[] = {
    0xa0e73ef14a525d41u, 0xa4f758fd7cbe2f67u, 0xe8c7ecd3cbfd45aeu, 0xa6cfd9e4f96136e9u,
    0xe5e2316b414da3edu, 0xff12fffbc0b301fdu, 0x908bf177bf250763u, 0x323ac7306a673e93u,
    0x338e04d68befc827u, 0x1046bea5d768909du, 0xc3bd63962534e7ddu, 0xc925dd413a32439fu,
    0xe48dc74849ba5c00u, 0xca2c757bd778ac36u, 0x6c52b3278872083fu, 0x84eafa3ea69bb81bu,
    0x5f306dc9c882a53fu, 0x0000000000000014u
};
static struct _mint_t MF_2_PI_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = MF_2_PI_storage };

static uint64_t MF_INV_E_storage[] = {
    0x9a89798666d9d76bu, 0x77cab9a636f0058cu, 0xdbb2311a392ad4b4u, 0xbab8371b2189defau,
    0xf5037d46ec9da9efu, 0xa55bc9cb1a22486cu, 0x10f87e7fc888c4d9u, 0x24bfb7ab019e29b8u,
    0x1d85677b75524e1bu, 0x3e078c4edb954b18u, 0x5d4aaa12d1b87245u, 0x048b6cd3b21a4f4bu,
    0x2c8017e8e56842b7u, 0x04973a14a0fb5db6u, 0x5300b556ad8ee666u, 0x5bd8f0520a9f21bbu,
    0xc5ab1b16779be357u, 0x000000000000000bu
};
static struct _mint_t MF_INV_E_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = MF_INV_E_storage };

static uint64_t MF_LN2_storage[] = {
    0x607f4ca11fb5bfb9u, 0x2da2d97c50f3fd5cu, 0x8655fa1872f20e3au, 0xaf5dfa6bd3830324u,
    0xb72ce87b19d6548cu, 0xb256fa0ec7657f74u, 0xeb9ea9bc3b136603u, 0x51acbda11317c387u,
    0x53e96ca16224ae8cu, 0x427573b291169b82u, 0x0ed2eae35c138214u, 0x8559552fb4afa1b1u,
    0xbe7b876206debac9u, 0xd8a0d175b8baafa2u, 0xf40f343267298b62u, 0xbc9e3b39803f2f6au,
    0x0b17217f7d1cf79au
};
static struct _mint_t MF_LN2_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_LN2_storage };

static uint64_t MF_INVLN2_storage[] = {
    0xca16da20b1d74a1fu, 0x25e11f75c6142e64u, 0xcfc406b19abb71ecu, 0xf199e108cf392819u,
    0x3352906deb692ce4u, 0xa39e8af56c64a783u, 0x6bd777e75050a8d1u, 0x43687aaf3ab440c1u,
    0xfc529264c2fb3ab6u, 0x97f5e06a7be73665u, 0x52173cc1895213f8u, 0x9b25eeb82d7c167du,
    0xc3887eeaa2ed9ac4u, 0x64a2cd9a342648fbu, 0x6aef551bad2b4b11u, 0xd0ffda0d23a7d11du,
    0x1547652b82fe1777u, 0x0000000000000017u
};
static struct _mint_t MF_INVLN2_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = MF_INVLN2_storage };

static uint64_t MF_SQRT_HALF_storage[] = {
    0x409ca55b6c7e877fu, 0x75525044c8206525u, 0xfa978907c1b72c17u, 0x659531a198f9e426u,
    0xe36ad451c5dbf4eeu, 0x23055e3917be2719u, 0xe558de488b4422c5u, 0x2982ce3008de19bdu,
    0x69101743a1578fa7u, 0x3c02439b1efd13b4u, 0x87ba542f21ce3da5u, 0xd458ff37ee41ed9cu,
    0xa57e41821d5c5161u, 0x768bd642c199cc8au, 0x8eb7b05d449dd426u, 0x2cbec4d9baa55f4fu,
    0x5a827999fcef3242u
};
static struct _mint_t MF_SQRT_HALF_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_SQRT_HALF_storage };

static uint64_t MF_SQRT_2PI_storage[] = {
    0x39d369d11273654fu, 0xb3d7ddc90a180f92u, 0xcfb2e914491cd62au, 0x8e54bbf3aeaab611u,
    0x793d53d338c2c472u, 0x0712b3d1352e9a5du, 0xaa8d4dea9bc022adu, 0x5a0c76ca1f36dbdfu,
    0xc99b03aaabb49478u, 0x3e36ef4a0c2d8327u, 0xbbe3d296dcd5aa3cu, 0x50273dfe814756d0u,
    0x66fb90a92271f44cu, 0xf0e719fb30988c30u, 0xa07469d7f40629e9u, 0xcb2449fd5a8ad79au,
    0x06f72e4316b3a550u, 0xb27fe966325888c9u, 0xf50a85f070ae1aebu, 0xe2063ee60c1fc3cdu,
    0xd95a61c9a11f5582u, 0x77bae522d41a0cdeu, 0x058002d4370bd04bu, 0x39167717c67cfa99u,
    0xb1382cb2be520fd7u, 0x00000000a06c98ffu
};
static struct _mint_t MF_SQRT_2PI_mint = { .sign = 1, .length = 26u, .capacity = 26u, .storage = MF_SQRT_2PI_storage };

static uint64_t MF_SQRT_PI_OVER_TWO_storage[] = {
    0x36f945ae96744df3u, 0x30cffdadd75fbd8bu, 0x91ba0e53c451c73du, 0x64e047a051edb6c0u,
    0x85c58b8bc1ae270fu, 0x6381789268b7d9d4u, 0x8195545088d31371u, 0xdaa9d6f232df5362u,
    0xe5d7ef884e8d9d67u, 0xa9224861a5210ba6u, 0xb6290a236ee1ac28u, 0x66c73b0a0f063ba3u,
    0x6bb7feb7ca0f8eddu, 0xe50805e9f50a3a37u, 0xf1c90aa37b1d9296u, 0xe1d82906aedc9c1fu,
    0xe2dfc48da77b553cu
};
static struct _mint_t MF_SQRT_PI_OVER_TWO_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_SQRT_PI_OVER_TWO_storage };

static uint64_t MF_SQRT1ONPI_storage[] = {
    0xd69ce7c336e84b15u, 0xc69f48d6e7fb601cu, 0x83aa5a04f4a69968u, 0x616445ddd40d8e3au,
    0x5cff5921b7979395u, 0x93d19416d6d398b7u, 0xa91797c9f0b5951eu, 0xe117a3fbdbfdabe4u,
    0x92b0ee61226e32f4u, 0xa7bb7c3bfff6128au, 0xe8fa7772470e53c3u, 0x0601b04b663cd75du,
    0x03ace7c2c938788au, 0xcd0ac18667310586u, 0xa04d075d61f3a8b9u, 0x8ea453fb5ff61a20u,
    0x8375d410a6db446bu, 0x0000000000000004u
};
static struct _mint_t MF_SQRT1ONPI_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = MF_SQRT1ONPI_storage };

static uint64_t MF_2_SQRTPI_storage[] = {
    0x9c24b8e071c879cdu, 0x18d3e91adcff6c03u, 0x50754b409e94d32du, 0xac2c88bbba81b1c7u,
    0xeb9feb2436f2f272u, 0xd27a3282dada7316u, 0x9522f2f93e16b2a3u, 0x9c22f47f7b7fb57cu,
    0x52561dcc244dc65eu, 0x74f76f877ffec251u, 0xbd1f4eee48e1ca78u, 0x40c036096cc79aebu,
    0xc0759cf859270f11u, 0x39a15830cce620b0u, 0x1409a0ebac3e7517u, 0x71d48a7f6bfec344u,
    0x906eba8214db688du
};
static struct _mint_t MF_2_SQRTPI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_2_SQRTPI_storage };

static uint64_t MF_NEG_TWO_OVER_SQRT_PI_storage[] = {
    0x9c24b8e071c879cdu, 0x18d3e91adcff6c03u, 0x50754b409e94d32du, 0xac2c88bbba81b1c7u,
    0xeb9feb2436f2f272u, 0xd27a3282dada7316u, 0x9522f2f93e16b2a3u, 0x9c22f47f7b7fb57cu,
    0x52561dcc244dc65eu, 0x74f76f877ffec251u, 0xbd1f4eee48e1ca78u, 0x40c036096cc79aebu,
    0xc0759cf859270f11u, 0x39a15830cce620b0u, 0x1409a0ebac3e7517u, 0x71d48a7f6bfec344u,
    0x906eba8214db688du
};
static struct _mint_t MF_NEG_TWO_OVER_SQRT_PI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_NEG_TWO_OVER_SQRT_PI_storage };

static uint64_t MF_INV_SQRT_2PI_storage[] = {
    0x03624a3988c48b41u, 0x43109aeed2521f92u, 0x34aac90d7b917245u, 0xf87d9c20ca6acff2u,
    0x073b1220b1deab3bu, 0x9da4dec6f159da47u, 0xb2d51379a7ba8cb0u, 0xb36f5b99708ce572u,
    0x0a0b87f651f392d3u, 0xe3a64d117c233fe6u, 0xc509de5c2f8544c9u, 0xfd74eaecdd3a237au,
    0x7e2b15511f4f6dc8u, 0x11c6d20098e74ba7u, 0xe0a7cc1449c1b630u, 0xcb3c500bab8e2ff4u,
    0x884533d436508d0fu, 0x0000000000000019u
};
static struct _mint_t MF_INV_SQRT_2PI_mint = { .sign = 1, .length = 18u, .capacity = 18u, .storage = MF_INV_SQRT_2PI_storage };

static uint64_t MF_LOG_SQRT_2PI_storage[] = {
    0x3e2d864390f07733u, 0x3bd6e6fba48aa194u, 0x54b6d36bee63e04au, 0xc525605f70bb125eu,
    0xded77fbec954a0afu, 0x27086c366978e17eu, 0x9254d1304a59fb7eu, 0x307d867635c11696u,
    0x926770eca54487a7u, 0xcf66ece1772badf2u, 0xb05cab571b4cda5bu, 0x93eabf905c5569bbu,
    0x212f9d7fe00e86bfu, 0xdec6a3133daa155du, 0xcfb08f8d13458b4du, 0x94bc900144192023u,
    0xeb3f8e4325f5a534u
};
static struct _mint_t MF_LOG_SQRT_2PI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_LOG_SQRT_2PI_storage };

static uint64_t MF_LN_2PI_storage[] = {
    0x3e2d864390f07733u, 0x3bd6e6fba48aa194u, 0x54b6d36bee63e04au, 0xc525605f70bb125eu,
    0xded77fbec954a0afu, 0x27086c366978e17eu, 0x9254d1304a59fb7eu, 0x307d867635c11696u,
    0x926770eca54487a7u, 0xcf66ece1772badf2u, 0xb05cab571b4cda5bu, 0x93eabf905c5569bbu,
    0x212f9d7fe00e86bfu, 0xdec6a3133daa155du, 0xcfb08f8d13458b4du, 0x94bc900144192023u,
    0xeb3f8e4325f5a534u
};
static struct _mint_t MF_LN_2PI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_LN_2PI_storage };

static uint64_t MF_PI_SQUARED_storage[] = {
    0x056f4ccd4baebf19u, 0x5ccd0783c4651a84u, 0xca093b101cf84b2au, 0x02b3b4f60e940babu,
    0x50ef4458a29da30cu, 0x88b3cdc26e966badu, 0xd913043cbc583213u, 0x925574293bcf3a32u,
    0x0caa99af4e40f987u, 0xd4b10ebc35e41e6fu, 0x6273b286e4bf158du, 0x419de38d6ffd25cau,
    0x697949f45ef7a4f4u, 0x28a1681d178ca6d7u, 0xade684a3c3198774u, 0x86e87a2832cf37ceu,
    0x08b29b7ca3ec5ff2u, 0x23eab49471e79a7bu, 0x8015a8a589e6b7acu, 0x02888743aa356034u,
    0x6e69c4dcb66d7a2fu, 0xa49cedead7403d29u, 0xbb1eb7c1da482003u, 0xb1377f3662ff4277u,
    0xbd0da0188915b798u, 0xa0bcad33aeed9fcau, 0x924953331f44c65eu, 0xf8bffb14f2c2d442u,
    0x544a173de83c2500u, 0xa3dc426da61174c4u, 0x8a19a0884094f1cdu, 0xb1c2159a8ff83428u,
    0xb495b89b36602306u, 0x00277a79937c8bbcu
};
static struct _mint_t MF_PI_SQUARED_mint = { .sign = 1, .length = 34u, .capacity = 34u, .storage = MF_PI_SQUARED_storage };

static uint64_t MF_2PI_CUBED_storage[] = {
    0x396ffddafdf890ddu, 0x808231a5f532b75bu, 0x0d485c9e8f33cb07u, 0x97c31c966401d07eu,
    0x1cd53570a9428391u, 0x61e8243dbf5bcac0u, 0xc89ff449224152cdu, 0xf38414fd08678aa9u,
    0x6e3c274066103b73u, 0xda7601646986d79eu, 0x396048cb56354cbbu, 0x5cd0a4571ab72f3fu,
    0xee4f17d75b60910fu, 0x6b30b2582e2482cau, 0x8b6b0525dd52463du, 0x4f6386d7ee05f084u,
    0x8ffddd794024d7c2u, 0x7b99f9e06a971f83u, 0x02efac59bede5dcau, 0x770ba3dd608e4eefu,
    0x3715f2af7a21d8ccu, 0x8897e63d65095d3eu, 0xefa622dae824da7au, 0x6647536b0d91dd1eu,
    0x85243e38fd514f20u, 0x47b38a4636ed8356u, 0x97514b2021cb57bcu, 0x802e851f5173573bu,
    0x7c29d61d4dc7d078u, 0xda9a80f82f6615b6u, 0xc9b746bfe4c7891du, 0x70aa83ae201be952u,
    0x399a81e2524b6620u, 0xf00caac729c06b6cu, 0x00f80cdac9c4ebe0u
};
static struct _mint_t MF_2PI_CUBED_mint = { .sign = 1, .length = 35u, .capacity = 35u, .storage = MF_2PI_CUBED_storage };

/* Canonical immortal mfloat values. */
const mfloat_t MF_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1088u, .immortal = true, .mantissa = &mfloat_pi_mint };
const mfloat_t MF_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1080, .precision = 1088u, .immortal = true, .mantissa = &MF_2PI_mint };
const mfloat_t MF_PI_2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1082, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_2_mint };
const mfloat_t MF_PI_4_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1083, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_4_mint };
const mfloat_t MF_3PI_4_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1083, .precision = 1088u, .immortal = true, .mantissa = &MF_3PI_4_mint };
const mfloat_t MF_PI_6_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2176, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_6_mint };
const mfloat_t MF_PI_3_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2175, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_3_mint };
const mfloat_t MF_2_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1093, .precision = 1088u, .immortal = true, .mantissa = &MF_2_PI_mint };
const mfloat_t MF_E_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1088u, .immortal = true, .mantissa = &mfloat_e_mint };
const mfloat_t MF_INV_E_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1093, .precision = 1088u, .immortal = true, .mantissa = &MF_INV_E_mint };
const mfloat_t MF_LN2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1084, .precision = 1088u, .immortal = true, .mantissa = &MF_LN2_mint };
const mfloat_t MF_INVLN2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1092, .precision = 1088u, .immortal = true, .mantissa = &MF_INVLN2_mint };
const mfloat_t MF_EULER_MASCHERONI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1088u, .immortal = true, .mantissa = &mfloat_gamma_mint };
const mfloat_t MF_PHI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1088u, .immortal = true, .mantissa = &mfloat_phi_mint };
const mfloat_t MF_SQRT_HALF_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_SQRT_HALF_mint };
const mfloat_t MF_SQRT_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2154, .precision = 1088u, .immortal = true, .mantissa = &mfloat_sqrt_pi_mint };
const mfloat_t MF_SQRT2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1088u, .immortal = true, .mantissa = &mfloat_sqrt2_mint };
const mfloat_t MF_SQRT3_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1088u, .immortal = true, .mantissa = &mfloat_sqrt3_mint };
const mfloat_t MF_SQRT2_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2156, .precision = 1088u, .immortal = true, .mantissa = &mfloat_sqrt2_mint };
const mfloat_t MF_SQRT3_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2156, .precision = 1088u, .immortal = true, .mantissa = &mfloat_sqrt3_mint };
const mfloat_t MF_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1630, .precision = 1088u, .immortal = true, .mantissa = &MF_SQRT_2PI_mint };
const mfloat_t MF_SQRT_PI_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1536, .precision = 1088u, .immortal = true, .mantissa = &MF_SQRT_PI_OVER_TWO_mint };
const mfloat_t MF_SQRT1ONPI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1540, .precision = 1088u, .immortal = true, .mantissa = &MF_SQRT1ONPI_mint };
const mfloat_t MF_2_SQRTPI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_2_SQRTPI_mint };
const mfloat_t MF_NEG_TWO_OVER_SQRT_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_NEG_TWO_OVER_SQRT_PI_mint };
const mfloat_t MF_INV_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1094, .precision = 1088u, .immortal = true, .mantissa = &MF_INV_SQRT_2PI_mint };
const mfloat_t MF_LOG_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1088, .precision = 1088u, .immortal = true, .mantissa = &MF_LOG_SQRT_2PI_mint };
const mfloat_t MF_LN_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_LN_2PI_mint };
const mfloat_t MF_PI_SQUARED_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2162, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_SQUARED_mint };
const mfloat_t MF_2PI_CUBED_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2224, .precision = 1152u, .immortal = true, .mantissa = &MF_2PI_CUBED_mint };
const mfloat_t MF_TENTH_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -263, .precision = 256u, .immortal = true, .mantissa = &mfloat_tenth_mint };

/* Exported canonical handles. */
const mfloat_t * const MF_ZERO = &MF_ZERO_VALUE;
const mfloat_t * const MF_ONE = &MF_ONE_VALUE;
const mfloat_t * const MF_HALF = &MF_HALF_VALUE;
const mfloat_t * const MF_TENTH = &MF_TENTH_VALUE;
const mfloat_t * const MF_TEN = &MF_TEN_VALUE;
const mfloat_t * const MF_PI = &MF_PI_VALUE;
const mfloat_t * const MF_2PI = &MF_2PI_VALUE;
const mfloat_t * const MF_PI_2 = &MF_PI_2_VALUE;
const mfloat_t * const MF_PI_4 = &MF_PI_4_VALUE;
const mfloat_t * const MF_3PI_4 = &MF_3PI_4_VALUE;
const mfloat_t * const MF_PI_6 = &MF_PI_6_VALUE;
const mfloat_t * const MF_PI_3 = &MF_PI_3_VALUE;
const mfloat_t * const MF_2_PI = &MF_2_PI_VALUE;
const mfloat_t * const MF_E = &MF_E_VALUE;
const mfloat_t * const MF_INV_E = &MF_INV_E_VALUE;
const mfloat_t * const MF_LN2 = &MF_LN2_VALUE;
const mfloat_t * const MF_INVLN2 = &MF_INVLN2_VALUE;
const mfloat_t * const MF_EULER_MASCHERONI = &MF_EULER_MASCHERONI_VALUE;
const mfloat_t * const MF_PHI = &MF_PHI_VALUE;
const mfloat_t * const MF_SQRT_HALF = &MF_SQRT_HALF_VALUE;
const mfloat_t * const MF_SQRT2 = &MF_SQRT2_VALUE;
const mfloat_t * const MF_SQRT3 = &MF_SQRT3_VALUE;
const mfloat_t * const MF_SQRT2_OVER_TWO = &MF_SQRT2_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT3_OVER_TWO = &MF_SQRT3_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT_2PI = &MF_SQRT_2PI_VALUE;
const mfloat_t * const MF_SQRT_PI = &MF_SQRT_PI_VALUE;
const mfloat_t * const MF_SQRT_PI_OVER_TWO = &MF_SQRT_PI_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT1ONPI = &MF_SQRT1ONPI_VALUE;
const mfloat_t * const MF_2_SQRTPI = &MF_2_SQRTPI_VALUE;
const mfloat_t * const MF_NEG_TWO_OVER_SQRT_PI = &MF_NEG_TWO_OVER_SQRT_PI_VALUE;
const mfloat_t * const MF_INV_SQRT_2PI = &MF_INV_SQRT_2PI_VALUE;
const mfloat_t * const MF_LOG_SQRT_2PI = &MF_LOG_SQRT_2PI_VALUE;
const mfloat_t * const MF_LN_2PI = &MF_LN_2PI_VALUE;
const mfloat_t * const MF_PI_SQUARED = &MF_PI_SQUARED_VALUE;
const mfloat_t * const MF_2PI_CUBED = &MF_2PI_CUBED_VALUE;
const mfloat_t * const MF_NAN = &MF_NAN_VALUE;
const mfloat_t * const MF_INF = &MF_INF_VALUE;
const mfloat_t * const MF_NINF = &MF_NINF_VALUE;
