#define  _IN_TTYIO_  1  /* switch to help out ipport.h */
#define PRINTF_STDARG      1  /* C compiler supports VARARGS */
#define NATIVE_SNPRINTF    1  /* use target functions , needed by IPSEC */

//#include <stdarg.h>
#include "ttyio.h"
#include "string.h"
#include <math.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

#define  FIELDWIDTH     1        /* flag to switch in Field Width code */
#define  LINESIZE       144      /* max # chars per field */
#define doputchar(sendByte)    putch(sendByte)

typedef unsigned short int Uint16;

int doprint(char *target, unsigned tlen, const char *sp, va_list va);

va_list PRINTF(const char *format,...)
{
    va_list arg = NULL;
    va_start(arg, format);
    doprint(NULL, 32767, format, arg);
    va_end(arg);
    return(arg);
}

/* struct output_ctx - output context, used by doprint() and
 * its helper functions
 */
struct output_ctx
{
   char *target;
   unsigned int tlen;
   unsigned int outlen;
};

/* local doprint helper routines. All take target as in doprint() */
static void hexbyte(struct output_ctx *outctxp, unsigned x);
static void hexword(struct output_ctx *outctxp, unsigned x);
static void declong(struct output_ctx *outctxp, long lg);
#ifdef FIELDWIDTH
static int setnum(char **nptr); /* fetch field width value from string */
static int fplen(const char *sp, void *varp); /* get field length of var */
static void reverse(char* str, int len);
static int intToStr(int x, char str[], int d);
static void ftoa(float n, char* res, int afterpoint);

#endif   /* FIELDWIDTH */

static wchar_t digits[] = { L"0123456789ABCDEF" };

