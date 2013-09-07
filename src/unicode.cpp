/**
 * @file unicode.cpp
 * Detects, read and writes characters in the proper format.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "toks_types.h"
#include "prototypes.h"
#include "unc_ctype.h"
#include <cstring>
#include <cstdlib>


void encode_utf8(int ch, vector<UINT8>& res)
{
   if (ch < 0)
   {
      /* illegal code - do not store */
   }
   else if (ch < 0x80)
   {
      /* 0xxxxxxx */
      res.push_back(ch);
   }
   else if (ch < 0x0800)
   {
      /* 110xxxxx 10xxxxxx */
      res.push_back(0xC0 | (ch >> 6));
      res.push_back(0x80 | (ch & 0x3f));
   }
   else if (ch < 0x10000)
   {
      /* 1110xxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xE0 | (ch >> 12));
      res.push_back(0x80 | ((ch >> 6) & 0x3f));
      res.push_back(0x80 | (ch & 0x3f));
   }
   else if (ch < 0x200000)
   {
      /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xF0 | (ch >> 18));
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >> 6) & 0x3f));
      res.push_back(0x80 | (ch & 0x3f));
   }
   else if (ch < 0x4000000)
   {
      /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xF8 | (ch >> 24));
      res.push_back(0x80 | ((ch >> 18) & 0x3f));
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >> 6) & 0x3f));
      res.push_back(0x80 | (ch & 0x3f));
   }
   else /* (ch <= 0x7fffffff) */
   {
      /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xFC | (ch >> 30));
      res.push_back(0x80 | ((ch >> 24) & 0x3f));
      res.push_back(0x80 | ((ch >> 18) & 0x3f));
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >> 6) & 0x3f));
      res.push_back(0x80 | (ch & 0x3f));
   }
}


/**
 * Decode UTF-8 sequences from in_data and put the chars in out_data.
 * If there are any decoding errors, then return false.
 */
bool decode_utf8(const vector<UINT8>& in_data, deque<int>& out_data)
{
   int idx = 0;
   int ch, tmp, cnt;

   while (idx < (int)in_data.size())
   {
      ch = in_data[idx++];
      if (ch < 0x80)                   /* 1-byte sequence */
      {
         out_data.push_back(ch);
         continue;
      }
      else if ((ch & 0xE0) == 0xC0)    /* 2-byte sequence */
      {
         ch &= 0x1F;
         cnt = 1;
      }
      else if ((ch & 0xF0) == 0xE0)    /* 3-byte sequence */
      {
         ch &= 0x0F;
         cnt = 2;
      }
      else if ((ch & 0xF8) == 0xF0)    /* 4-byte sequence */
      {
         ch &= 0x07;
         cnt = 3;
      }
      else if ((ch & 0xFC) == 0xF8)    /* 5-byte sequence */
      {
         ch &= 0x03;
         cnt = 4;
      }
      else if ((ch & 0xFE) == 0xFC)    /* 6-byte sequence */
      {
         ch &= 0x01;
         cnt = 5;
      }
      else
      {
         /* invalid UTF-8 sequence */
         return false;
      }

      while ((cnt-- > 0) && (idx < (int)in_data.size()))
      {
         tmp = in_data[idx++];
         if ((tmp & 0xC0) != 0x80)
         {
            /* invalid UTF-8 sequence */
            return false;
         }
         ch = (ch << 6) | (tmp & 0x3f);
      }
      if (cnt >= 0)
      {
         /* short UTF-8 sequence */
         return false;
      }
      out_data.push_back(ch);
   }
   return true;
}


/**
 * Extract 2 bytes from the stream and increment idx by 2
 */
static int get_word(const vector<UINT8>& in_data, int& idx, bool be)
{
   int ch;

   if ((idx + 2) > (int)in_data.size())
   {
      ch = -1;
   }
   else if (be)
   {
      ch = (in_data[idx] << 8) | in_data[idx + 1];
   }
   else
   {
      ch = in_data[idx] | (in_data[idx + 1] << 8);
   }
   idx += 2;
   return ch;
}


/**
 * Decode a UTF-16 sequence.
 */
bool decode_utf16(const vector<UINT8>& in_data, deque<int>& out_data, CharEncoding enc)
{
   int idx = 0;

   if (in_data.size() & 1)
   {
      /* can't have an odd length */
      return false;
   }

   if (in_data.size() < 2)
   {
      /* we require at least 1 char */
      return false;
   }

   bool be = (enc == ENC_UTF16_BE);

   while (idx < (int)in_data.size())
   {
      int ch = get_word(in_data, idx, be);
      if ((ch & 0xfc00) == 0xd800)
      {
         ch  &= 0x3ff;
         ch <<= 10;
         int tmp = get_word(in_data, idx, be);
         if ((tmp & 0xfc00) != 0xdc00)
         {
            return false;
         }
         ch |= (tmp & 0x3ff);
         ch += 0x10000;
         out_data.push_back(ch);
      }
      else if (((ch >= 0) && (ch < 0xD800)) || (ch >= 0xE000))
      {
         out_data.push_back(ch);
      }
      else
      {
         /* invalid character */
         return false;
      }
   }
   return true;
}


/**
 * Looks for the BOM of UTF-16 and UTF-8.
 * On return the p_file position indicator will be past any bom found.
 */
CharEncoding decode_bom(FILE *p_file)
{
   CharEncoding enc = ENC_UTF8;
   unsigned char data[6];
   int len, skip = 0;

   len = fread(data, 1, 6, p_file);

   if (len >= 2)
   {
      if ((data[0] == 0xfe) && (data[1] == 0xff))
      {
         skip = 2;
         enc = ENC_UTF16_BE;
      }
      else if ((data[0] == 0xff) && (data[1] == 0xfe))
      {
         skip = 2;
         enc = ENC_UTF16_LE;
      }
      else if ((len >= 3) &&
               (data[0] == 0xef) &&
               (data[1] == 0xbb) &&
               (data[2] == 0xbf))
      {
         skip = 3;
      }
   }

   if ((len >= 6) && (enc == ENC_UTF8))
   {
      if ((data[0] == 0) && (data[2] == 0) && (data[4] == 0))
      {
         enc = ENC_UTF16_BE;
      }
      else if ((data[1] == 0) && (data[3] == 0) && (data[5] == 0))
      {
         enc = ENC_UTF16_LE;
      }
   }

   fseek(p_file, skip, SEEK_SET);
   return enc;
}

