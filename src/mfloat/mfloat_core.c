#include "mfloat_internal.h"
#include "mfloat_coeff_tables.h"
#include "internal/qfloat_internal.h"
#include "internal/mint_internal.h"
#include "mrational.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static uint64_t mfloat_one_storage[] = { 1u };
static uint64_t mfloat_ten_storage[] = { 10u };
static const double MFLOAT_LOG10_2 = 0.3010299956639812;
static const double MFLOAT_LOG2_10 = 3.3219280948873626;

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
    0xcc6d241b0e2ae9cdu, 0xe1003e5c50b1df82u, 0x24943328f6722d9eu, 0xd74f9208be258ff3u,
    0xf71c35fdad44cfd2u, 0x85ffae5b7a035bf6u, 0x7a262174d31bf6b5u, 0xf242dabb312f3f63u,
    0xa7f09ab6b6a8e122u, 0x98158536f92f8a1bu, 0xf7ca8cd9e69d218du, 0x28a5043cc71a026eu,
    0x0105df531d89cd91u, 0x948127044533e63au, 0x62633145c06e0e68u, 0x6487ed5110b4611au
};
static struct _mint_t MF_PI_2_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_PI_2_storage };

static uint64_t MF_PI_4_storage[] = {
    0xcc6d241b0e2ae9cdu, 0xe1003e5c50b1df82u, 0x24943328f6722d9eu, 0xd74f9208be258ff3u,
    0xf71c35fdad44cfd2u, 0x85ffae5b7a035bf6u, 0x7a262174d31bf6b5u, 0xf242dabb312f3f63u,
    0xa7f09ab6b6a8e122u, 0x98158536f92f8a1bu, 0xf7ca8cd9e69d218du, 0x28a5043cc71a026eu,
    0x0105df531d89cd91u, 0x948127044533e63au, 0x62633145c06e0e68u, 0x6487ed5110b4611au
};
static struct _mint_t MF_PI_4_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_PI_4_storage };

static uint64_t MF_3PI_4_storage[] = {
    0x0ca8ed8a255017adu, 0x946017629e42b3d1u, 0x2db7932f5c6ad11bu, 0x10bdd6c3474e15fbu,
    0x9caa943f20f9cdefu, 0x123fe1624dc1427cu, 0x4dce4c8bcf2a7c84u, 0x1ad912063271b7c5u,
    0x5efa3a04847f546du, 0x190811f49d71d3cau, 0x9cebf4d1b67aec95u, 0x6f3de196caa9c0e9u,
    0xc06233bf2b13ad16u, 0x37b06ea199f37655u, 0xe4e5327a28294567u, 0x25b2f8fe6643a469u
};
static struct _mint_t MF_3PI_4_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_3PI_4_storage };

static uint64_t MF_PI_6_storage[] = {
    0x4424615e5a0e4defu, 0xa0556a1ec5909fd6u, 0x0c316662fcd0b9dfu, 0x9d1a8602ea0c8551u,
    0xa7b411ff39c19a9bu, 0xd7553a1e7e011ea7u, 0x28b7607c465ea791u, 0xa61648e910651521u,
    0xe2a588e792384b0bu, 0x32b1d712530fd8b3u, 0xa7ee2ef34cdf0b2fu, 0x62e1ac14425e00cfu,
    0x55ac9fc65f2def30u, 0xdc2b0d016c66a213u, 0xcb7665c1eacf5a22u, 0x2182a4705ae6cb08u
};
static struct _mint_t MF_PI_6_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_PI_6_storage };

static uint64_t MF_PI_3_storage[] = {
    0x4424615e5a0e4defu, 0xa0556a1ec5909fd6u, 0x0c316662fcd0b9dfu, 0x9d1a8602ea0c8551u,
    0xa7b411ff39c19a9bu, 0xd7553a1e7e011ea7u, 0x28b7607c465ea791u, 0xa61648e910651521u,
    0xe2a588e792384b0bu, 0x32b1d712530fd8b3u, 0xa7ee2ef34cdf0b2fu, 0x62e1ac14425e00cfu,
    0x55ac9fc65f2def30u, 0xdc2b0d016c66a213u, 0xcb7665c1eacf5a22u, 0x2182a4705ae6cb08u
};
static struct _mint_t MF_PI_3_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_PI_3_storage };