int doprint(char *target, unsigned tlen, const char *sp, va_list va)
{
    char *cp = NULL;
    unsigned prefill = 0U, postfill = 0U, fieldlen = 0U; /* Variables for field len padding */
    int swap = 0U; /* flag and temp holder for prefill-postfill swap */
    unsigned char fillchar = 0;
    int maxfieldlen = LINESIZE; /* this can be set to control precision */
    unsigned minfieldlen = 0U;
    int i = 0U;
    unsigned tmp = 0U;
    unsigned long lng = 0UL;
    /*********************/
    float fltarg = 0.0f;
    char strbuff[20] = { 0 };
    /********************/
    struct output_ctx outctx = {.target = target, .outlen = 0, .tlen = tlen};
    struct output_ctx *outctxp = &outctx;
#ifdef PRINTF_STDARG
    unsigned w0 = 0U;
    int i0 = 0U;
    unsigned char c = 0;
    char *cap = NULL;
    void *varp = NULL;
#endif   /* PRINTF_STDARG */

    while (*sp != (char) 0)
    {
        if (*sp != '%')
        {
            doputchar(*sp++);
            continue;
        }

        /* fall to here if sp ==> '%' */
        sp++; /* point past '%' */

        /* see if any field width control stuff is present */
        cp = (char *) sp; /* save pointer to filed width data in cp */

        while (*sp == '-' || (*sp >= '0' && *sp <= '9') || *sp == '.')
        {
            sp++;
        } /* scan past field control goodies */

#ifdef PRINTF_STDARG
        switch (*sp)
        {
            case 'p' : /* '%p' - pointer */
                lng =(unsigned long) va_arg(va, unsigned long);
            break;
            case 'x' : /* '%x' - this always does 0 prefill */
                tmp = va_arg(va,unsigned);
            break;
            case 'd' : /* '%d' */
                i0 = (int)va_arg(va, int);
            break;
            case 'u' : /* '%u' */
                w0 = (unsigned)va_arg(va, unsigned);
            break;
            case 'c' : /* '%c' */
                c = (unsigned char)va_arg(va, unsigned);
            break;
            case 's' : /* '%s' */
                cap = va_arg(va, char *);
            break;
            case 'f' : /* '%f' */
                fltarg = va_arg(va, float);
                //fltarg = va_arg(va, double);
            break;
            case 'l' :
                if (sp[1] == (char) 'x' || sp[1] == (char) 'd' || sp[1] == (char) 'u') /* '%lx', '%ld', or '%lu' */
                {
                    lng = (unsigned long)va_arg(va, unsigned long);
                } else {
                    lng = (long int)va_arg(va, long int);
                }
            /*  else   '%l?', ignore it */
            break;
        default: /* %?, ignore it */
        break;
        } /* end switch *sp */
#endif   /* PRINTF_STDARG */

#ifdef FIELDWIDTH
        prefill = postfill = 0U; /* default to no filling */
        fillchar = (unsigned char) ' '; /* ...but fill with spaces, if filling */
        swap = (int) TRUE; /* ...and swap prefill & postfill */
        if (sp != cp) /* if there's field control stuff... */
        {
            if (*cp == (char) '-') /* field is to be left adjusted */
            {
                swap = (int) FALSE; /* leave pXXfill unswaped */
                cp++;
            }
            else
            {
                swap = (int) TRUE;
            } /* we will swap prefill & postfill later */

            /* set prefill, postfill for left adjustment */
            if (*cp == (char) '0') /* fill char is '0', not space default */
            {
                cp++;
                fillchar = (unsigned char) '0';
            }
            else
            {
                fillchar = (unsigned char) ' ';
            }

            minfieldlen = (unsigned) setnum(&cp); /* get number, advance cp */

            switch (*sp)
            {
                case 's' :
                    varp = (void *) &cap;
                break;
                case 'd' :
                    varp = (void *) &i0;
                break;
                case 'u' :
                    varp = (void *) &w0;
                break;
                case 'l' :
                    varp = (void *) &lng;
                break;
                case 'f' :
                    varp = (void *) &fltarg;
                break;
                default:
                    varp = NULL;
                break;
            }
            fieldlen = (unsigned) fplen(sp, varp);

            if (*cp == '.') /* do we have a max field size? */
            {
                cp++; /* point to number past '.' */
                maxfieldlen = setnum(&cp);
            }

            if (maxfieldlen < (int) fieldlen)
            {
                fieldlen = (unsigned) maxfieldlen;
            }
            if (minfieldlen > fieldlen)
            {
                postfill = minfieldlen - fieldlen;
            }
            else
            {
                postfill = 0U;
            }
            if ((postfill + fieldlen) > (unsigned) LINESIZE)
            {
                postfill = 0U; /* sanity check*/
            }
        }

        if (swap != 0) /* caller wanted right adjustment, swap prefill/postfill */
        {
            swap = (int) prefill;
            prefill = postfill;
            postfill = (unsigned) swap;
        }

        while (prefill-- != 0U)
        {
            doputchar((char )fillchar); /* do any pre-field padding */
        }
#endif   /* FIELDWIDTH */

        switch (*sp)
        {
            case 'p' : /* '%p' - pointer */
                hexword(outctxp, (unsigned) (lng >> 16));
                hexword(outctxp, (unsigned) (lng & (unsigned long) 0x0000FFFF));
            break;
            case 'x' : /* '%x' - this always does 0 prefill */
                if (tmp > 255U)
                {
                    hexword(outctxp, tmp);
                }
                else
                {
                    hexbyte(outctxp, tmp);
                }
            break;
            case 'd' : /* '%d' */
                declong(outctxp, (long) i0);
            break;
            case 'f' :
                ftoa(fltarg, strbuff, (int)4);
                i = 0;
                cap = &strbuff[0];
                while (cap && (*cap) && (i++ < maxfieldlen))
                {
                    doputchar(((int )*cap++));
                }
            break;
            case 'u' : /* '%u' */
                declong(outctxp, (long) w0);
            break;
            case 'c' : /* '%c' */
                doputchar((int )c)
                ;
            break;
            case 's' : /* '%s' */
                i = 0;
                while (cap && (*cap) && (i++ < maxfieldlen))
                {
                    doputchar(((int )*cap++));
                }
            break;
            case 'l' :
                sp++;
                if (*sp == 'x') /* '%lx' */
                {
                    hexword(outctxp, (unsigned) (lng >> 16));
                    hexword(outctxp, (unsigned) (lng & 0x0000FFFFUL));
                }
                else if ((*sp == 'd') || (*sp == 'u')) /* '%ld' or '%lu' */
                {
                    declong(outctxp, (long) lng); /* we treat %lu as %ld. */
                }
                else
                { /* no action*/
                    declong(outctxp, (long) lng); /* %l */;
                }
                /*  else   '%l?', ignore it */
            break;
            default: /* %?, ignore it */
            break;
        } /* end switch *sp */

#ifdef   FIELDWIDTH
        while (postfill-- != 0U)
        {
            doputchar((int )fillchar); /* do any post-field padding */
        }
#endif   /* FIELDWIDTH */

        sp++; /* point past '%?' */

    } /* end while *sp */

    if (outctxp->target != NULL)
    {
        *(outctxp->target) = (char) '\0';
    } /* Null terminate the string */

    return ((int) outctxp->outlen);
}

