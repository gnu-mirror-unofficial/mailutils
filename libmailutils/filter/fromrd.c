#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <mailutils/errno.h>
#include <mailutils/filter.h>
#include <mailutils/stream.h>

enum
  {
    S_INIT,   /* Initial state */
    S_BOL,    /* Beginning of line */
    S_ESC,    /* Collecting > escapes */
    S_FROM,   /* Collecting "From " characters */
  };

struct transcoder
{
  int state; /* Transcoder state */
  int count; /* Number of consecutive '>' seen. */
  int len;   /* Number of "From" characters collected */
};

static char from_line[] = "From ";

/* Move min(isize,osize) bytes from iptr to optr, removing initial '>'
   from each sequence '>+From ' at the beginning of line */
static enum mu_filter_result
_fromrd_decoder (void *xd,
		 enum mu_filter_command cmd,
		 struct mu_filter_io *iobuf)
{
  const unsigned char *iptr;
  size_t isize;
  char *optr;
  size_t osize;
  struct transcoder *xcode = xd;
  size_t i, j, k;
  size_t len, reqlen;

  switch (cmd)
    {
    case mu_filter_init:
      xcode->state = S_BOL;
      xcode->count = 0;
      xcode->len = 0;
      return mu_filter_ok;
      
    case mu_filter_done:
      return mu_filter_ok;
      
    default:
      break;
    }
  
  iptr = (const unsigned char *) iobuf->input;
  isize = iobuf->isize;
  optr = iobuf->output;
  osize = iobuf->osize;

  for (i = j = 0; i < isize && j < osize; i++)
    {
      unsigned char c = *iptr++;

      switch (xcode->state)
	{
	case S_INIT:
	  optr[j++] = c;
	  if (c == '\n')
	    xcode->state = S_BOL;
	  break;
	  
	case S_BOL:
	  if (c == '>')
	    {
	      xcode->state = S_ESC;
	      xcode->count = 1;
	    }
	  else
	    {
	      optr[j++] = c;
	      xcode->state = S_INIT;
	    }
	  break;
	  
	case S_ESC:
	  if (c == '>')
	    {
	      xcode->count++;
	    }
	  else if (c == from_line[0])
	    {
	      xcode->state = S_FROM;
	      xcode->len = 1;
	    }
	  else
	    {
	      xcode->state = S_INIT;
	      goto emit;
	    }	      
	  break;
	  
	case S_FROM:
	  if (from_line[xcode->len] == 0)
	    {
	      xcode->count--;
	    }
	  else if (c == from_line[xcode->len])
	    {
	      xcode->len++;
	      continue;
	    }
	  else
	    {
	      //RESTORE
	    }
	emit:
	  reqlen = xcode->len + xcode->count;
	  len = osize - j;
	  if (len < reqlen)
	    {
	      iobuf->osize = reqlen;
	      return mu_filter_moreoutput;
	    }
	  for (k = 0; k < xcode->count; k++, j++)
	    optr[j] = '>';
	  memcpy (optr + j, from_line, xcode->len);
	  j += xcode->len;

	  xcode->state = S_INIT;
	  xcode->count = xcode->len = 0;
	  i--;
	  iptr--;
	}
    }
  iobuf->isize = i;
  iobuf->osize = j;
  return mu_filter_ok;
}

static enum mu_filter_result
_fromrd_encoder (void *xd,
		 enum mu_filter_command cmd,
		 struct mu_filter_io *iobuf)
{
  const unsigned char *iptr;
  size_t isize;
  char *optr;
  size_t osize;
  struct transcoder *xcode = xd;
  size_t i, j, k;
  size_t len, reqlen;

  switch (cmd)
    {
    case mu_filter_init:
      xcode->state = S_BOL;
      xcode->count = 0;
      xcode->len = 0;
      return mu_filter_ok;
      
    case mu_filter_done:
      return mu_filter_ok;
      
    default:
      break;
    }
  
  iptr = (const unsigned char *) iobuf->input;
  isize = iobuf->isize;
  optr = iobuf->output;
  osize = iobuf->osize;

  for (i = j = 0; i < isize && j < osize; i++)
    {
      unsigned char c = *iptr++;

      switch (xcode->state)
	{
	case S_INIT:
	  optr[j++] = c;
	  if (c == '\n')
	    xcode->state = S_BOL;
	  break;
	  
	case S_BOL:
	  if (c == '>')
	    {
	      xcode->state = S_ESC;
	      xcode->count = 1;
	    }
	  else if (c == from_line[0])
	    {
	      xcode->state = S_FROM;
	      xcode->count = 0;
	      xcode->len = 1;
	    }
	  else
	    {
	      optr[j++] = c;
	      xcode->state = S_INIT;
	    }
	  break;
	  
	case S_ESC:
	  if (c == '>')
	    {
	      xcode->count++;
	    }
	  else if (c == from_line[0])
	    {
	      xcode->state = S_FROM;
	      xcode->len = 1;
	    }
	  else
	    {
	      xcode->state = S_INIT;
	      goto emit;
	    }	      
	  break;
	  
	case S_FROM:
	  if (from_line[xcode->len] == 0)
	    {
	      xcode->count++;
	    }
	  else if (c == from_line[xcode->len])
	    {
	      xcode->len++;
	      continue;
	    }
	  else
	    {
	      //RESTORE
	    }
	emit:	  
	  reqlen = xcode->len + xcode->count;
	  len = osize - j;
	  if (len < reqlen)
	    {
	      iobuf->osize = reqlen;
	      return mu_filter_moreoutput;
	    }
	  for (k = 0; k < xcode->count; k++, j++)
	    optr[j] = '>';
	  memcpy (optr + j, from_line, xcode->len);
	  j += xcode->len;

	  xcode->state = S_INIT;
	  xcode->count = xcode->len = 0;
	  i--;
	  iptr--;
	}
    }
  iobuf->isize = i;
  iobuf->osize = j;
  return mu_filter_ok;
}


static int
_fromrd_alloc_state (void **pret, int mode,
		   int argc MU_ARG_UNUSED, const char **argv MU_ARG_UNUSED)
{
  *pret = malloc (sizeof (struct transcoder));
  if (!*pret)
    return ENOMEM;
  return 0;
}

static struct _mu_filter_record _fromrd_filter = {
  "FROMRD",
  _fromrd_alloc_state,
  _fromrd_encoder,
  _fromrd_decoder
};

mu_filter_record_t mu_fromrd_filter = &_fromrd_filter;

/* For compatibility with previous versions */
static struct _mu_filter_record _fromrb_filter = {
  "FROMRB",
  _fromrd_alloc_state,
  _fromrd_encoder,
  _fromrd_decoder
};

mu_filter_record_t mu_fromrb_filter = &_fromrb_filter;


