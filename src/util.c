/*
* This source file is part of praetor, a free and open-source IRC bot,
* designed to be robust, portable, and easily extensible.
*
* Copyright (c) 2015-2018 David Zero
* All rights reserved.
*
* The following code is licensed for use, modification, and redistribution
* according to the terms of the Revised BSD License. The text of this license
* can be found in the "LICENSE" file bundled with this source distribution.
*/

#include <stdbool.h>
#include <stdlib.h>
#include "log.h"

char rdigit(){
    return (random() % 10) + 48;
}

int strrepl(char* src, char i, char (*f)(), bool first){
    int repl = 0;
    for(int count = 0; src[count] != 0; count++){
        if(src[count] == i){
            src[count] = (*f)();
            repl++;
            if(first){return repl;}
        }
    }
    return repl;
}
