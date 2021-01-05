//
// Created by suitm on 2020/12/28.
//

#ifndef ISSTR_H
#define ISSTR_H

#define ISSTR_VERSION_NO "20.12.2"
char * isstr_version();

char *isstr_trim(char *str);
char *isstr_split(char *str, char * sign, int num, char * value);

#endif //WRKTCP_ISSTR_H