static uint64_t MF_2_PI_storage[] = {
    0xd49eeb1faf97c5edu, 0x3d18fd9a797fa8b5u, 0xb4d9fb3c9f2c26ddu, 0xbcbc462d6829b47du,
    0x7fe25fff7816603fu, 0x72117e2ef7e4a0ecu, 0xe64758e60d4ce7d2u, 0xa671c09ad17df904u,
    0xa208d7d4baed1213u, 0xf877ac72c4a69cfbu, 0x1924bba827464873u, 0xdc91b8e909374b80u,
    0xf9458eaf7aef1586u, 0x6d8a5664f10e4107u, 0xf09d5f47d4d37703u, 0x8be60db9391054a7u,
    0x0000000000000002u
};
static struct _mint_t MF_2_PI_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_2_PI_storage };

static uint64_t MF_INV_E_storage[] = {
    0xef95734c6de00b19u, 0xb76462347255a968u, 0x75706e364313bdf5u, 0xea06fa8dd93b53dfu,
    0x4ab79396344490d9u, 0x21f0fcff911189b3u, 0x497f6f56033c5370u, 0x3b0acef6eaa49c36u,
    0x7c0f189db72a9630u, 0xba955425a370e48au, 0x0916d9a764349e96u, 0x59002fd1cad0856eu,
    0x092e742941f6bb6cu, 0xa6016aad5b1dccccu, 0xb7b1e0a4153e4376u, 0x8b56362cef37c6aeu,
    0x0000000000000017u
};
static struct _mint_t MF_INV_E_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_INV_E_storage };

static uint64_t MF_LN2_storage[] = {
    0x6d16cbe2879feae3u, 0x32afd0c3979071d1u, 0x7aefd35e9c181924u, 0xb96743d8ceb2a465u,
    0x92b7d0763b2bfba5u, 0x5cf54de1d89b301du, 0x8d65ed0898be1c3fu, 0x9f4b650b11257462u,
    0x13ab9d9488b4dc12u, 0x7697571ae09c10a2u, 0x2acaa97da57d0d88u, 0xf3dc3b1036f5d64cu,
    0xc5068badc5d57d15u, 0xa079a193394c5b16u, 0xe4f1d9cc01f97b57u, 0x58b90bfbe8e7bcd5u
};
static struct _mint_t MF_LN2_mint = { .sign = 1, .length = 16u, .capacity = 16u, .storage = MF_LN2_storage };

static uint64_t MF_INVLN2_storage[] = {
    0x12f08fbae30a1733u, 0xe7e20358cd5db8f6u, 0x78ccf084679c940cu, 0x99a94836f5b49672u,
    0xd1cf457ab63253c1u, 0xb5ebbbf3a8285468u, 0x21b43d579d5a2060u, 0xfe294932617d9d5bu,
    0x4bfaf0353df39b32u, 0xa90b9e60c4a909fcu, 0x4d92f75c16be0b3eu, 0xe1c43f755176cd62u,
    0xb25166cd1a13247du, 0xb577aa8dd695a588u, 0xe87fed0691d3e88eu, 0x8aa3b295c17f0bbbu,
    0x000000000000000bu
};
static struct _mint_t MF_INVLN2_mint = { .sign = 1, .length = 17u, .capacity = 17u, .storage = MF_INVLN2_storage };

static uint64_t MF_SQRT_HALF_storage[] = {
    0xb45eb2160cce6455u, 0x75bd82ea24eea133u, 0x65f626cdd52afa7cu, 0xd413cccfe7799211u,
    0x0000000000000002u
};
static struct _mint_t MF_SQRT_HALF_mint = { .sign = 1, .length = 5u, .capacity = 5u, .storage = MF_SQRT_HALF_storage };

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
    0xaaad6504588bd045u, 0x78856ecb2fd50f05u, 0x6f3c9b0470c9ee54u, 0xca3c571e6a2ec4e0u,
    0x5e87bba49bb3be73u, 0x572f180a8ffb7ad8u, 0x564bc973ad790307u, 0xc48222c2d19dcde4u,
    0xb9ab54787c6dde93u, 0x028eada177c7a52du, 0x44e3e898a04e7bfdu, 0x61311860cdf72152u,
    0xe80c53d3e1ce33f6u, 0xb515af3540e8d3afu, 0x2d674aa1964893fau, 0x64b111920dee5c86u,
    0xe15c35d764ffd2ccu, 0x183f879bea150be0u, 0x423eab05c40c7dccu, 0xa83419bdb2b4c393u,
    0x6e17a096ef75ca45u, 0x8cf9f5320b0005a8u, 0x7ca41fae722cee2fu, 0x40d931ff62705965u,
    0x0000000000000001u
};
static struct _mint_t MF_SQRT_PI_OVER_TWO_mint = { .sign = 1, .length = 25u, .capacity = 25u, .storage = MF_SQRT_PI_OVER_TWO_storage };

