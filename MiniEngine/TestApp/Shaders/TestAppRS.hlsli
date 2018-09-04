
#define TestApp_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t0, numDescriptors = 6), visibility = SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL)"
