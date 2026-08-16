// Debug.cpp: implementation of the CDebug class.
//
//////////////////////////////////////////////////////////////////////
#include "Debug.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <Windows.h>
#include <process.h>
#define DEBUG_FOLDER "C:\\debug"
#define PATH_SEPARATOR "\\"
#ifndef INVALID_FILE_ATTRIBUTES
	#define INVALID_FILE_ATTRIBUTES -1
#endif
#else
#include <unistd.h>
#include <sys/time.h>
#define DEBUG_FOLDER "/tmp/debug"
#define PATH_SEPARATOR "/"
#endif
#include <stdio.h>
#define MAX_PATH_LEN 4096
#define NG_SUCCESS 0
#define NG_ERROR 1

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDebug::CDebug() {}

CDebug::~CDebug() {}


int CDebug::CreateDir(char* pstrPath) {
    if (!pstrPath || strlen(pstrPath) == 0) {
        return NG_ERROR;
    }
	
    char pstrTmpPath[MAX_PATH_LEN];
    strncpy(pstrTmpPath, pstrPath, sizeof(pstrTmpPath) - 1);
    pstrTmpPath[sizeof(pstrTmpPath) - 1] = '\0';
	
    // Odebrani posledního lomitka, pokud existuje
    if (pstrTmpPath[strlen(pstrTmpPath) - 1] == '\\' || pstrTmpPath[strlen(pstrTmpPath) - 1] == '/') {
        pstrTmpPath[strlen(pstrTmpPath) - 1] = '\0';
    }
	
    struct stat st;
    if (stat(pstrTmpPath, &st) == 0) {
        return NG_SUCCESS; // Adresar uz existuje
    }
	
#ifdef _WIN32
    char buffer[MAX_PATH_LEN];
    char* p = buffer;
    char* lastSlash = NULL;
	
    strncpy(buffer, pstrTmpPath, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
	
    // Prochazime cestu a vytvarime slozky postupne
    while (*p) {
        if (*p == '\\' || *p == '/') {
            lastSlash = p;
            *p = '\0';
            if (strlen(buffer) > 0 && GetFileAttributesA(buffer) == INVALID_FILE_ATTRIBUTES) {
                if (!CreateDirectoryA(buffer, NULL)) {
                    return NG_ERROR;
                }
            }
            *p = '\\';
        }
        p++;
    }
	
    // Nakonec vytvorime celou cestu
    if (!CreateDirectoryA(pstrTmpPath, NULL)) {
        return (GetLastError() == ERROR_ALREADY_EXISTS) ? NG_SUCCESS : NG_ERROR;
    }
	
    return NG_SUCCESS;
#else
    return (mkdir(pstrTmpPath, 0755) == 0 || errno == EEXIST) ? NG_SUCCESS : NG_ERROR;
#endif
}


/********************************************************************/
/*** Debug function  ANSI                                         ***/
/********************************************************************/
void CDebug::debug(const char *text, ...) {
    char debug_targ[MAX_PATH_LEN];
    
#ifdef _WIN32
	_snprintf(debug_targ, sizeof(debug_targ), "%s", DEBUG_FOLDER);    
#else
	snprintf(debug_targ, sizeof(debug_targ), "%s", DEBUG_FOLDER);    
#endif
	
    FILE *f1;
    va_list list;
    int status = 0;
    long int pidnum = 0;
    char pom[MAX_PATH_LEN];
	
#ifdef _WIN32
    pidnum = _getpid();
#else
    pidnum = getpid();
#endif
	
    if (debug_targ != NULL) {
        CDebug::CreateDir(debug_targ);
        
		
#ifdef _WIN32		
		_snprintf(pom, sizeof(pom), "%s%s%ld.txt", debug_targ, PATH_SEPARATOR, pidnum);
#else
		snprintf(pom, sizeof(pom), "%s%s%ld.txt", debug_targ, PATH_SEPARATOR, pidnum);		
#endif
		
        srand((unsigned)time(NULL));
        while ((f1 = fopen(pom, "a")) == NULL) {
#ifdef _WIN32
            Sleep(rand() % 100);
#else
            usleep((rand() % 100) * 1000);
#endif
        }
		
        if (f1 != NULL) {
            va_start(list, text);
            if (vfprintf(f1, text, list) < 0) status = -1;
            va_end(list);
            if (fprintf(f1, "\n") < 0) status = -2;
			
            if (status != 0) {
                printf("pid:%ld:>>Cannot write to debug file :%s:\n", pidnum, debug_targ);
            }
            fclose(f1);
        }
    }
}

/********************************************************************/
/*** Dump function                                                ***/
/*** str - jmeno promenne                                       ***/
/*** buf - buffer, ktery se bude vypisovat                        ***/
/*** len - delka dat                                              ***/
/********************************************************************/

void CDebug::dump(char *str, unsigned char *buf, unsigned long len) {
    unsigned int i, j;
    FILE *f1;
    char pom[MAX_PATH_LEN];
    char debug_targ[MAX_PATH_LEN];
	
    
	
#ifdef _WIN32
	_snprintf(debug_targ, sizeof(debug_targ), "%s", DEBUG_FOLDER);
    long int pidnum = _getpid();
#else
	snprintf(debug_targ, sizeof(debug_targ), "%s", DEBUG_FOLDER);
    long int pidnum = getpid();
#endif
	
    if (debug_targ != NULL) {        
		
#ifdef _WIN32		
		_snprintf(pom, sizeof(pom), "%s%s%ld.txt", debug_targ, PATH_SEPARATOR, pidnum);
#else
		snprintf(pom, sizeof(pom), "%s%s%ld.txt", debug_targ, PATH_SEPARATOR, pidnum);
#endif
		
        srand((unsigned)time(NULL));
        while ((f1 = fopen(pom, "a")) == NULL) {
#ifdef _WIN32
            Sleep(rand() % 100);
#else
            usleep((rand() % 100) * 1000);
#endif
        }
		
        if (f1 != NULL) {
            fprintf(f1, "\ndump of %s, len 0x%lx(%lu) ...\n", str, len, len);
            for (i = 0; i < len; i += 16) {
                fprintf(f1, "%08x   ", i);
                for (j = 0; (j < 16) && ((i + j) < len); j++) {
                    fprintf(f1, " %02x", buf[i + j]);
                }
                for (; (j < 16); j++) {
                    fprintf(f1, "   ");
                }
                fprintf(f1, "   ");
                for (j = 0; (j < 16) && ((i + j) < len); j++) {
                    fprintf(f1, "%c", isprint(buf[i + j]) ? buf[i + j] : '.');
                }
                fprintf(f1, "\n");
            }
            fprintf(f1, "end of dump %s\n\n", str);
            fclose(f1);
        }
    }
}
