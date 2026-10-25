// Generated from GameBindingProfile.json by scripts/Generate-GameBindings.cjs; edit the JSON, then regenerate.
#pragma once
#include "GameBindingResolver.h"
namespace ewi::bindings::profile {
// Stable indices connect capture fields to discovered sites and actual call returns.
enum class Site : uint8_t { Normal, Enqueue, Actor, Shrink, Pool, Location, Group, Spread, SpreadCallback, Center, Scheduler, Tick, Queue, Thread };
enum class Call : uint8_t { Enqueue0, Enqueue1, Center, Pool, Normal, Scheduler, Tick, Actor, Shrink };
// Digest identifies the reviewed source data in diagnostics and build evidence.
inline constexpr const char* Sha256 = "E6FE96F2539FE29966B911F38FE5BE0320BA58FFF9E0958224DF386E8E4B8883";
// Audited address operands for normal; all other bytes remain significant.
inline constexpr std::array<MaskRange, 1> MasksNormal{{{36,4}}};
// Audited address operands for enqueue; all other bytes remain significant.
inline constexpr std::array<MaskRange, 61> MasksEnqueue{{{79,4},{99,4},{140,4},{232,4},{243,4},{275,4},{296,4},{310,4},{322,4},{334,4},{346,4},{366,4},{401,4},{454,4},{488,4},{512,4},{520,4},{538,4},{565,4},{617,4},{633,4},{678,4},{693,4},{773,4},{808,4},{842,4},{950,4},{1106,4},{1129,4},{1141,4},{1172,4},{1184,4},{1191,4},{1210,4},{1229,4},{1244,4},{1308,4},{1349,4},{1362,4},{1374,4},{1409,4},{1430,4},{1449,4},{1460,4},{1501,4},{1514,4},{1526,4},{1561,4},{1582,4},{1601,4},{1612,4},{1653,4},{1666,4},{1678,4},{1713,4},{1734,4},{1749,4},{1757,4},{1767,4},{1780,4},{1794,4}}};
// Audited address operands for actor; all other bytes remain significant.
inline constexpr std::array<MaskRange, 89> MasksActor{{{45,4},{66,4},{80,4},{95,4},{102,4},{109,4},{127,4},{134,4},{139,4},{161,4},{168,4},{190,4},{197,4},{202,4},{213,4},{220,4},{242,4},{249,4},{254,4},{259,4},{268,4},{286,4},{303,4},{314,4},{339,4},{369,4},{413,4},{425,4},{459,4},{471,4},{486,4},{500,4},{514,4},{545,4},{574,4},{581,4},{605,4},{612,4},{617,4},{632,4},{639,4},{646,4},{659,4},{673,4},{678,4},{694,4},{763,4},{770,4},{783,4},{801,4},{808,4},{826,4},{840,4},{850,4},{877,4},{929,4},{959,4},{992,4},{1023,4},{1052,4},{1072,4},{1094,4},{1102,4},{1116,4},{1123,4},{1147,4},{1154,4},{1159,4},{1186,4},{1214,4},{1235,4},{1247,4},{1270,4},{1275,4},{1280,4},{1294,4},{1299,4},{1304,4},{1312,4},{1318,4},{1325,4},{1344,4},{1372,4},{1393,4},{1405,4},{1425,4},{1440,4},{1523,4},{1539,4}}};
// Audited address operands for shrink; all other bytes remain significant.
inline constexpr std::array<MaskRange, 2> MasksShrink{{{73,4},{119,4}}};
// Audited address operands for pool; all other bytes remain significant.
inline constexpr std::array<MaskRange, 30> MasksPool{{{18,4},{54,4},{72,4},{98,4},{105,4},{126,4},{139,4},{153,4},{165,4},{195,4},{217,4},{252,4},{263,4},{274,4},{285,4},{373,4},{396,4},{540,4},{556,4},{608,4},{616,4},{627,4},{635,4},{646,4},{654,4},{739,4},{777,4},{808,4},{821,4},{851,4}}};
// Audited address operands for location; all other bytes remain significant.
inline constexpr std::array<MaskRange, 49> MasksLocation{{{23,4},{50,4},{69,4},{83,4},{97,4},{115,4},{122,4},{156,4},{230,4},{238,4},{258,4},{288,4},{350,4},{364,4},{383,4},{425,4},{442,4},{450,4},{472,4},{496,4},{504,4},{515,4},{538,4},{553,4},{561,4},{588,4},{600,4},{610,4},{648,4},{662,4},{677,4},{703,4},{721,4},{778,4},{807,4},{869,4},{910,4},{981,4},{1002,4},{1033,4},{1045,4},{1058,4},{1080,4},{1094,4},{1113,4},{1124,4},{1138,4},{1151,4},{1165,4}}};
// Audited address operands for group; all other bytes remain significant.
inline constexpr std::array<MaskRange, 26> MasksGroup{{{24,4},{65,4},{77,4},{87,4},{125,4},{135,4},{150,4},{157,4},{171,4},{195,4},{262,4},{273,4},{284,4},{293,4},{321,4},{329,4},{340,4},{348,4},{359,4},{367,4},{383,4},{445,4},{468,4},{538,4},{603,4},{641,4}}};
// Audited address operands for spread; all other bytes remain significant.
inline constexpr std::array<MaskRange, 31> MasksSpread{{{17,4},{25,4},{65,4},{77,4},{121,4},{131,4},{146,4},{159,4},{196,4},{208,4},{221,4},{246,4},{265,4},{277,4},{299,4},{323,4},{390,4},{401,4},{412,4},{421,4},{455,4},{463,4},{474,4},{482,4},{493,4},{501,4},{517,4},{582,4},{621,4},{657,4},{672,4}}};
// Audited address operands for spreadCallback; all other bytes remain significant.
inline constexpr std::array<MaskRange, 28> MasksSpreadCallback{{{16,4},{63,4},{75,4},{85,4},{123,4},{133,4},{148,4},{155,4},{169,4},{193,4},{257,4},{268,4},{279,4},{288,4},{316,4},{324,4},{335,4},{343,4},{354,4},{362,4},{378,4},{440,4},{455,4},{500,4},{537,4},{664,4},{714,4},{735,4}}};
// Audited address operands for center; all other bytes remain significant.
inline constexpr std::array<MaskRange, 46> MasksCenter{{{38,4},{115,4},{168,4},{208,4},{231,4},{250,4},{319,4},{342,4},{417,4},{439,4},{603,4},{626,4},{663,4},{778,4},{820,4},{833,4},{1103,4},{1117,4},{1282,4},{1326,4},{1383,4},{1414,4},{1426,4},{1436,4},{1484,4},{1499,4},{1514,4},{1601,4},{1657,4},{2027,4},{2082,4},{2112,4},{2156,4},{2180,4},{2228,4},{2242,4},{2256,4},{2271,4},{2374,4},{2406,4},{2424,4},{2459,4},{2484,4},{2493,4},{2594,4},{2754,4}}};
// Audited address operands for scheduler; all other bytes remain significant.
inline constexpr std::array<MaskRange, 45> MasksScheduler{{{76,4},{88,4},{135,4},{157,4},{195,4},{226,4},{262,4},{300,4},{308,4},{872,4},{1131,4},{1174,4},{1201,4},{1245,4},{1262,4},{1318,4},{1340,4},{1350,4},{1368,4},{1377,4},{1441,4},{1471,4},{1495,4},{1505,4},{1528,4},{1553,4},{1563,4},{1577,4},{1643,4},{1651,4},{1709,4},{1719,4},{1733,4},{1744,4},{1753,4},{1767,4},{1783,4},{1802,4},{1830,4},{1877,4},{1887,4},{1901,4},{1936,4},{1983,4},{1998,4}}};
// Audited address operands for tick; all other bytes remain significant.
inline constexpr std::array<MaskRange, 32> MasksTick{{{99,4},{119,4},{160,4},{168,4},{215,4},{232,4},{434,4},{456,4},{515,4},{591,4},{602,4},{640,4},{657,4},{669,4},{733,4},{745,4},{758,4},{780,4},{790,4},{805,4},{816,4},{862,4},{868,4},{888,4},{901,4},{921,4},{932,4},{944,4},{965,4},{978,4},{991,4},{1011,4}}};
// Audited address operands for queue; all other bytes remain significant.
inline constexpr std::array<MaskRange, 67> MasksQueue{{{101,4},{112,4},{144,4},{165,4},{179,4},{190,4},{204,4},{216,4},{229,4},{264,4},{327,4},{437,4},{455,4},{487,4},{498,4},{524,4},{535,4},{547,4},{572,4},{584,4},{598,4},{612,4},{625,4},{709,4},{726,4},{741,4},{757,4},{796,4},{802,4},{821,4},{881,4},{916,4},{969,4},{1033,4},{1081,4},{1108,4},{1126,4},{1140,4},{1163,4},{1189,4},{1206,4},{1219,4},{1279,4},{1301,4},{1318,4},{1374,4},{1396,4},{1437,4},{1449,4},{1490,4},{1532,4},{1540,4},{1551,4},{1695,4},{1716,4},{1742,4},{1759,4},{1778,4},{1793,4},{1829,4},{1885,4},{1938,4},{1975,4},{2032,4},{2051,4},{2058,4},{2067,4}}};
// Audited address operands for thread; all other bytes remain significant.
inline constexpr std::array<MaskRange, 3> MasksThread{{{2,4},{12,4},{18,4}}};
// Full-span hashes and short search anchors jointly require unique instruction shapes.
inline constexpr std::array<Pattern, 14> Patterns{{
    {"normal", 45, 0x4f5d451e1415862bull, "4883ec4833c0488944243048894424380fb64424708844242844884c24204c8d4c2430e8????????4883c448c3", MasksNormal.data(), MasksNormal.size(), true},
    {"enqueue", 1824, 0x6ababc36aca8751cull, "48895c24184c894c242048895424105556574154415541564157488dac2450ffffff4881ecb001000080b9e8000000004d8be94d8be0488bf2488bf90f84c406", MasksEnqueue.data(), MasksEnqueue.size(), true},
    {"actor", 1602, 0xdc7bb60d83ac105aull, "488bc4555356574154415541564157488da8b8feffff4881ec080200000f2970a80f297898440f294088488b05????????4833c448894550488bd948894da048", MasksActor.data(), MasksActor.size(), true},
    {"shrink", 144, 0x14cbc4fa97c0d3c0ull, "40534883ec20448b410c488bd948635108458bc8442bca4963c148c1e007483d00400000730b8d0c52438d04003bc87d4d4183f9407f0485d2754385d27e1948", MasksShrink.data(), MasksShrink.size(), true},
    {"pool", 880, 0x54591009863ed856ull, "4c894424185355574881eca00000000fb605????????498be90f297c2470498bf80f28f9488bd93c0572420fb68424e80000004c8d05????????894424300f57", MasksPool.data(), MasksPool.size(), true},
    {"location", 1184, 0x1ac7c25f2ec7d678ull, "4055565741554156488d6c24f04881ec10010000488b05????????4833c4488945b0488b4560488bfa488bd1418bf0488b0d????????41b8020000004d8be948", MasksLocation.data(), MasksLocation.size(), true},
    {"group", 688, 0x6d2e923aafe42e41ull, "4c8bdc49895b1849896b20565741564881eca0000000803d????????05498be9410f297bc8488bf20f28fa488bd9726a488b4218498d4b98498d53a849894398", MasksGroup.data(), MasksGroup.size(), true},
    {"spread", 704, 0xe1bf7da07c7ca4b1ull, "4c8bdc53565741564881ec98000000803d????????05488d1d????????0f297c24704d8bf10f28fa488bf2488bf97266488b4218498d4b20498d539849894320", MasksSpread.data(), MasksSpread.size(), true},
    {"spreadCallback", 768, 0x018bcee96b46b041ull, "405355565741574881eca0000000803d????????05498bf10f29bc2480000000488bea0f28fa488bd9726d488b4218488d4c2450488d5424604889442450e8??", MasksSpreadCallback.data(), MasksSpreadCallback.size(), true},
    {"center", 2864, 0xb0f2b6653c617ef8ull, "4055535657415441554156488dac2420ffffff4881ece0010000440f298424a0010000488b05????????4833c448898590000000488bb118010000440f28c34c", MasksCenter.data(), MasksCenter.size(), true},
    {"scheduler", 2009, 0xbf8991919c258fe3ull, "488bc4554154488d68c84881ec28010000488970104533e44889781833f64c8968e84c8be94c8970e04533f6440f299878ffffff440f28d9488b81a80000004c", MasksScheduler.data(), MasksScheduler.size(), true},
    {"tick", 1041, 0xfae4593ddacdf12cull, "4889ac24980000004889b424a00000004c896424684c897c246085ff0f8ee000000083ef010f88cc0300004863c7488db3f800000041bc010000004c8d34c500", MasksTick.data(), MasksTick.size(), true},
    {"queue", 2126, 0xb95806cdc70ff46dull, "0f29bc24400200000f57ff488945c00f1f400066660f1f8400000000004c8be849c1e5074d032ef3410f104574f30f5cc6f3410f11457484c90f85f00000000f", MasksQueue.data(), MasksQueue.size(), true},
    {"thread", 22, 0xb6c989e2b7cb0fe7ull, "ff15????????498bcf448835????????8905????????", MasksThread.data(), MasksThread.size(), false},
}};
// Call edges preserve exact source provenance after relocation.
inline constexpr std::array<Edge, 9> Edges{{
    {"enqueue0", 9, 1661, 1, 0},
    {"enqueue1", 9, 2086, 1, 0},
    {"center", 9, 2726, 9, 0},
    {"pool", 4, 781, 9, 2304},
    {"normal", 0, 40, 4, 0},
    {"scheduler", 10, 1987, 0, 0},
    {"tick", 11, 936, 10, 0},
    {"actor", 12, 1305, 2, 0},
    {"shrink", 12, 2055, 3, 0},
}};
// Independently audited queue operands remain mandatory ABI checks.
inline constexpr std::array<Window, 6> Windows{{
    {"worldPrivate", 1, 66, "488b81a8000000"},
    {"queueStride", 12, 32, "49c1e507"},
    {"descriptor", 12, 86, "498b4d28"},
    {"transform", 12, 1283, "4d8d4540"},
    {"translation", 12, 813, "410f105550"},
    {"managerQueue", 12, 1947, "4c8db7c0010000"},
}};
// One profile is shared by production and the standalone native inspector.
inline constexpr Profile Data{Patterns.data(), Patterns.size(), Edges.data(), Edges.size(), Windows.data(), Windows.size(), 13};
} // namespace ewi::bindings::profile
