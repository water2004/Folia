#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FoliaNativeMathRenderer FoliaNativeMathRenderer;
typedef struct FoliaNativeMathCancellation FoliaNativeMathCancellation;
typedef struct FoliaNativeMathResult FoliaNativeMathResult;

typedef enum FoliaNativeMathStatus {
    FOLIA_NATIVE_MATH_OK = 0,
    FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR = 1,
    FOLIA_NATIVE_MATH_CANCELLED = 2,
    FOLIA_NATIVE_MATH_SESSION_UNUSABLE = 3,
    FOLIA_NATIVE_MATH_OUT_OF_MEMORY = 4
} FoliaNativeMathStatus;

// Construct, use, and destroy a renderer on its creating worker thread.
// RenderSvg actively rejects a wrong-thread call; Destroy has a caller
// precondition because its void signature cannot report a rejected destroy.
FoliaNativeMathStatus FoliaNativeMathRendererCreate(
    FoliaNativeMathRenderer** renderer);
void FoliaNativeMathRendererDestroy(FoliaNativeMathRenderer* renderer);

// Cancellation is an opaque bridge-owned handle; only RequestStop may be
// called from another thread. It must remain alive until RenderSvg returns and
// any registered stop callback has been destroyed.
FoliaNativeMathStatus FoliaNativeMathCancellationCreate(
    FoliaNativeMathCancellation** cancellation);
void FoliaNativeMathCancellationRequestStop(
    FoliaNativeMathCancellation* cancellation);
void FoliaNativeMathCancellationDestroy(
    FoliaNativeMathCancellation* cancellation);

// tex contains UTF-16 code units (not UTF-8 bytes). No C++/STL type crosses
// this boundary. On a non-OOM render error, a result may be returned with the
// corresponding error/status. On OOM, invalid arguments, or a wrong-thread
// call, *result remains null.
FoliaNativeMathStatus FoliaNativeMathRenderSvg(
    FoliaNativeMathRenderer* renderer,
    uint16_t const* tex,
    size_t tex_units,
    int display,
    double em,
    double container_width,
    FoliaNativeMathCancellation* cancellation,
    unsigned int timeout_ms,
    FoliaNativeMathResult** result);

typedef struct FoliaNativeMathInput {
    uint16_t const* tex;
    size_t tex_units;
    int display;
    double em;
    double container_width;
    FoliaNativeMathCancellation* cancellation;
    unsigned int timeout_ms;
} FoliaNativeMathInput;

typedef struct FoliaNativeMathOutput {
    FoliaNativeMathStatus status;
    FoliaNativeMathResult* result;
} FoliaNativeMathOutput;

// One font request for the batch. Item errors remain isolated; each returned
// result is caller-owned. Timeout starts when that item begins rendering.
FoliaNativeMathStatus FoliaNativeMathRenderSvgBatch(
    FoliaNativeMathRenderer* renderer,
    FoliaNativeMathInput const* inputs,
    size_t count,
    FoliaNativeMathOutput* outputs);

char const* FoliaNativeMathResultSvg(
    FoliaNativeMathResult const* result,
    size_t* size);
char const* FoliaNativeMathResultError(
    FoliaNativeMathResult const* result,
    size_t* size);
void FoliaNativeMathResultDestroy(FoliaNativeMathResult* result);

#ifdef __cplusplus
}
#endif