/* FUNCTION: hexword()
 *
 * hexword(x) - print 16 bit value as hexadecimal
 *
 * PARAM1: char ** targ
 * PARAM2: unsigned x
 *
 * RETURNS:
 */
static void
hexword(struct output_ctx * outctxp, unsigned int x)
{
   doputchar(digits[((Uint16)(x >> 12U)) &(Uint16)0x0f]);
   doputchar(digits[((Uint16)(x >>  8U)) &(Uint16)0x0f]);
   doputchar(digits[((Uint16)(x >>  4U)) & (Uint16)0x0f]);
   doputchar(digits[(Uint16)x &(Uint16)0x0f]);
}



/* FUNCTION: hexbyte()
 *
 * PARAM1: char ** targ
 * PARAM2: unsigned  x
 *
 * RETURNS:
 */

static void hexbyte(struct output_ctx * outctxp, unsigned int x)
{
   doputchar(digits[(Uint16)(x >> 4U) & (Uint16)0x0f]);
   doputchar(digits[(Uint16)x & (Uint16)0x0f]);
}


/* FUNCTION: declong()
 *
 * declong(char*, long) - print a long to target as a decimal number.
 * Assumes signed, prepends '-' if negative.
 *
 *
 * PARAM1: char ** targ
 * PARAM2: long lg
 *
 * RETURNS:
 */
static void declong(struct output_ctx *outctxp, long lg) 
{
   int  digit;
   long tens = 1L;

   if (lg == 0L)
   {
      doputchar((int)'0');
      return;
   }
   else if (lg < 0L)
   {
      doputchar('-');
      lg = -lg;
   } else { /*no action*/; }

   /* make "tens" the highest power of 10 smaller than lg */
   while ((lg/10L) >= tens) {
   tens *= 10L;}

   while (tens !=0L)
   {
      digit = (int)(lg/tens);    /* get highest digit in lg */

      doputchar(digits[digit]);

      lg -= ((long)digit * tens);
      tens /= 10L;
   }
}


#ifdef FIELDWIDTH

/* FUNCTION: setnum()
 *
 * returns the value of fieldwidth digits from a
 * printf format string. fptr should be a pointer to a pointer to one
 * or more fieldwidth digits. On return, is advanced past the digits
 * and the value of the digits is returned as an int.
 *
 *
 * PARAM1: char ** nptr
 *
 * RETURNS: returns the value of fieldwidth digits from a
 * printf format string
 */
static int setnum(char **nptr)
{
   int  snval = 0;
   char ch;

   while (((ch = **nptr) >= (char)'0') && (ch <= (char)'9')) /* scan through digits */
   {
      snval = (snval * 10) + (int)(ch - (char)'0');  /* calculate return value */
      /* bump pointer (not pointer to pointer) past valid digits */
      (*nptr)++;
   }

   return (snval);
}



