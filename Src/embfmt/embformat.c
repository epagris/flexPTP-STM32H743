/* (C) Wiesner András, 2022 */

#include "embformat.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define FORMAT_DELIMITER ('%')

// format length ((nothing),l)
typedef enum {
	LEN_NORMAL, LEN_LONG
} FmtLength;

// format flags ((nothing),0)
typedef enum {
	FLAG_NO = 0, FLAG_LEADING_ZEROS = 1, FLAG_LEADING_SPACES = 2, FLAG_PREPEND_PLUS_SIGN = 4
} FmtFlags;

// format type
typedef enum {
	UNKNOWN = -1, LITERAL_PERCENT, SIGNED_INTEGER, UNSIGNED_INTEGER, DOUBLE, DOUBLE_EXPONENTIAL, UNSIGNED_HEXADECIMAL_INT, UNSIGNED_HEXADECIMAL_INT_UPPERCASE, STRING, CHARACTER
} FmtType;

struct _FmtWord;
typedef int (*printfn)(va_list *va, struct _FmtWord *fmt, char *outbuf, size_t free_space);

// pair of type and designator character
typedef struct {
	char designator;
	FmtType type;
	printfn fn;
} FmtTypeDesignatorPair;

// structure holding format word properties
typedef struct _FmtWord {
	FmtFlags flags;
	int width;
	int precision;
	FmtLength length;
	FmtType type;
	FmtTypeDesignatorPair *pTypeDes;
} FmtWord;

// ---------------------------------------------

// copy a maximum of n characters AND insert '\0' into dst as the n+1-th character
static int string_copy(char *dst, const char *src, size_t n) {
	size_t i;
	int cnt = 0;
	for (i = 0; i < n && src[i] != '\0'; i++) {
		dst[i] = src[i];
		cnt++;
	}
	dst[i] = '\0';
	return cnt;
}

// get the length of a string
static size_t string_length(char *str) {
	size_t len = 0;
	while (*str != '\0') {
		len++;
		str++;
	}
	return len;
}

// calculate power
static uint64_t power(uint64_t a, unsigned long int n) {
	if (n == 0) {
		return 1;
	}
	uint64_t a_orig = a;
	for (unsigned long int i = 0; i < n - 1; i++) {
		a *= a_orig;
	}
	return a;
}

// round to closest base^1 value
static uint64_t round_to_base(uint64_t a, uint64_t base) {
	uint64_t mod = a % base;
	if (2 * mod < base) { // avoid floating-point arithemtics!
		return a - mod;
	} else {
		return a + (base - mod);
	}
}

// ---------------------------------------------

// insert leading characters
// str: inout string
// n_str: length of the input string (NOT buflen, stringlen)
// c: character to be used for filling
// n_insert: number of character to insert
static void insert_leading_characters(char *str, size_t n_str, char c, size_t n_insert) {
	size_t len = string_length(str);
	for (size_t i = 0; i < len; i++) {
		str[len - 1 + n_insert - i] = str[len - i - 1];
	}
	for (size_t i = 0; i < n_insert; i++) {
		str[i] = c;
	}
	str[len + n_insert] = '\0';
}

// get exponent in the scientific form of the input number (i. e. get the highest decimal place)
// l: input number
// base: base
// gntpd: greatest non-zero ten-power divisor
// return: exponent, if l != 0, -1 if l == 0
static int scientific_form_exponent(uint64_t l, unsigned int base, uint64_t *gntpd) {
	int e = -1;
	*gntpd = 1;
	while (l != 0) {
		l /= base;
		*gntpd *= base;
		e++;
	}
	if (*gntpd > 1) {
		(*gntpd) /= base;
	}
	return MAX(e, 0);
}

static int pfn_literal_percent(va_list *va, FmtWord *fmt, char *outbuf, size_t free_space) {
	if (free_space >= 1) {
		outbuf[0] = '%';
		outbuf[1] = '\0';
	}
	return 1;
}

#define INT_PRINT_OUTBUF_SIZE (47)

static char sNumChars[] = "0123456789abcdefABCDEF";

