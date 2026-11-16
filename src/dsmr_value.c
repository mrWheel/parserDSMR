//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_value.c - locale-independent value and timestamp parsing
//-- Date: 2026-11-16 · Build: 2
//-- ==========================================================================

#include "dsmr_value.h"

#include <string.h>

//-- Powers of ten used to keep the mantissa and scale of a decimal value.
static const int64_t pow10Table[19] =
{
  1LL, 10LL, 100LL, 1000LL, 10000LL, 100000LL, 1000000LL, 10000000LL,
  100000000LL, 1000000000LL, 10000000000LL, 100000000000LL, 1000000000000LL,
  10000000000000LL, 100000000000000LL, 1000000000000000LL, 10000000000000000LL,
  100000000000000000LL, 1000000000000000000LL
};

static bool isDigitChar(char character)
{
  return (character >= '0') && (character <= '9');
}

//-- Multiplies the running mantissa by ten and adds one digit. Returns false
//-- on overflow so the caller can raise a value-overflow diagnostic.
static bool accumulateDigit(int64_t *mantissa, char digitChar)
{
  int64_t digit = (int64_t)(digitChar - '0');

  if ((*mantissa > (INT64_MAX / 10)) || ((*mantissa == (INT64_MAX / 10)) && (digit > (INT64_MAX % 10))))
  {
    return false;
  }

  *mantissa = (*mantissa * 10) + digit;
  return true;
}

static bool textEqualsIgnoreCase(const char *text, const char *reference)
{
  while ((*text != '\0') && (*reference != '\0'))
  {
    char left = *text;
    char right = *reference;

    if ((left >= 'a') && (left <= 'z'))
    {
      left = (char)(left - ('a' - 'A'));
    }

    if ((right >= 'a') && (right <= 'z'))
    {
      right = (char)(right - ('a' - 'A'));
    }

    if (left != right)
    {
      return false;
    }

    text++;
    reference++;
  }

  return (*text == '\0') && (*reference == '\0');
}

