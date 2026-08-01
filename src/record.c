/* KEY4 两次按键之间的左右编码器增量记录。 */

#include <limits.h>

#include "encoder.h"
#include "record.h"

static int32_t gStartLeft;
static int32_t gStartRight;
static int32_t gRecordLeft;
static int32_t gRecordRight;
static bool gRecording;

static int32_t Record_AbsoluteDelta(int32_t current, int32_t start)
{
    int64_t delta = (int64_t) current - (int64_t) start;

    if (delta < 0) {
        delta = -delta;
    }
    if (delta > INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t) delta;
}

void Record_Init(void)
{
    gStartLeft = 0;
    gStartRight = 0;
    gRecordLeft = 0;
    gRecordRight = 0;
    gRecording = false;
}

bool Record_Start(void)
{
    if (gRecording) {
        return false;
    }
    gStartLeft = Encoder_GetLeftCount();
    gStartRight = Encoder_GetRightCount();
    gRecording = true;
    return true;
}

bool Record_Stop(void)
{
    if (!gRecording) {
        return false;
    }
    gRecordLeft = Record_AbsoluteDelta(
        Encoder_GetLeftCount(), gStartLeft);
    gRecordRight = Record_AbsoluteDelta(
        Encoder_GetRightCount(), gStartRight);
    gRecording = false;
    return true;
}

bool Record_IsRecording(void)
{
    return gRecording;
}

int32_t Record_GetLeft(void)
{
    return gRecordLeft;
}

int32_t Record_GetRight(void)
{
    return gRecordRight;
}