static int print_number(uint64_t u, bool negative, FmtWord *fmt, char *outbuf, size_t free_space) {
	// output buffer fitting the full long int range
	char outstrbuf[INT_PRINT_OUTBUF_SIZE + 1];
	outstrbuf[0] = '\0';
	char *outstr = outstrbuf;

	// separate sign, process only non-negative numbers
	bool sign_prepended = false;
	if (negative) {
		*outstr = '-';
		outstr++;
		sign_prepended = true;
	} else if (fmt->flags & FLAG_PREPEND_PLUS_SIGN) {
		*outstr = '+';
		outstr++;
		sign_prepended = true;
	}

	// base of the numeral system
	int base = 10; // for safety...
	if (fmt->type == UNSIGNED_INTEGER || fmt->type == SIGNED_INTEGER) {
		base = 10;
	} else if (fmt->type == UNSIGNED_HEXADECIMAL_INT || fmt->type == UNSIGNED_HEXADECIMAL_INT_UPPERCASE) {
		base = 16;
	}

	// get number of digits the number consists of
	uint64_t gntpd;
	int digits = scientific_form_exponent(u, base, &gntpd) + 1;

	// convert int to string
	while (gntpd > 0) {
		uint64_t place_value = (u / gntpd); // integer division!
		u -= place_value * gntpd; // substract value corresponding to the place

		if (fmt->type == UNSIGNED_HEXADECIMAL_INT_UPPERCASE && place_value > 9) {
			place_value += 6;
		}
		*outstr = sNumChars[place_value]; // print place value string

		gntpd /= base; // divide by the base (advance to lower place)
		outstr++; // advance in the string
		*outstr = '\0'; // move the terminating zero
	}

	// pad with zeros if requested
	if (fmt->width != -1 && (fmt->flags & FLAG_LEADING_ZEROS || fmt->flags & FLAG_LEADING_SPACES)) {
		int pad_n = MAX(fmt->width - digits - sign_prepended, 0);
		if (pad_n > 0) {
			// pad differently based on padding character
			if (fmt->flags & FLAG_LEADING_ZEROS) { // -00000nnn
				insert_leading_characters(outstrbuf + sign_prepended, INT_PRINT_OUTBUF_SIZE - sign_prepended, '0', pad_n); // insert leading characters
			} else if (fmt->flags & FLAG_LEADING_SPACES) { // _____-nnn
				insert_leading_characters(outstrbuf, INT_PRINT_OUTBUF_SIZE, ' ', pad_n); // insert leading characters
			}
			outstr += pad_n; // advance pointer to the end of the string
		}
	}

	// copy string to output
	int copy_len = string_copy(outbuf, outstrbuf, free_space);
	return copy_len;
}

static int pfn_integer(va_list *va, FmtWord *fmt, char *outbuf, size_t free_space) {
	uint64_t u = 0;
	bool negative = false;
	if (fmt->type == SIGNED_INTEGER) { // for signed integers
		int64_t si;
		if (fmt->length == LEN_NORMAL) { // ...without length specifiers
			si = va_arg((*va), int);
		} else { // ...with length specifiers
			si = va_arg((*va), int64_t);
		}

		// absolute value
		if (si < 0) {
			negative = true;
			u = -si;
		} else {
			u = si;
		}
	} else if (fmt->type == UNSIGNED_INTEGER || fmt->type == UNSIGNED_HEXADECIMAL_INT || fmt->type == UNSIGNED_HEXADECIMAL_INT_UPPERCASE) { // for UNsigned integers
		if (fmt->length == LEN_NORMAL) { // ...without length specifiers
			unsigned int d = va_arg((*va), unsigned int);
			u = d;
		} else { // ...with length specifiers
			u = va_arg((*va), uint64_t);
		}
	}

	// print number
	int copy_len = print_number(u, negative, fmt, outbuf, free_space);
	return copy_len;
}

#define DECIMAL_POINT ('.')

