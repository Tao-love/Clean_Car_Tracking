/* UART 自动调参协议的解析、校验和无硬件遥测节流。 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "UART_Auto_PID.h"

#define UART_AUTO_PID_SPEED_GAIN_MAX_Q16 (64L * Q16_ONE)
#define UART_AUTO_PID_LINE_GAIN_MAX_Q16  (8L * Q16_ONE)
#define UART_AUTO_PID_REQUIRED_FIELDS    (0x7FU)

#define UART_AUTO_PID_FIELD_SEQ   (0x01U)
#define UART_AUTO_PID_FIELD_LKP   (0x02U)
#define UART_AUTO_PID_FIELD_LKI   (0x04U)
#define UART_AUTO_PID_FIELD_RKP   (0x08U)
#define UART_AUTO_PID_FIELD_RKI   (0x10U)
#define UART_AUTO_PID_FIELD_LINEP (0x20U)
#define UART_AUTO_PID_FIELD_LINED (0x40U)

static UARTAutoPidConfig gConfig;
static UARTAutoPidControlSample gLastSample;
static uint8_t gTelemetryPhase;
static bool gTelemetryReady;

static bool UART_Auto_PID_IsSpace(char character);
static bool UART_Auto_PID_Matches(const char *text, size_t length,
    const char *expected);
static bool UART_Auto_PID_ParseSequence(const char *text, size_t length);
static bool UART_Auto_PID_ParseQ16(const char *text, size_t length,
    int32_t *value);
static bool UART_Auto_PID_ParseSetAll(const char *body, size_t bodyLength,
    ControlParams *candidate);
static bool UART_Auto_PID_GainsAreValid(const ControlParams *params);
static int8_t UART_Auto_PID_HexValue(char character);

void UART_Auto_PID_Init(const UARTAutoPidConfig *config)
{
    if (config != 0) {
        gConfig = *config;
    } else {
        gConfig.params = 0;
        gConfig.requestControlReset = 0;
        gConfig.callbackContext = 0;
    }
    gTelemetryPhase = 0U;
    gTelemetryReady = false;
}

bool UART_Auto_PID_ProcessLineForTest(const char *line)
{
    size_t length;
    size_t star = 0U;
    size_t index;
    uint8_t checksum = 0U;
    int8_t highNibble;
    int8_t lowNibble;
    ControlParams candidate;

    if ((line == 0) || (gConfig.params == 0)) {
        return false;
    }
    for (length = 0U; length <= UART_AUTO_PID_MAX_LINE; length++) {
        if (line[length] == '\0') {
            break;
        }
    }
    if (length > UART_AUTO_PID_MAX_LINE) {
        return false;
    }
    if ((length > 0U) && (line[length - 1U] == '\r')) {
        length--;
    }
    if (UART_Auto_PID_Matches(line, length, "STATUS")) {
        return false;
    }
    for (index = 0U; index < length; index++) {
        if (line[index] == '*') {
            if (star != 0U) {
                return false;
            }
            star = index;
        }
    }
    if ((star == 0U) || ((star + 3U) != length)) {
        return false;
    }
    highNibble = UART_Auto_PID_HexValue(line[star + 1U]);
    lowNibble = UART_Auto_PID_HexValue(line[star + 2U]);
    if ((highNibble < 0) || (lowNibble < 0)) {
        return false;
    }
    for (index = 0U; index < star; index++) {
        checksum ^= (uint8_t) line[index];
    }
    if (checksum != (uint8_t) (((uint8_t) highNibble << 4U) |
        (uint8_t) lowNibble)) {
        return false;
    }
    candidate = *gConfig.params;
    if (!UART_Auto_PID_ParseSetAll(line, star, &candidate) ||
        !UART_Auto_PID_GainsAreValid(&candidate)) {
        return false;
    }
    *gConfig.params = candidate;
    if (gConfig.requestControlReset != 0) {
        gConfig.requestControlReset(gConfig.callbackContext);
    }
    return true;
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

static bool UART_Auto_PID_ParseSequence(const char *text, size_t length)
{
    uint32_t value = 0U;
    size_t index;

    if (length == 0U) {
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
    ControlParams *candidate)
{
    size_t index = 0U;
    uint8_t seen = 0U;

    if ((candidate == 0) || (bodyLength < 6U) ||
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
                tokenEnd - colon - 1U)) {
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