static uint64_t MF_SQRT1ONPI_storage[] = {
    0x86234cf7fc455bd5u, 0x2fbe9b22ade9786cu, 0x40e4edbb459d57dfu, 0xc9ce65f55a448e08u,
    0x56d90f3f1d7561cbu, 0xd3be1bc4c722cd81u, 0xed4bef68070a9e08u, 0xc24b8e071c879ccdu,
    0x8d3e91adcff6c039u, 0x0754b409e94d32d1u, 0xc2c88bbba81b1c75u, 0xb9feb2436f2f272au,
    0x27a3282dada7316eu, 0x522f2f93e16b2a3du, 0xc22f47f7b7fb57c9u, 0x2561dcc244dc65e9u,
    0x4f76f877ffec2515u, 0xd1f4eee48e1ca787u, 0x0c036096cc79aebbu, 0x0759cf859270f114u,
    0x9a15830cce620b0cu, 0x409a0ebac3e75173u, 0x1d48a7f6bfec3441u, 0x06eba8214db688d7u,
    0x0000000000000009u
};
static struct _mint_t MF_SQRT1ONPI_mint = { .sign = 1, .length = 25u, .capacity = 25u, .storage = MF_SQRT1ONPI_storage };

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

const mfloat_t MF_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .immortal = true, .mantissa = &mfloat_pi_mint };
const mfloat_t MF_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1080, .precision = 1088u, .immortal = true, .mantissa = &MF_2PI_mint };
const mfloat_t MF_PI_2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1022, .precision = 1024u, .immortal = true, .mantissa = &MF_PI_2_mint };
const mfloat_t MF_PI_4_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1023, .precision = 1024u, .immortal = true, .mantissa = &MF_PI_4_mint };
const mfloat_t MF_3PI_4_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1020, .precision = 1024u, .immortal = true, .mantissa = &MF_3PI_4_mint };
const mfloat_t MF_PI_6_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1022, .precision = 1024u, .immortal = true, .mantissa = &MF_PI_6_mint };
const mfloat_t MF_PI_3_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1021, .precision = 1024u, .immortal = true, .mantissa = &MF_PI_3_mint };
const mfloat_t MF_2_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1026, .precision = 1024u, .immortal = true, .mantissa = &MF_2_PI_mint };
const mfloat_t MF_E_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .immortal = true, .mantissa = &mfloat_e_mint };
const mfloat_t MF_INV_E_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1030, .precision = 1024u, .immortal = true, .mantissa = &MF_INV_E_mint };
const mfloat_t MF_LN2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1023, .precision = 1024u, .immortal = true, .mantissa = &MF_LN2_mint };
const mfloat_t MF_INVLN2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1027, .precision = 1024u, .immortal = true, .mantissa = &MF_INVLN2_mint };
const mfloat_t MF_EULER_MASCHERONI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2158, .precision = 1024u, .immortal = true, .mantissa = &mfloat_gamma_mint };
const mfloat_t MF_SQRT_HALF_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -258, .precision = 256u, .immortal = true, .mantissa = &MF_SQRT_HALF_mint };
const mfloat_t MF_SQRT_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2154, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sqrt_pi_mint };
const mfloat_t MF_SQRT2_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sqrt2_mint };
const mfloat_t MF_SQRT3_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2155, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sqrt3_mint };
const mfloat_t MF_SQRT2_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2156, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sqrt2_mint };
const mfloat_t MF_SQRT3_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2156, .precision = 1024u, .immortal = true, .mantissa = &mfloat_sqrt3_mint };
const mfloat_t MF_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1630, .precision = 1088u, .immortal = true, .mantissa = &MF_SQRT_2PI_mint };
const mfloat_t MF_SQRT_PI_OVER_TWO_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1536, .precision = 1024u, .immortal = true, .mantissa = &MF_SQRT_PI_OVER_TWO_mint };
const mfloat_t MF_SQRT1ONPI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1540, .precision = 1024u, .immortal = true, .mantissa = &MF_SQRT1ONPI_mint };
const mfloat_t MF_2_SQRTPI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_2_SQRTPI_mint };
const mfloat_t MF_NEG_TWO_OVER_SQRT_PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = -1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_NEG_TWO_OVER_SQRT_PI_mint };
const mfloat_t MF_INV_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1094, .precision = 1088u, .immortal = true, .mantissa = &MF_INV_SQRT_2PI_mint };
const mfloat_t MF_LOG_SQRT_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1088, .precision = 1088u, .immortal = true, .mantissa = &MF_LOG_SQRT_2PI_mint };
const mfloat_t MF_LN_2PI_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -1087, .precision = 1088u, .immortal = true, .mantissa = &MF_LN_2PI_mint };
const mfloat_t MF_PI_SQUARED_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2162, .precision = 1088u, .immortal = true, .mantissa = &MF_PI_SQUARED_mint };
const mfloat_t MF_2PI_CUBED_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -2224, .precision = 1152u, .immortal = true, .mantissa = &MF_2PI_CUBED_mint };
const mfloat_t MF_TENTH_VALUE = { .kind = MFLOAT_KIND_FINITE, .sign = 1, .exponent2 = -263, .precision = 256u, .immortal = true, .mantissa = &mfloat_tenth_mint };

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

