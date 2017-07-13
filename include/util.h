/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015-2017 David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

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
 * \param src   The source string
 * \param i     The character to be replaced
 * \param f     A pointer to a function that generates random characters
 * \param first If set to true, causes the function to replace only the first
 *              occurance of character \b i
 *
 * \return The number of replacements that occurred
 */
int strrepl(char* src, char i, char (*f)(), bool first);

#endif
