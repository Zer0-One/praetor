#ifndef PRAETOR_UTIL
#define PRAETOR_UTIL

#include <stdbool.h>

/**
 * A function that generates random decimal digits
 *
 * \return A random decimal digit
 */
char rdigit();

/*
 * Replaces all occurances of character \b i in character string \b src with
 * characters generated via calls to the function pointed to by \b f
 *
 * \param src The source string
 * \param i The character to be replaced
 * \param f A pointer to a function that generates random characters
 * \param first If set to true, causes the function to replace only the first
 * occurance of character \b i
 * \return The number of replacements that occurred
 */
int strrepl(char* src, char i, char (*f)(), bool first);

#endif