static int pfn_double(va_list *va, FmtWord *fmt, char *outbuf, size_t free_space) {
	// get passed double variable
	double d = va_arg((*va), double);
	bool negative = d < 0;
	if (negative) {
		d *= -1;
	}

	// normalize if exponential form is required
	int exponent = 0;
	if (fmt->type == DOUBLE_EXPONENTIAL) {
		double q = d < 1.0 ? 10.0 : 0.1; // quotient
		int d_exp = d < 1.0 ? -1 : 1; // shift per iteration in exponent
		while (!(d >= 1.0 && d < 10.0)) {
			d *= q;
			exponent += d_exp;
		}
	}

	// formatting of the integer part
	FmtWord int_fmt = { .flags = fmt->flags, .type = UNSIGNED_INTEGER, .width = fmt->width };

	// separate into integer and fractional part
	uint64_t int_part = (uint64_t) d; // integer part
	int copy_len = print_number(int_part, negative, &int_fmt, outbuf, free_space);
	free_space -= copy_len;
	outbuf += copy_len;

	int sum_copy_len = copy_len; // summed copy length

	// fractional part
	if (fmt->precision > 0) {
		// place decimal point
		*outbuf = DECIMAL_POINT;
		outbuf++;
		free_space--;
		sum_copy_len++;

		*outbuf = '\0';

		// prepare format for printing fractional part
		FmtWord frac_fmt = { .flags = FLAG_NO, .width = -1, .type = UNSIGNED_INTEGER };

        // get "leading zeros" in fractional part
		double d_frac = d - (double) int_part;

        // print leading zeros
        int leading_zeros_printed = 0;
        while ((d_frac * 10.0) < 1.0 && leading_zeros_printed < fmt->precision) {
            d_frac *= 10.0;
            *outbuf = '0';
            outbuf++;
            free_space--;
            sum_copy_len++;
            *outbuf = '\0';
            leading_zeros_printed++;
        }

        // extract fractional part as integer
		d_frac *= (double) power(10, fmt->precision - leading_zeros_printed + 1); // get one more digit
		uint64_t frac_part = (uint64_t) d_frac;
		frac_part = round_to_base(frac_part, 10) / 10; // remove last zero digit (result of rounding)

		// print fractional part
		copy_len = print_number(frac_part, false, &frac_fmt, outbuf, free_space);
		free_space -= copy_len;
		outbuf += copy_len;
		sum_copy_len += copy_len;
	}

	// print exponential if format requires
	if (fmt->type == DOUBLE_EXPONENTIAL) {
		// print 'e'
		*outbuf = 'e';
		outbuf++;
		*outbuf = '\0';
		free_space--;
		sum_copy_len++;

		// print value of exponent
		FmtWord exponent_fmt = { .flags = FLAG_PREPEND_PLUS_SIGN | FLAG_LEADING_ZEROS, .width = 2, .type = UNSIGNED_INTEGER };

		unsigned int exponent_abs = exponent > 0 ? exponent : -exponent;

		copy_len = print_number(exponent_abs, exponent < 0, &exponent_fmt, outbuf, free_space);
		free_space -= copy_len;
		outbuf += copy_len;
		sum_copy_len += copy_len;
	}

	return sum_copy_len;
}

// print character
static int pfn_char(va_list *va, FmtWord *fmt, char *outbuf, size_t free_space) {
	int c = va_arg((*va), int);
	if (free_space >= 1) {
		outbuf[0] = (char) c;
		outbuf[1] = '\0';
	}
	return 1;
}

// print string
static int pfn_string(va_list *va, FmtWord *fmt, char *outbuf, size_t free_space) {
	char *str = va_arg((*va), char*);
	return string_copy(outbuf, str, free_space);
}

// format swting assignment table
static FmtTypeDesignatorPair sTypeDesAssignment[] = { { '%', LITERAL_PERCENT, pfn_literal_percent }, { 'd', SIGNED_INTEGER, pfn_integer }, { 'i', SIGNED_INTEGER, pfn_integer }, { 'u',
		UNSIGNED_INTEGER, pfn_integer }, { 'f', DOUBLE, pfn_double }, { 'e', DOUBLE_EXPONENTIAL, pfn_double }, { 'x', UNSIGNED_HEXADECIMAL_INT, pfn_integer }, { 'X',
		UNSIGNED_HEXADECIMAL_INT_UPPERCASE, pfn_integer }, { 's', STRING, pfn_string }, { 'c', CHARACTER, pfn_char }, { '\0', UNKNOWN } // termination
};

// ------------------------------------------------------------

// try to fetch number from the start of the string
// return: number found OR -1 on failure
static char* fetch_number(char *str, int *num) {
	char *start = str;
	char *end = start;
	*num = 0;
	while ('0' <= *end && '9' >= *end) {
		*num *= 10;
		*num += *end - '0';
		end++;
	}

	if (end == start) {
		*num = -1;
	}

	return end;
}

// get number of arguments from the format string
static int get_number_of_arguments_by_format_string(char *format_str) {
	char *iter = format_str;
	int cnt = 0;
	while (*iter != '\0') {
		if (*iter == FORMAT_DELIMITER) {
			if (*(iter + 1) == '%') { // filter out '%%' literals
				iter++;
			} else {
				cnt++;
			}
		}
		iter++;
	}
	return cnt;
}

// seek for format delimiter ('%')
static char* seek_delimiter(char *str) {
	// iterate over characters until either '%' is found or end of string is reached
	while (*(str) != '\0' && *(str) != FORMAT_DELIMITER) {
		str++;
	}

	// return based on what we found
	if (*str == FORMAT_DELIMITER) {
		return str;
	} else {
		return NULL;
	}
}

// get the the formatting word
// str: input string
// begin: pointer to pointer to the begin of the format word
// length: length of the format word (number of bytes to be copied after)
// return: pointer to unprocessed input string
static char* locate_format_word(char *str, char **begin, size_t *length) {
	char *delim_pos = seek_delimiter(str); // seek for format specifier begin
	if (delim_pos == NULL) { // if not found...
		*length = 0; // set to zero if not found
		return NULL; // ...then return
	}

	// here we have pointer to the '%' character of a format "word"

	// a valid format string has a length at least of 1 byte (i. e. something must follow the '%')
	char *word_start = delim_pos + 1;
	*begin = word_start;

	// search the ending of the format word, which can be either a whitespace or a new '%' character (or the '\0')
	char *word_iter = word_start;
	while ((*word_iter > ' ' || word_iter == word_start) && (*word_iter != FORMAT_DELIMITER || word_iter == word_start) && *word_iter != '\0') {
		word_iter++;
	}

	// now, word_iter points to the first character following the previously found delimiter now belonging to the format word
	char *unproc_text = word_iter;

	// calculate the length of the word
	*length = word_iter - word_start;

	return unproc_text;
}