static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

bool mfloat_is_immortal(const mfloat_t *mfloat)
{
    return mfloat && mfloat->immortal;
}

bool mfloat_is_finite(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_FINITE;
}

bool mfloat_is_nan(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_NAN;
}

bool mfloat_is_inf(const mfloat_t *mfloat)
{
    return mfloat &&
        (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF);
}

bool mf_is_finite(const mfloat_t *mfloat)
{
    return mfloat_is_finite(mfloat);
}

bool mf_is_nan(const mfloat_t *mfloat)
{
    return mfloat_is_nan(mfloat);
}

bool mf_is_inf(const mfloat_t *mfloat)
{
    return mfloat_is_inf(mfloat);
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

int mfloat_round_to_precision_internal(mfloat_t *mfloat, size_t precision)
{
    size_t bitlen, excess;
    mint_t *hi = NULL, *trunc = NULL, *low = NULL, *half = NULL;
    int rc = -1;

    if (!mfloat || !mfloat->mantissa || !mfloat_is_finite(mfloat) || precision == 0)
        return -1;

    bitlen = mi_bit_length(mfloat->mantissa);
    if (bitlen <= precision) {
        mfloat->precision = precision;
        return 0;
    }

    excess = bitlen - precision;
    hi = mi_clone(mfloat->mantissa);
    low = mi_clone(mfloat->mantissa);
    if (!hi || !low)
        goto cleanup;

    if (mi_shr(hi, (long)excess) != 0)
        goto cleanup;

    trunc = mi_clone(hi);
    if (!trunc)
        goto cleanup;
    if (mi_shl(trunc, (long)excess) != 0)
        goto cleanup;
    if (mi_sub(low, trunc) != 0)
        goto cleanup;

    half = mi_create_2pow((uint64_t)(excess - 1u));
    if (!half)
        goto cleanup;
    if (mi_cmp(low, half) >= 0) {
        if (mi_inc(hi) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, hi) != 0)
        goto cleanup;
    mfloat->exponent2 += (long)excess;
    mfloat->precision = precision;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(hi);
    mi_free(trunc);
    mi_free(low);
    mi_free(half);
    return rc;
}

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat = calloc(1, sizeof(*mfloat));

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
    mfloat->immortal = false;
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
    return mfloat_alloc(precision_bits);
}

size_t mfloat_get_default_precision_internal(void)
{
    return mfloat_default_precision_bits;
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
    if (precision < src->precision &&
        mfloat_round_to_precision_internal(dst, precision) != 0) {
        mf_free(dst);
        return NULL;
    }
    dst->precision = precision;
    return dst;
}

int mfloat_set_from_immortal_internal(mfloat_t *dst, const mfloat_t *src, size_t precision)
{
    if (src && precision == src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc == 0)
            dst->precision = precision;
        return rc;
    }
    if (src && precision < src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc != 0)
            return rc;
        if (mfloat_round_to_precision_internal(dst, precision) != 0)
            return -1;
        dst->precision = precision;
        return 0;
    }
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

