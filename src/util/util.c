#include <stdbool.h>
#include <stdlib.h>
#include "../log.h"

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
