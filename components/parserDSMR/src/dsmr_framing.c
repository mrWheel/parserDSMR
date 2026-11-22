//-- ==========================================================================
//-- parserDSMR - DSMR/P1 telegram parser
//-- File: src/dsmr_framing.c - readable ASCII framing state machine
//-- Date: 2026-11-16 · Build: 2
//-- ==========================================================================

#include "dsmr_framing.h"

#include "dsmr_crc.h"

//-- Returns the value of a hexadecimal digit or -1 for any other byte.
static int hexDigitValue(uint8_t byte)
{
  if ((byte >= '0') && (byte <= '9'))
  {
    return (int)(byte - '0');
  }

  if ((byte >= 'a') && (byte <= 'f'))
  {
    return (int)(byte - 'a') + 10;
  }

  if ((byte >= 'A') && (byte <= 'F'))
  {
    return (int)(byte - 'A') + 10;
  }

  return -1;
}

void dsmrFramingInit(dsmrFraming *framing, uint8_t *buffer, size_t capacity)
{
  framing->buffer = buffer;
  framing->capacity = capacity;
  dsmrFramingRestart(framing);
}

void dsmrFramingRestart(dsmrFraming *framing)
{
  framing->state = DSMR_FRAMING_SEEK_HEADER;
  framing->length = 0;
  framing->bodyLength = 0;
  framing->runningCrc = dsmrCrc16ArcInit();
  framing->trailerCrc = 0;
  framing->trailerDigits = 0;
  framing->crcPresent = false;
  framing->frameActive = false;
}

//-- Stores one byte. Returns false when the candidate exceeds the buffer.
static bool storeByte(dsmrFraming *framing, uint8_t byte, bool partOfCrc)
{
  if ((framing->length + 1u) > framing->capacity)
  {
    return false;
  }

  framing->buffer[framing->length++] = byte;

  if (partOfCrc)
  {
    framing->runningCrc = dsmrCrc16ArcUpdate(framing->runningCrc, &byte, 1);
  }

  return true;
}

dsmrFramingOutcome dsmrFramingByte(dsmrFraming *framing, dsmrCrcPolicy crcPolicy, bool acceptLfLineEndings, uint8_t byte)
{
  switch (framing->state)
  {
    //-- A leading slash at a valid header position starts a candidate.
    case DSMR_FRAMING_SEEK_HEADER:
    {
      if (byte == '/')
      {
        dsmrFramingRestart(framing);
        framing->frameActive = true;
        framing->state = DSMR_FRAMING_IN_BODY;

        if (!storeByte(framing, byte, true))
        {
          return DSMR_FRAMING_FRAME_TOO_LONG;
        }
      }

      return DSMR_FRAMING_NONE;
    }

    //-- Body bytes are stored and enter the CRC until the '!' is reached.
    case DSMR_FRAMING_IN_BODY:
    {
      if (!storeByte(framing, byte, true))
      {
        return DSMR_FRAMING_FRAME_TOO_LONG;
      }

      if (byte == '!')
      {
        framing->bodyLength = framing->length;
        framing->state = DSMR_FRAMING_TRAILER_DIGITS;
        framing->trailerCrc = 0;
        framing->trailerDigits = 0;
        framing->crcPresent = false;
      }

      return DSMR_FRAMING_NONE;
    }

    //-- The four trailer hex digits encode the CRC most significant nibble
    //-- first. Digits and the trailing CR LF are outside the calculation.
    case DSMR_FRAMING_TRAILER_DIGITS:
    {
      int digit = hexDigitValue(byte);

      if (digit >= 0)
      {
        if (crcPolicy == DSMR_CRC_POLICY_ABSENT)
        {
          return DSMR_FRAMING_TRAILER_MALFORMED;
        }

        if (!storeByte(framing, byte, false))
        {
          return DSMR_FRAMING_FRAME_TOO_LONG;
        }

        framing->trailerCrc = (uint16_t)((framing->trailerCrc << 4) | (uint16_t)digit);
        framing->trailerDigits++;

        if (framing->trailerDigits == 4u)
        {
          framing->crcPresent = true;
          framing->state = DSMR_FRAMING_TRAILER_CR;
        }

        return DSMR_FRAMING_NONE;
      }

      //-- A non-digit byte is only valid as the start of the line ending.
      //-- Line ending bytes are stored so a completed frame in the buffer
      //-- is byte-for-byte the sequence as received.
      if (framing->trailerDigits == 0u)
      {
        if (byte == '\r')
        {
          if (crcPolicy == DSMR_CRC_POLICY_REQUIRED)
          {
            return DSMR_FRAMING_TRAILER_MISSING_CRC;
          }

          if (!storeByte(framing, byte, false))
          {
            return DSMR_FRAMING_FRAME_TOO_LONG;
          }

          framing->crcPresent = false;
          framing->state = DSMR_FRAMING_TRAILER_LF;
          return DSMR_FRAMING_NONE;
        }

        if ((byte == '\n') && acceptLfLineEndings)
        {
          if (crcPolicy == DSMR_CRC_POLICY_REQUIRED)
          {
            return DSMR_FRAMING_TRAILER_MISSING_CRC;
          }

          if (!storeByte(framing, byte, false))
          {
            return DSMR_FRAMING_FRAME_TOO_LONG;
          }

          framing->crcPresent = false;
          return DSMR_FRAMING_FRAME_COMPLETE;
        }
      }

      return DSMR_FRAMING_TRAILER_MALFORMED;
    }

    //-- After the last digit or after an absent-CRC '!' a CR is expected. In
    //-- LF compatibility mode the CR may be skipped entirely.
    case DSMR_FRAMING_TRAILER_CR:
    {
      if (byte == '\r')
      {
        if (!storeByte(framing, byte, false))
        {
          return DSMR_FRAMING_FRAME_TOO_LONG;
        }

        framing->state = DSMR_FRAMING_TRAILER_LF;
        return DSMR_FRAMING_NONE;
      }

      if ((byte == '\n') && acceptLfLineEndings)
      {
        if (!storeByte(framing, byte, false))
        {
          return DSMR_FRAMING_FRAME_TOO_LONG;
        }

        return DSMR_FRAMING_FRAME_COMPLETE;
      }

      return DSMR_FRAMING_TRAILER_MALFORMED;
    }

    //-- The trailer always closes on LF.
    case DSMR_FRAMING_TRAILER_LF:
    default:
    {
      if (byte == '\n')
      {
        if (!storeByte(framing, byte, false))
        {
          return DSMR_FRAMING_FRAME_TOO_LONG;
        }

        return DSMR_FRAMING_FRAME_COMPLETE;
      }

      return DSMR_FRAMING_TRAILER_MALFORMED;
    }
  }
}