mfloat_t *mf_create_mrational(const mrational_t *value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_mrational(mfloat, value) != 0) {
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

static size_t mfloat_bits_to_decimal_digits(size_t precision_bits)
{
    size_t digits;

    if (precision_bits == 0u)
        return 0u;
    digits = (size_t)floor((double)precision_bits * MFLOAT_LOG10_2);
    return digits > 0u ? digits : 1u;
}

static size_t mfloat_decimal_digits_to_bits(size_t significant_digits)
{
    size_t bits;

    if (significant_digits == 0u)
        return 0u;
    bits = (size_t)ceil((double)significant_digits * MFLOAT_LOG2_10);
    return bits > 0u ? bits : 1u;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->precision : 0;
}

int mf_set_default_precision_digits(size_t significant_digits)
{
    return mf_set_default_precision(
        mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_default_precision_digits(void)
{
    return mfloat_bits_to_decimal_digits(mf_get_default_precision());
}

int mf_set_precision_digits(mfloat_t *mfloat, size_t significant_digits)
{
    return mf_set_precision(mfloat,
                            mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_precision_digits(const mfloat_t *mfloat)
{
    return mfloat_bits_to_decimal_digits(mf_get_precision(mfloat));
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
    if (!mfloat)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    return qf_to_mfloat_exact(mfloat, value);
}

int mf_set_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    if (!mfloat || !value || mfloat_is_immortal(mfloat))
        return -1;
    if (mf_set_long(mfloat, 0) != 0)
        return -1;
    return mf_add_mrational(mfloat, value);
}

int mf_add_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    size_t precision, work_prec;
    const mint_t *num = NULL;
    const mint_t *den = NULL;
    mint_t *lhs_num = NULL;
    mint_t *rhs_num = NULL;
    mint_t *common_den = NULL;
    mfloat_t *tmp = NULL;
    mfloat_t *den_mf = NULL;
    int rc = -1;

    if (!mfloat || !value)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    num = mr_numerator(value);
    den = mr_denominator(value);
    if (!num || !den || mi_is_zero(den))
        goto cleanup;

    precision = mfloat->precision;
    work_prec = precision + 32u;
    tmp = mf_clone(mfloat);
    den_mf = mf_new_prec(work_prec);
    if (!tmp || !den_mf)
        goto cleanup;
    if (mf_set_precision(tmp, work_prec) != 0)
        goto cleanup;

    lhs_num = mi_clone(tmp->mantissa);
    rhs_num = mi_clone(num);
    common_den = mi_clone(den);
    if (!lhs_num || !rhs_num || !common_den)
        goto cleanup;
    if (tmp->sign < 0 && mi_neg(lhs_num) != 0)
        goto cleanup;

    if (tmp->exponent2 >= 0) {
        if (mi_shl(lhs_num, tmp->exponent2) != 0 ||
            mi_mul(lhs_num, common_den) != 0)
            goto cleanup;
    } else {
        long shift = -tmp->exponent2;
        if (mi_mul(lhs_num, common_den) != 0 ||
            mi_shl(rhs_num, shift) != 0 ||
            mi_shl(common_den, shift) != 0)
            goto cleanup;
    }

    if (mi_add(lhs_num, rhs_num) != 0)
        goto cleanup;
    if (mfloat_set_from_signed_mint(tmp, lhs_num, 0) != 0 ||
        mfloat_set_from_signed_mint(den_mf, common_den, 0) != 0 ||
        mf_div(tmp, den_mf) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, tmp) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision_internal(mfloat, precision);

cleanup:
    mi_free(lhs_num);
    mi_free(rhs_num);
    mi_free(common_den);
    mf_free(tmp);
    mf_free(den_mf);
    return rc;
}

int mf_mul_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    size_t precision, work_prec;
    const mint_t *num = NULL;
    const mint_t *den = NULL;
    mint_t *num_mag = NULL;
    mint_t *den_mag = NULL;
    mfloat_t *tmp = NULL;
    mfloat_t *den_mf = NULL;
    int rc = -1;

    if (!mfloat || !value)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    num = mr_numerator(value);
    den = mr_denominator(value);
    if (!num || !den || mi_is_zero(den))
        goto cleanup;

    precision = mfloat->precision;
    work_prec = precision + 32u;
    tmp = mf_clone(mfloat);
    den_mf = mf_new_prec(work_prec);
    if (!tmp || !den_mf)
        goto cleanup;
    if (mf_set_precision(tmp, work_prec) != 0)
        goto cleanup;

    if (mi_is_zero(num)) {
        rc = mf_set_long(mfloat, 0);
        goto cleanup;
    }

    num_mag = mi_clone(num);
    den_mag = mi_clone(den);
    if (!num_mag || !den_mag)
        goto cleanup;
    if (mi_is_negative(num)) {
        if (mf_neg(tmp) != 0 || mi_abs(num_mag) != 0)
            goto cleanup;
    }
    if (mi_mul(tmp->mantissa, num_mag) != 0 || mfloat_normalise(tmp) != 0)
        goto cleanup;
    if (mfloat_set_from_signed_mint(den_mf, den_mag, 0) != 0 || mf_div(tmp, den_mf) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, tmp) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision_internal(mfloat, precision);

cleanup:
    mi_free(num_mag);
    mi_free(den_mag);
    mf_free(tmp);
    mf_free(den_mf);
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