// fetch format word
// str: input text
// word: output format word (if return value != NULL)
// preceding_text: text before format word
// maxlen: maximum STRING LENGTH of the output
// return: pointer to unprocessed text OR NULL on failure
static char* fetch_format_word(char *str, char *word, char **word_begin, size_t maxlen) {
	// locate word
	size_t len;
	char *unproc_text = locate_format_word(str, word_begin, &len);

	// copy
	string_copy(word, *word_begin, MIN(maxlen, len));

	return unproc_text;
}

// fetch type from designator character
static FmtTypeDesignatorPair* get_type_by_designator(char designator) {
	FmtTypeDesignatorPair *iter = sTypeDesAssignment;
	while (iter->designator != '\0' && iter->designator != designator) {
		iter++;
	}
	return iter;
}

#define DEFAULT_PRINT_PRECISION (6)

static char sFlags[] = " 0+";

// decide whether the passed character is a flag indicator
static bool is_flag(char c) {
	char *iter = sFlags;
	while (*iter != '\0') {
		if (*iter == c) {
			return true;
		}
		iter++;
	}
	return false;
}

// process format word into format structure
// str: input format string to process
// word: output format specifier
// rewind: number of characters to rewind input (some characters may be considered wrongly as being part of the format word)
// return: 0 on success, -1 on failure
static int process_format_word(char *str, FmtWord *word, int *rewind) {
	// 1.: look for flags
	word->flags = FLAG_NO;
	while (is_flag(*str)) {
		switch (*str) {
		case '0':
			word->flags |= FLAG_LEADING_ZEROS;
			break;
		case ' ':
			word->flags |= FLAG_LEADING_SPACES;
			break;
		case '+':
			word->flags |= FLAG_PREPEND_PLUS_SIGN;
			break;
		}
		str++;
	}

	// 2.: look for width
	str = fetch_number(str, &(word->width));

	// 3.: look for precision
	word->precision = DEFAULT_PRINT_PRECISION; // default double precision
	if (*str == '.') {
		str++;
		str = fetch_number(str, &(word->precision));
	}

	// 4.: look for length
	word->length = LEN_NORMAL;
	if (*str == 'l') {
		word->length = LEN_LONG;
		str++;
	}

	// 5.: get type from the designator
	word->pTypeDes = get_type_by_designator(*str);
	word->type = word->pTypeDes->type;
	*rewind = string_length(str + 1);

	if (word->type == UNKNOWN) {
		return -1;
	} else {
		return 0;
	}
}

#define MAX_FORMAT_WORD_LEN (15)

unsigned long int vembfmt(char *str, unsigned long int len, char *format, va_list args) {
	// process format string
	long int free_space = len;
	char *unproc_text = format;
	char *unproc_text_next = NULL;
	char word_str[MAX_FORMAT_WORD_LEN + 1];
	char *word_begin;
	size_t sum_copy_len = 0;
	while ((*unproc_text) && (unproc_text_next = fetch_format_word(unproc_text, word_str, &word_begin, MAX_FORMAT_WORD_LEN)) && free_space > 0) {
		// print preceding text
		word_begin--;
		int copy_len = MIN((word_begin - unproc_text), free_space);
		string_copy(str, unproc_text, copy_len);
		free_space -= copy_len;
		str += copy_len;
		sum_copy_len += copy_len;

		// print data
		FmtWord word;
		int rewind;
		process_format_word(word_str, &word, &rewind);
		if (word.type != UNKNOWN) {
			int copy_len = word.pTypeDes->fn(&args, &word, str, free_space); // variable with the same name!
			free_space -= copy_len;
			str += copy_len;
			sum_copy_len += copy_len;
		}

		// advance pointer in unprocessed text
		unproc_text = unproc_text_next - rewind;
	}

	// also copy last part of the string not containing any formatting sequences
	if (unproc_text != NULL) {
		sum_copy_len += string_copy(str, unproc_text, free_space);
	}

	return sum_copy_len;
}

unsigned long int embfmt(char *str, unsigned long int len, char *format, ...) {
	// get number of expected arguments and initiate va_list usage
	int argc = get_number_of_arguments_by_format_string(format);

	va_list args;
	va_start(args, format);

	unsigned long int copy_len = vembfmt(str, len, format, args);

	va_end(args);
	return copy_len;
}
