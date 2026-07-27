/* UART 自动调参协议解析、遥测格式化及非阻塞 UART2 轮询服务。 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "UART_Auto_PID.h"
#include "line_run.h"
#include "ti_msp_dl_config.h"

#define UART_AUTO_PID_SPEED_GAIN_MAX_Q16 (64L * Q16_ONE)
#define UART_AUTO_PID_LINE_GAIN_MAX_Q16  (8L * Q16_ONE)
#define UART_AUTO_PID_REQUIRED_FIELDS    (0x7FU)
#define UART_AUTO_PID_TX_FIFO_SIZE       (512U)
#define UART_AUTO_PID_MAX_FRAME          (128U)
#define UART_AUTO_PID_Q16_DECIMALS       (100000U)
#define UART_AUTO_PID_TELEMETRY_LIMIT    (9999)

#define UART_AUTO_PID_FIELD_SEQ   (0x01U)
#define UART_AUTO_PID_FIELD_LKP   (0x02U)
#define UART_AUTO_PID_FIELD_LKI   (0x04U)
#define UART_AUTO_PID_FIELD_RKP   (0x08U)
#define UART_AUTO_PID_FIELD_RKI   (0x10U)
#define UART_AUTO_PID_FIELD_LINEP (0x20U)
#define UART_AUTO_PID_FIELD_LINED (0x40U)

typedef enum {
    UART_AUTO_PID_COMMAND_IGNORED = 0,
    UART_AUTO_PID_COMMAND_STATUS,
    UART_AUTO_PID_COMMAND_ACCEPTED,
    UART_AUTO_PID_COMMAND_REJECTED
} UARTAutoPidCommandResult;

static UARTAutoPidConfig gConfig;
static UARTAutoPidControlSample gLastSample;
static uint8_t gTelemetryPhase;
static bool gTelemetryReady;
static bool gTelemetryPending;
static char gRxLine[UART_AUTO_PID_MAX_LINE + 1U];
static size_t gRxLength;
static bool gRxDiscarding;
static uint8_t gTxFifo[UART_AUTO_PID_TX_FIFO_SIZE];
static uint16_t gTxHead;
static uint16_t gTxTail;
static uint16_t gTxCount;

static bool UART_Auto_PID_IsSpace(char character);
static bool UART_Auto_PID_Matches(const char *text, size_t length,
    const char *expected);
static bool UART_Auto_PID_ParseSequence(const char *text, size_t length,
    uint16_t *sequence);
static bool UART_Auto_PID_ParseQ16(const char *text, size_t length,
    int32_t *value);
static bool UART_Auto_PID_ParseSetAll(const char *body, size_t bodyLength,
    ControlParams *candidate, uint16_t *sequence);
static bool UART_Auto_PID_ParseRun(const char *body, size_t bodyLength,
    uint16_t *sequence);
static bool UART_Auto_PID_GainsAreValid(const ControlParams *params);
static int8_t UART_Auto_PID_HexValue(char character);
static uint8_t UART_Auto_PID_Checksum(const char *text, size_t length);
static bool UART_Auto_PID_ExtractSequence(const char *body,
    size_t bodyLength, uint16_t *sequence);
static bool UART_Auto_PID_IsRunCommand(const char *text, size_t length);
static UARTAutoPidCommandResult UART_Auto_PID_ProcessLine(
    const char *line, size_t length, uint16_t *sequence,
    const char **reason);
static bool UART_Auto_PID_AppendCharacter(char *destination,
    size_t capacity, size_t *length, char character);
static bool UART_Auto_PID_AppendText(char *destination, size_t capacity,
    size_t *length, const char *text);
static bool UART_Auto_PID_AppendUnsigned(char *destination,
    size_t capacity, size_t *length, uint32_t value);
static bool UART_Auto_PID_AppendInteger(char *destination,
    size_t capacity, size_t *length, int32_t value);
static bool UART_Auto_PID_AppendQ16(char *destination, size_t capacity,
    size_t *length, int32_t value);
static bool UART_Auto_PID_AppendColumnPrefix(char *destination,
    size_t capacity, size_t *length, uint8_t columnCount);
static bool UART_Auto_PID_AppendIntegerColumn(char *destination,
    size_t capacity, size_t *length, uint8_t *columnCount, int32_t value);
static bool UART_Auto_PID_AppendQ16Column(char *destination,
    size_t capacity, size_t *length, uint8_t *columnCount, int32_t value);
static bool UART_Auto_PID_ProtectFrame(char *frame, size_t capacity,
    size_t *length);
static bool UART_Auto_PID_FormatTelemetry(char *frame, size_t capacity,
    size_t *length);
static bool UART_Auto_PID_QueueBytes(const char *bytes, size_t length);
static bool UART_Auto_PID_QueueProtectedBody(const char *body,
    size_t bodyLength);
static void UART_Auto_PID_QueueCommandResponse(
    UARTAutoPidCommandResult result, uint16_t sequence, const char *reason);
static bool UART_Auto_PID_QueueTelemetry(void);
static void UART_Auto_PID_HandleCompletedLine(const char *line,
    size_t length);
static void UART_Auto_PID_DrainHardwareTx(void);
static int32_t UART_Auto_PID_ClampTelemetry(int32_t value);

void UART_Auto_PID_Init(const UARTAutoPidConfig *config)
{
    if (config != 0) {
        gConfig = *config;
    } else {
        gConfig.params = 0;
        gConfig.requestControlReset = 0;
        gConfig.callbackContext = 0;
    }
    gLastSample = (UARTAutoPidControlSample) {0};
    gTelemetryPhase = 0U;
    gTelemetryReady = false;
    gTelemetryPending = false;
    gRxLength = 0U;
    gRxDiscarding = false;
    gTxHead = 0U;
    gTxTail = 0U;
    gTxCount = 0U;
}

void UART_Auto_PID_Service(void)
{
    UART_Auto_PID_DrainHardwareTx();

    while (!DL_UART_Main_isRXFIFOEmpty(UART_DEBUG_INST)) {
        char received = (char) DL_UART_Main_receiveData(UART_DEBUG_INST);

        if (received == '\r') {
            continue;
        }
        if (received == '\n') {
            if (!gRxDiscarding) {
                gRxLine[gRxLength] = '\0';
                UART_Auto_PID_HandleCompletedLine(gRxLine, gRxLength);
            }
            gRxLength = 0U;
            gRxDiscarding = false;
            continue;
        }
        if (gRxDiscarding) {
            continue;
        }
        if (gRxLength < UART_AUTO_PID_MAX_LINE) {
            gRxLine[gRxLength] = received;
            gRxLength++;
        } else {
            gRxLength = 0U;
            gRxDiscarding = true;
        }
    }

    if (gTelemetryPending && UART_Auto_PID_QueueTelemetry()) {
        gTelemetryPending = false;
    }
    UART_Auto_PID_DrainHardwareTx();
}

bool UART_Auto_PID_ProcessLineForTest(const char *line)
{
    char normalized[UART_AUTO_PID_MAX_LINE + 1U];
    size_t sourceLength = 0U;
    size_t normalizedLength = 0U;
    uint16_t sequence;
    const char *reason;

    if (line == 0) {
        return false;
    }
    while (sourceLength <= UART_AUTO_PID_MAX_LINE) {
        char character = line[sourceLength];

        if (character == '\0') {
            break;
        }
        if (character != '\r') {
            if (normalizedLength == UART_AUTO_PID_MAX_LINE) {
                return false;
            }
            normalized[normalizedLength] = character;
            normalizedLength++;
        }
        sourceLength++;
    }
    if (sourceLength > UART_AUTO_PID_MAX_LINE) {
        return false;
    }
    normalized[normalizedLength] = '\0';
    return UART_Auto_PID_ProcessLine(normalized, normalizedLength,
        &sequence, &reason) == UART_AUTO_PID_COMMAND_ACCEPTED;
}

void UART_Auto_PID_OnControlSample(const UARTAutoPidControlSample *sample)
{
    if (sample == 0) {
        return;
    }
    gLastSample = *sample;
    gTelemetryPhase++;
    if (gTelemetryPhase == 3U) {
        gTelemetryPhase = 0U;
        gTelemetryReady = true;
        gTelemetryPending = true;
    } else {
        gTelemetryReady = false;
    }
}

bool UART_Auto_PID_IsTelemetryReadyForTest(void)
{
    return gTelemetryReady;
}

static bool UART_Auto_PID_IsSpace(char character)
{
    return (character == ' ') || (character == '\t');
}

static bool UART_Auto_PID_Matches(const char *text, size_t length,
    const char *expected)
{
    size_t index;

    for (index = 0U; index < length; index++) {
        if ((expected[index] == '\0') || (text[index] != expected[index])) {
            return false;
        }
    }
    return expected[index] == '\0';
}

static bool UART_Auto_PID_ParseSequence(const char *text, size_t length,
    uint16_t *sequence)
{
    uint32_t value = 0U;
    size_t index;

    if ((sequence == 0) || (length == 0U)) {
        return false;
    }
    for (index = 0U; index < length; index++) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return false;
        }
        value = (value * 10U) + (uint32_t) (text[index] - '0');
        if (value > 65535U) {
            return false;
        }
    }
    *sequence = (uint16_t) value;
    return true;
}

static bool UART_Auto_PID_ParseQ16(const char *text, size_t length,
    int32_t *value)
{
    int64_t whole = 0;
    int64_t fraction = 0;
    int64_t scale = 1;
    int64_t result;
    size_t index = 0U;
    size_t fractionDigits = 0U;
    bool sawWholeDigit = false;

    if ((value == 0) || (length == 0U)) {
        return false;
    }
    while ((index < length) && (text[index] != '.')) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return false;
        }
        whole = (whole * 10) + (int64_t) (text[index] - '0');
        if (whole > (INT32_MAX / Q16_ONE)) {
            return false;
        }
        sawWholeDigit = true;
        index++;
    }
    if (!sawWholeDigit) {
        return false;
    }
    if (index < length) {
        index++;
        while (index < length) {
            if ((text[index] < '0') || (text[index] > '9') ||
                (fractionDigits >= 6U)) {
                return false;
            }
            fraction = (fraction * 10) + (int64_t) (text[index] - '0');
            scale *= 10;
            fractionDigits++;
            index++;
        }
        if (fractionDigits == 0U) {
            return false;
        }
    }
    result = (whole * Q16_ONE) +
        ((fraction * Q16_ONE + (scale / 2)) / scale);
    if (result > INT32_MAX) {
        return false;
    }
    *value = (int32_t) result;
    return true;
}

static bool UART_Auto_PID_ParseSetAll(const char *body, size_t bodyLength,
    ControlParams *candidate, uint16_t *sequence)
{
    size_t index = 0U;
    uint8_t seen = 0U;

    if ((candidate == 0) || (sequence == 0) || (bodyLength < 6U) ||
        !UART_Auto_PID_Matches(body, 6U, "SETALL")) {
        return false;
    }
    index = 6U;
    while (index < bodyLength) {
        size_t tokenStart;
        size_t colon;
        size_t tokenEnd;
        uint8_t field;
        int32_t parsedValue;

        if (!UART_Auto_PID_IsSpace(body[index])) {
            return false;
        }
        while ((index < bodyLength) && UART_Auto_PID_IsSpace(body[index])) {
            index++;
        }
        tokenStart = index;
        colon = bodyLength;
        while ((index < bodyLength) && !UART_Auto_PID_IsSpace(body[index])) {
            if ((body[index] == ':') && (colon == bodyLength)) {
                colon = index;
            }
            index++;
        }
        tokenEnd = index;
        if ((colon == bodyLength) || (colon == tokenStart) ||
            ((colon + 1U) == tokenEnd)) {
            return false;
        }
        field = 0U;
        if (UART_Auto_PID_Matches(&body[tokenStart], colon - tokenStart,
            "SEQ")) {
            field = UART_AUTO_PID_FIELD_SEQ;
            if (!UART_Auto_PID_ParseSequence(&body[colon + 1U],
                tokenEnd - colon - 1U, sequence)) {
                return false;
            }
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "LKP")) {
            field = UART_AUTO_PID_FIELD_LKP;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->speedKpLeftQ16 = parsedValue;
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "LKI")) {
            field = UART_AUTO_PID_FIELD_LKI;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->speedKiLeftQ16 = parsedValue;
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "RKP")) {
            field = UART_AUTO_PID_FIELD_RKP;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->speedKpRightQ16 = parsedValue;
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "RKI")) {
            field = UART_AUTO_PID_FIELD_RKI;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->speedKiRightQ16 = parsedValue;
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "LINEP")) {
            field = UART_AUTO_PID_FIELD_LINEP;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->lineKpQ16 = parsedValue;
        } else if (UART_Auto_PID_Matches(&body[tokenStart],
            colon - tokenStart, "LINED")) {
            field = UART_AUTO_PID_FIELD_LINED;
            if (!UART_Auto_PID_ParseQ16(&body[colon + 1U],
                tokenEnd - colon - 1U, &parsedValue)) {
                return false;
            }
            candidate->lineKdQ16 = parsedValue;
        } else {
            return false;
        }
        if ((seen & field) != 0U) {
            return false;
        }
        seen |= field;
    }
    return seen == UART_AUTO_PID_REQUIRED_FIELDS;
}

static bool UART_Auto_PID_ParseRun(const char *body, size_t bodyLength,
    uint16_t *sequence)
{
    size_t index = 3U;
    size_t tokenStart;
    size_t tokenEnd;

    if ((sequence == 0) || (bodyLength < 3U) ||
        !UART_Auto_PID_Matches(body, 3U, "RUN") ||
        ((index == bodyLength) || !UART_Auto_PID_IsSpace(body[index]))) {
        return false;
    }
    while ((index < bodyLength) && UART_Auto_PID_IsSpace(body[index])) {
        index++;
    }
    tokenStart = index;
    while ((index < bodyLength) && !UART_Auto_PID_IsSpace(body[index])) {
        index++;
    }
    tokenEnd = index;
    if ((tokenEnd <= (tokenStart + 4U)) ||
        !UART_Auto_PID_Matches(&body[tokenStart], 3U, "SEQ") ||
        (body[tokenStart + 3U] != ':') ||
        !UART_Auto_PID_ParseSequence(&body[tokenStart + 4U],
            tokenEnd - tokenStart - 4U, sequence)) {
        return false;
    }
    while (index < bodyLength) {
        if (!UART_Auto_PID_IsSpace(body[index])) {
            return false;
        }
        index++;
    }
    return true;
}

static bool UART_Auto_PID_GainsAreValid(const ControlParams *params)
{
    return (params->speedKpLeftQ16 >= 0) &&
        (params->speedKpLeftQ16 <= UART_AUTO_PID_SPEED_GAIN_MAX_Q16) &&
        (params->speedKiLeftQ16 >= 0) &&
        (params->speedKiLeftQ16 <= UART_AUTO_PID_SPEED_GAIN_MAX_Q16) &&
        (params->speedKpRightQ16 >= 0) &&
        (params->speedKpRightQ16 <= UART_AUTO_PID_SPEED_GAIN_MAX_Q16) &&
        (params->speedKiRightQ16 >= 0) &&
        (params->speedKiRightQ16 <= UART_AUTO_PID_SPEED_GAIN_MAX_Q16) &&
        (params->lineKpQ16 >= 0) &&
        (params->lineKpQ16 <= UART_AUTO_PID_LINE_GAIN_MAX_Q16) &&
        (params->lineKdQ16 >= 0) &&
        (params->lineKdQ16 <= UART_AUTO_PID_LINE_GAIN_MAX_Q16);
}

static int8_t UART_Auto_PID_HexValue(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return (int8_t) (character - '0');
    }
    if ((character >= 'A') && (character <= 'F')) {
        return (int8_t) (character - 'A' + 10);
    }
    if ((character >= 'a') && (character <= 'f')) {
        return (int8_t) (character - 'a' + 10);
    }
    return -1;
}

static uint8_t UART_Auto_PID_Checksum(const char *text, size_t length)
{
    uint8_t checksum = 0U;
    size_t index;

    for (index = 0U; index < length; index++) {
        checksum ^= (uint8_t) text[index];
    }
    return checksum;
}

static bool UART_Auto_PID_ExtractSequence(const char *body,
    size_t bodyLength, uint16_t *sequence)
{
    size_t index = 6U;

    if ((sequence == 0) || (bodyLength < 3U)) {
        return false;
    }
    if (UART_Auto_PID_IsRunCommand(body, bodyLength)) {
        return UART_Auto_PID_ParseRun(body, bodyLength, sequence);
    }
    if ((bodyLength < 6U) || !UART_Auto_PID_Matches(body, 6U, "SETALL")) {
        return false;
    }
    while (index < bodyLength) {
        size_t tokenStart;
        size_t tokenEnd;

        while ((index < bodyLength) && UART_Auto_PID_IsSpace(body[index])) {
            index++;
        }
        tokenStart = index;
        while ((index < bodyLength) && !UART_Auto_PID_IsSpace(body[index])) {
            index++;
        }
        tokenEnd = index;
        if ((tokenEnd > (tokenStart + 4U)) &&
            UART_Auto_PID_Matches(&body[tokenStart], 3U, "SEQ") &&
            (body[tokenStart + 3U] == ':') &&
            UART_Auto_PID_ParseSequence(&body[tokenStart + 4U],
                tokenEnd - tokenStart - 4U, sequence)) {
            return true;
        }
    }
    return false;
}

static bool UART_Auto_PID_IsRunCommand(const char *text, size_t length)
{
    return (length >= 3U) && UART_Auto_PID_Matches(text, 3U, "RUN") &&
        ((length == 3U) || UART_Auto_PID_IsSpace(text[3]) ||
            (text[3] == '*'));
}

static UARTAutoPidCommandResult UART_Auto_PID_ProcessLine(
    const char *line, size_t length, uint16_t *sequence,
    const char **reason)
{
    size_t star = length;
    size_t index;
    uint8_t checksum;
    int8_t highNibble;
    int8_t lowNibble;
    ControlParams candidate;
    bool isSetAll;
    bool isRun;

    *sequence = 0U;
    *reason = "FORMAT";
    if (UART_Auto_PID_Matches(line, length, "STATUS")) {
        return UART_AUTO_PID_COMMAND_STATUS;
    }
    isSetAll = (length >= 6U) && UART_Auto_PID_Matches(line, 6U, "SETALL");
    isRun = UART_Auto_PID_IsRunCommand(line, length);
    if (!isSetAll && !isRun) {
        return UART_AUTO_PID_COMMAND_IGNORED;
    }
    for (index = 0U; index < length; index++) {
        if (line[index] == '*') {
            if (star != length) {
                (void) UART_Auto_PID_ExtractSequence(line, star, sequence);
                return UART_AUTO_PID_COMMAND_REJECTED;
            }
            star = index;
        }
    }
    (void) UART_Auto_PID_ExtractSequence(line, star, sequence);
    if ((star == length) || (star == 0U) || ((star + 3U) != length)) {
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    highNibble = UART_Auto_PID_HexValue(line[star + 1U]);
    lowNibble = UART_Auto_PID_HexValue(line[star + 2U]);
    if ((highNibble < 0) || (lowNibble < 0)) {
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    checksum = UART_Auto_PID_Checksum(line, star);
    if (checksum != (uint8_t) (((uint8_t) highNibble << 4U) |
        (uint8_t) lowNibble)) {
        *reason = "CHECKSUM";
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    if (isRun) {
        if (!UART_Auto_PID_ParseRun(line, star, sequence)) {
            return UART_AUTO_PID_COMMAND_REJECTED;
        }
        if (LineRun_IsRunning()) {
            *reason = "BUSY";
            return UART_AUTO_PID_COMMAND_REJECTED;
        }
        if (!LineRun_Start()) {
            *reason = "STATE";
            return UART_AUTO_PID_COMMAND_REJECTED;
        }
        return UART_AUTO_PID_COMMAND_ACCEPTED;
    }
    if ((gConfig.params == 0) || (gConfig.requestControlReset == 0)) {
        *reason = "STATE";
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    candidate = *gConfig.params;
    if (!UART_Auto_PID_ParseSetAll(line, star, &candidate, sequence)) {
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    if (!UART_Auto_PID_GainsAreValid(&candidate)) {
        *reason = "RANGE";
        return UART_AUTO_PID_COMMAND_REJECTED;
    }
    *gConfig.params = candidate;
    gConfig.requestControlReset(gConfig.callbackContext);
    return UART_AUTO_PID_COMMAND_ACCEPTED;
}

static bool UART_Auto_PID_AppendCharacter(char *destination,
    size_t capacity, size_t *length, char character)
{
    if (*length >= capacity) {
        return false;
    }
    destination[*length] = character;
    (*length)++;
    return true;
}

static bool UART_Auto_PID_AppendText(char *destination, size_t capacity,
    size_t *length, const char *text)
{
    while (*text != '\0') {
        if (!UART_Auto_PID_AppendCharacter(destination, capacity, length,
            *text)) {
            return false;
        }
        text++;
    }
    return true;
}

static bool UART_Auto_PID_AppendUnsigned(char *destination,
    size_t capacity, size_t *length, uint32_t value)
{
    char reversed[10];
    size_t digits = 0U;

    do {
        reversed[digits] = (char) ('0' + (value % 10U));
        digits++;
        value /= 10U;
    } while (value != 0U);
    while (digits > 0U) {
        digits--;
        if (!UART_Auto_PID_AppendCharacter(destination, capacity, length,
            reversed[digits])) {
            return false;
        }
    }
    return true;
}

static bool UART_Auto_PID_AppendInteger(char *destination,
    size_t capacity, size_t *length, int32_t value)
{
    uint32_t magnitude = (uint32_t) value;

    if (value < 0) {
        if (!UART_Auto_PID_AppendCharacter(destination, capacity, length,
            '-')) {
            return false;
        }
        magnitude = 0U - magnitude;
    }
    return UART_Auto_PID_AppendUnsigned(destination, capacity, length,
        magnitude);
}

static bool UART_Auto_PID_AppendQ16(char *destination, size_t capacity,
    size_t *length, int32_t value)
{
    uint32_t magnitude = (uint32_t) value;
    uint32_t whole;
    uint32_t fraction;
    char fractionText[5];
    size_t digits = 5U;
    size_t index;

    if (value < 0) {
        if (!UART_Auto_PID_AppendCharacter(destination, capacity, length,
            '-')) {
            return false;
        }
        magnitude = 0U - magnitude;
    }
    whole = magnitude / (uint32_t) Q16_ONE;
    fraction = ((magnitude % (uint32_t) Q16_ONE) *
        UART_AUTO_PID_Q16_DECIMALS + ((uint32_t) Q16_ONE / 2U)) /
        (uint32_t) Q16_ONE;
    if (fraction == UART_AUTO_PID_Q16_DECIMALS) {
        whole++;
        fraction = 0U;
    }
    if (!UART_Auto_PID_AppendUnsigned(destination, capacity, length, whole)) {
        return false;
    }
    if (fraction == 0U) {
        return true;
    }
    for (index = 5U; index > 0U; index--) {
        fractionText[index - 1U] = (char) ('0' + (fraction % 10U));
        fraction /= 10U;
    }
    while ((digits > 0U) && (fractionText[digits - 1U] == '0')) {
        digits--;
    }
    if (!UART_Auto_PID_AppendCharacter(destination, capacity, length, '.')) {
        return false;
    }
    for (index = 0U; index < digits; index++) {
        if (!UART_Auto_PID_AppendCharacter(destination, capacity, length,
            fractionText[index])) {
            return false;
        }
    }
    return true;
}

static bool UART_Auto_PID_AppendColumnPrefix(char *destination,
    size_t capacity, size_t *length, uint8_t columnCount)
{
    return (columnCount == 0U) || UART_Auto_PID_AppendCharacter(destination,
        capacity, length, ',');
}

static bool UART_Auto_PID_AppendIntegerColumn(char *destination,
    size_t capacity, size_t *length, uint8_t *columnCount, int32_t value)
{
    if (!UART_Auto_PID_AppendColumnPrefix(destination, capacity, length,
        *columnCount) || !UART_Auto_PID_AppendInteger(destination, capacity,
        length, value)) {
        return false;
    }
    (*columnCount)++;
    return true;
}

static bool UART_Auto_PID_AppendQ16Column(char *destination,
    size_t capacity, size_t *length, uint8_t *columnCount, int32_t value)
{
    if (!UART_Auto_PID_AppendColumnPrefix(destination, capacity, length,
        *columnCount) || !UART_Auto_PID_AppendQ16(destination, capacity,
        length, value)) {
        return false;
    }
    (*columnCount)++;
    return true;
}

static bool UART_Auto_PID_ProtectFrame(char *frame, size_t capacity,
    size_t *length)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t checksum;

    if ((*length + 4U) > capacity) {
        return false;
    }
    checksum = UART_Auto_PID_Checksum(frame, *length);
    return UART_Auto_PID_AppendCharacter(frame, capacity, length, '*') &&
        UART_Auto_PID_AppendCharacter(frame, capacity, length,
            hex[checksum >> 4U]) &&
        UART_Auto_PID_AppendCharacter(frame, capacity, length,
            hex[checksum & 0x0FU]) &&
        UART_Auto_PID_AppendCharacter(frame, capacity, length, '\n');
}

static bool UART_Auto_PID_FormatTelemetry(char *frame, size_t capacity,
    size_t *length)
{
    uint8_t columnCount = 0U;
    int32_t leftTarget;
    int32_t leftSpeed;
    int32_t rightTarget;
    int32_t rightSpeed;
    int32_t leftPwm;
    int32_t rightPwm;
    int32_t setpoint;
    int32_t input;
    int32_t pwm;

    if ((frame == 0) || (length == 0) || (gConfig.params == 0)) {
        return false;
    }
    *length = 0U;
    leftTarget = UART_Auto_PID_ClampTelemetry(gLastSample.leftTarget);
    leftSpeed = UART_Auto_PID_ClampTelemetry(gLastSample.leftSpeed);
    rightTarget = UART_Auto_PID_ClampTelemetry(gLastSample.rightTarget);
    rightSpeed = UART_Auto_PID_ClampTelemetry(gLastSample.rightSpeed);
    leftPwm = UART_Auto_PID_ClampTelemetry(gLastSample.leftPwm);
    rightPwm = UART_Auto_PID_ClampTelemetry(gLastSample.rightPwm);
    setpoint = (leftTarget + rightTarget) / 2;
    input = (leftSpeed + rightSpeed) / 2;
    pwm = (leftPwm + rightPwm) / 2;

    if (!UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, UART_Auto_PID_ClampTelemetry(
                (int32_t) gLastSample.timestampMs)) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, setpoint) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, input) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, pwm) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, setpoint - input) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->speedKpLeftQ16) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->speedKiLeftQ16) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, 0) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->speedKpRightQ16) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->speedKiRightQ16) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, 0) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, UART_Auto_PID_ClampTelemetry(
                gLastSample.lineError)) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, leftTarget) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, leftSpeed) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, rightTarget) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, rightSpeed) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, leftPwm) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, rightPwm) ||
        !UART_Auto_PID_AppendIntegerColumn(frame, capacity, length,
            &columnCount, gLastSample.running ? 1 : 0) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->lineKpQ16) ||
        !UART_Auto_PID_AppendQ16Column(frame, capacity, length,
            &columnCount, gConfig.params->lineKdQ16)) {
        return false;
    }
    return (columnCount == UART_AUTO_PID_TELEMETRY_COLUMNS) &&
        UART_Auto_PID_ProtectFrame(frame, capacity, length);
}

static bool UART_Auto_PID_QueueBytes(const char *bytes, size_t length)
{
    size_t index;

    if ((length > UART_AUTO_PID_TX_FIFO_SIZE) ||
        (length > (UART_AUTO_PID_TX_FIFO_SIZE - gTxCount))) {
        return false;
    }
    for (index = 0U; index < length; index++) {
        gTxFifo[gTxHead] = (uint8_t) bytes[index];
        gTxHead++;
        if (gTxHead == UART_AUTO_PID_TX_FIFO_SIZE) {
            gTxHead = 0U;
        }
    }
    gTxCount = (uint16_t) (gTxCount + length);
    return true;
}

static bool UART_Auto_PID_QueueProtectedBody(const char *body,
    size_t bodyLength)
{
    char frame[UART_AUTO_PID_MAX_FRAME];
    size_t length = 0U;
    size_t index;

    if ((body == 0) || (bodyLength > UART_AUTO_PID_MAX_FRAME)) {
        return false;
    }
    for (index = 0U; index < bodyLength; index++) {
        frame[length] = body[index];
        length++;
    }
    return UART_Auto_PID_ProtectFrame(frame, sizeof(frame), &length) &&
        UART_Auto_PID_QueueBytes(frame, length);
}

static void UART_Auto_PID_QueueCommandResponse(
    UARTAutoPidCommandResult result, uint16_t sequence, const char *reason)
{
    char body[48];
    size_t length = 0U;

    if (result == UART_AUTO_PID_COMMAND_ACCEPTED) {
        if (!UART_Auto_PID_AppendText(body, sizeof(body), &length, "ACK,") ||
            !UART_Auto_PID_AppendUnsigned(body, sizeof(body), &length,
                sequence)) {
            return;
        }
    } else if (result == UART_AUTO_PID_COMMAND_REJECTED) {
        if (!UART_Auto_PID_AppendText(body, sizeof(body), &length, "ERR,") ||
            !UART_Auto_PID_AppendUnsigned(body, sizeof(body), &length,
                sequence) ||
            !UART_Auto_PID_AppendCharacter(body, sizeof(body), &length, ',') ||
            !UART_Auto_PID_AppendText(body, sizeof(body), &length, reason)) {
            return;
        }
    } else {
        return;
    }
    (void) UART_Auto_PID_QueueProtectedBody(body, length);
}

static bool UART_Auto_PID_QueueTelemetry(void)
{
    char frame[UART_AUTO_PID_MAX_FRAME];
    size_t length;

    return UART_Auto_PID_FormatTelemetry(frame, sizeof(frame), &length) &&
        UART_Auto_PID_QueueBytes(frame, length);
}

static void UART_Auto_PID_HandleCompletedLine(const char *line,
    size_t length)
{
    uint16_t sequence;
    const char *reason;
    UARTAutoPidCommandResult result = UART_Auto_PID_ProcessLine(line,
        length, &sequence, &reason);

    if (result == UART_AUTO_PID_COMMAND_STATUS) {
        if (UART_Auto_PID_QueueTelemetry()) {
            gTelemetryPending = false;
        }
        return;
    }
    UART_Auto_PID_QueueCommandResponse(result, sequence, reason);
}

static void UART_Auto_PID_DrainHardwareTx(void)
{
    while ((gTxCount > 0U) &&
        !DL_UART_Main_isTXFIFOFull(UART_DEBUG_INST)) {
        DL_UART_Main_transmitData(UART_DEBUG_INST, gTxFifo[gTxTail]);
        gTxTail++;
        if (gTxTail == UART_AUTO_PID_TX_FIFO_SIZE) {
            gTxTail = 0U;
        }
        gTxCount--;
    }
}

static int32_t UART_Auto_PID_ClampTelemetry(int32_t value)
{
    if (value > UART_AUTO_PID_TELEMETRY_LIMIT) {
        return UART_AUTO_PID_TELEMETRY_LIMIT;
    }
    if (value < -UART_AUTO_PID_TELEMETRY_LIMIT) {
        return -UART_AUTO_PID_TELEMETRY_LIMIT;
    }
    return value;
}