static uint8_t daysInMonth(uint16_t year, uint8_t month)
{
  static const uint8_t table[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  uint8_t days = table[month - 1u];

  if ((month == 2u) && (((year % 4u) == 0u) && (((year % 100u) != 0u) || ((year % 400u) == 0u))))
  {
    days = 29;
  }

  return days;
}

int64_t dsmrValuePow10(uint8_t scale)
{
  if (scale >= 19u)
  {
    return pow10Table[18];
  }

  return pow10Table[scale];
}

void dsmrValueSetAbsent(dsmrValue *value)
{
  value->kind = DSMR_VALUE_ABSENT;
  value->protocolInvalid = false;
  value->mantissa = 0;
  value->scale = 0;
  value->text = NULL;
}

void dsmrValueSetText(dsmrValue *value, const char *text)
{
  value->kind = DSMR_VALUE_TEXT;
  value->protocolInvalid = false;
  value->mantissa = 0;
  value->scale = 0;
  value->text = text;
}

int dsmrValueParseDecimal(const char *text, uint8_t maxScale, dsmrValue *value)
{
  dsmrValueSetAbsent(value);

  size_t length = strlen(text);

  //-- An empty group represents a value the meter does not publish.
  if (length == 0u)
  {
    return 1;
  }

  //-- The protocol marks an explicitly invalid value as FF.
  if (textEqualsIgnoreCase(text, "FF"))
  {
    value->kind = DSMR_VALUE_NUMBER;
    value->protocolInvalid = true;
    return 1;
  }

  size_t index = 0;
  bool negative = false;

  if ((text[index] == '+') || (text[index] == '-'))
  {
    negative = (text[index] == '-');
    index++;
  }

  int64_t mantissa = 0;
  uint8_t scale = 0;
  bool sawDigit = false;

  while ((index < length) && isDigitChar(text[index]))
  {
    if (!accumulateDigit(&mantissa, text[index]))
    {
      return -1;
    }

    sawDigit = true;
    index++;
  }

  if ((index < length) && (text[index] == '.'))
  {
    index++;

    while ((index < length) && isDigitChar(text[index]))
    {
      if (!accumulateDigit(&mantissa, text[index]))
      {
        return -1;
      }

      if (scale >= maxScale)
      {
        return 0;
      }

      scale++;
      sawDigit = true;
      index++;
    }
  }

  if (!sawDigit || (index != length))
  {
    return 0;
  }

  value->kind = DSMR_VALUE_NUMBER;
  value->protocolInvalid = false;
  value->mantissa = negative ? (-mantissa) : mantissa;
  value->scale = scale;
  return 1;
}

int dsmrValueParseInteger(const char *text, dsmrValue *value)
{
  dsmrValueSetAbsent(value);

  size_t length = strlen(text);

  if (length == 0u)
  {
    return 1;
  }

  size_t index = 0;
  bool negative = false;

  if ((text[index] == '+') || (text[index] == '-'))
  {
    negative = (text[index] == '-');
    index++;
  }

  int64_t mantissa = 0;
  bool sawDigit = false;

  while (index < length)
  {
    if (!isDigitChar(text[index]))
    {
      return 0;
    }

    if (!accumulateDigit(&mantissa, text[index]))
    {
      return -1;
    }

    sawDigit = true;
    index++;
  }

  if (!sawDigit)
  {
    return 0;
  }

  value->kind = DSMR_VALUE_NUMBER;
  value->protocolInvalid = false;
  value->mantissa = negative ? (-mantissa) : mantissa;
  value->scale = 0;
  return 1;
}

bool dsmrTimestampParse(const char *text, dsmrValue *value, dsmrTimestamp *timestamp)
{
  memset(timestamp, 0, sizeof *timestamp);
  dsmrValueSetText(value, text);

  size_t length = strlen(text);
  size_t index = 0;
  uint8_t digits[14];
  uint8_t digitCount = 0;

  while ((index < length) && isDigitChar(text[index]) && (digitCount < 14u))
  {
    digits[digitCount++] = (uint8_t)(text[index] - '0');
    index++;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  size_t used = 0;

  if (digitCount == 14u)
  {
    year = (uint16_t)((digits[0] * 1000u) + (digits[1] * 100u) + (digits[2] * 10u) + digits[3]);
    used = 14u;
  }
  else if (digitCount == 12u)
  {
    //-- Documented century rule of the core: two digit years resolve as 20YY.
    year = (uint16_t)(2000u + (digits[0] * 10u) + digits[1]);
    used = 12u;
  }
  else
  {
    return false;
  }

  month = (uint8_t)((digits[used - 10u] * 10u) + digits[used - 9u]);
  day = (uint8_t)((digits[used - 8u] * 10u) + digits[used - 7u]);
  hour = (uint8_t)((digits[used - 6u] * 10u) + digits[used - 5u]);
  minute = (uint8_t)((digits[used - 4u] * 10u) + digits[used - 3u]);
  second = (uint8_t)((digits[used - 2u] * 10u) + digits[used - 1u]);

  index = used;

  //-- Optional fractional seconds are retained in the source text only.
  if ((index < length) && (text[index] == '.'))
  {
    index++;

    while ((index < length) && isDigitChar(text[index]))
    {
      index++;
    }
  }

  timestamp->year = year;
  timestamp->month = month;
  timestamp->day = day;
  timestamp->hour = hour;
  timestamp->minute = minute;
  timestamp->second = second;

  if ((index < length) && ((text[index] == '+') || (text[index] == '-')))
  {
    bool negativeOffset = (text[index] == '-');
    index++;

    uint8_t offsetDigits[4];
    uint8_t offsetCount = 0;

    while ((index < length) && isDigitChar(text[index]) && (offsetCount < 4u))
    {
      offsetDigits[offsetCount++] = (uint8_t)(text[index] - '0');
      index++;
    }

    if ((offsetCount != 2u) && (offsetCount != 4u))
    {
      return false;
    }

    uint8_t offsetHours = (uint8_t)((offsetDigits[0] * 10u) + offsetDigits[1]);
    uint8_t offsetMinutes = (offsetCount == 4u) ? (uint8_t)((offsetDigits[2] * 10u) + offsetDigits[3]) : 0u;
    int32_t total = ((int32_t)offsetHours * 60) + (int32_t)offsetMinutes;

    timestamp->hasOffset = true;
    timestamp->offsetMinutes = (int16_t)(negativeOffset ? (-total) : total);
  }

  //-- Optional daylight saving indicator, the S or W character.
  if ((index < length) && ((text[index] == 'S') || (text[index] == 'W')))
  {
    timestamp->dst = text[index];
    index++;
  }

  if (index != length)
  {
    return false;
  }

  if ((month < 1u) || (month > 12u) || (day < 1u) || (day > daysInMonth(year, month)))
  {
    return false;
  }

  if ((hour > 23u) || (minute > 59u) || (second > 59u))
  {
    return false;
  }

  timestamp->valid = true;
  return true;
}
