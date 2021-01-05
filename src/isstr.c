//
// Created by suitm on 2020/12/28.
//

#include <string.h>
#include "isstr.h"

char * isstr_version(){
    return ISSTR_VERSION_NO;
}

/** 去除字符串的前后空格,中间的空格不会去掉 **/
char *isstr_trim(char *str)
{
    int i;
    int b1, e1;

    if (strlen(str) == 0) return str;

    if (str)
    {
        for(i = 0; str[i] == ' '; i++);

        b1 = i;

        for(i = strlen(str) - 1; i >= b1 && str[i] == ' '; i--);

        e1 = i;

        if (e1 >= b1)
            memmove(str, str+b1, e1-b1+1);

        str[e1-b1+1] = 0;
        return str;
    }
    else return str;
}

char * isstr_split(char *str, char * sign, int num, char * value){
    int i;
    char *p, *q;
    int sign_len;

    sign_len = strlen(sign);

    q = str;

    num--;

    /** 跳过前 num 个 sign,查找开始位置 **/
    while(num != 0){
        q = strstr( q, sign);
        if( q == NULL){
            return NULL;
        }
        q = q + sign_len;
        num--;
    }

    /* 开始位置 */
    p = q;

    /** 获取下一个sign, 或者结尾 **/
    q = strstr(q, sign);
    if( q == NULL){
        i = strlen(p);
    }else{
        i = q - p;
    }

    strncpy(value, p, i);
    value[i] = 0x00;

    return value;

}