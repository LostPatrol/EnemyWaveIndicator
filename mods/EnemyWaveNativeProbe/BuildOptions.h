// Keep optional diagnostics and the archived prediction experiment out of normal builds.
#pragma once

// Detailed timing and heartbeat logs are enabled only by the developer build switch.
#ifndef EWI_DIAGNOSTICS
#define EWI_DIAGNOSTICS 0
#endif

// Prediction changes gameplay state and remains an explicit experimental build option.
#ifndef EWI_NORMAL_PREDICTION
#define EWI_NORMAL_PREDICTION 0
#endif