/* FUNCTION: fplen()
 *
 * fplen(sp, varp) - returns the number of chars required to print
 * the value pointed to by varp when formatted according to the string
 * pointed to by sp.
 *
 *
 * PARAM1: char * sp
 * PARAM2: void * varp
 *
 * RETURNS:
 */
static int fplen(const char *sp, void *varp)
{
    char *cp = NULL;
    int snval = 0;
    int ret_val = 0;

    /* define the maximum digits needed to display a long value in
     * decimal. Say , sizeof(long) = 4. So it has 8*4=32 bits.
     * 2^10=1024. So for every 10 bits, we can show 3 digits. So for
     * long, this value would be 32*3/10=96/10=9. And then we add one
     * more digit for the roundoff. max_long_dig is used to prevent
     * long overflow in lng1. There are better ways to prevent this,
     * but very difficult to verify whether we made the perfect fix.
     * Hence for now, this should do.
     */
    int max_long_dig = ((8 * (int) sizeof(long) * 3) / 10) + 1;

    switch (*sp)
    /* switch on conversion char */
    {
        case 's' :
            cp = *(char**) varp;
            while (*cp++ != (char) 0)
            {
                snval++;
            }
            ret_val = snval;
        break;

        case 'c' :
            ret_val = 1;
        break;

        case 'x' :
            ret_val = 4;
        break;

        case 'p' :
            ret_val = 9;
        break;

        case 'd' :
        case 'u' :
        {
            long lng1 = 1L;
            long lng2 = *(long *) varp;

            if (lng2 == 0L)
            {
                ret_val = 1;
                break;
            }

            if (lng2 < 0L) /* if variable value is negative... */
            {
                if (*sp == (char) 'd') /* format is signded decimal */
                {
                    snval++; /* add space for '-' sign */
                    lng2 = -lng2;
                }
                else
                { /* *sp == 'u' - format is unsigned */
                    lng2 &= 0xffffL;
                }
            }
            while (lng1 <= lng2)
            {
                lng1 *= 10L;
                snval++;
                if (snval >= max_long_dig)
                {
                    /* If we don't stop now, lng1 will have long overflow */
                    break;
                }
            }
            ret_val = snval;
            break;
        }

        case 'l' :
        {
            switch (*(++sp))
            {
                case 'x' : /* '%lx' always 8 bytes */
                    ret_val = 8;
                break;

                case 'u' : /* treat %lu like %ld */
                case 'd' :
                {
                    long lng1 = 1L;
                    long lng2 = *(long *) varp;

                    if (lng2 == 0L)
                    {
                        ret_val = 1;
                        break;
                    }

                    if (lng2 < 0L)
                    {
                        snval++; /* add space for '-' sign */
                        lng2 = -lng2;
                    }

                    while (lng1 <= lng2)
                    {
                        lng1 *= 10L;
                        snval++;
                        if (snval >= max_long_dig)
                        {
                            /* If we don't stop now, lng1 will have long overflow */
                            break;
                        }
                    }
                    ret_val = snval;
                    break;
                }
                    //break;
                default:
                    ret_val = 0;
                break;
            }
            return ret_val;
        }
        break;
        default:
            ret_val = 0;
        break;
    }
    return ret_val;
}

// Reverses a string 'str' of length 'len'
static void reverse(char* str, int len)
{
    int i = 0, j = len - 1, temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

// Converts a given integer x to string str[].
// d is the number of digits required in the output.
// If d is more than the number of digits in x,
// then 0s are added at the beginning.
static int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating-point/double number to a string.
static void ftoa(float n, char* res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;

    // Extract floating part
    float fpart = n - (float)ipart;

    // convert integer part to string
    int i = intToStr(ipart, res, 0);

    // check for display option after point
    if (afterpoint != 0) {
        res[i] = '.'; // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter
        // is needed to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);

        intToStr((int)fpart, (res + i + 1), afterpoint);
    }
}
#endif   /* FIELDWIDTH */

