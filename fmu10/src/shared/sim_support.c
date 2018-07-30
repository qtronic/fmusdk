/* -------------------------------------------------------------------------
 * sim_support.c
 * Functions used by both FMU simulators fmu10sim_me and fmu10sim_cs
 * to parse command-line arguments, to unzip and load an fmu, 
 * to write CSV file, and more.
 * Copyright QTronic GmbH. All rights reserved.
 * -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#ifdef FMI_COSIMULATION
#include "fmi_cs.h"
#else
#include "fmi_me.h"
#endif

#include "xmlVersionParser.h"
#include "sim_support.h"

#if !WINDOWS
#define MAX_PATH 1024
#include <unistd.h>  // mkdtemp()
#include <dlfcn.h> //dlsym()
#endif /* WINDOWS */

extern FMU fmu;

#if WINDOWS
int unzip(const char *zipPath, const char *outPath) {
    int code;
    char binPath[BUFSIZE];
    int n = BUFSIZE + strlen(UNZIP_CMD) + strlen(outPath) + 3 +  strlen(zipPath) + 9;
    char* cmd = (char*)calloc(sizeof(char), n);

    // %FMUSDK_HOME%\bin to find 7z.dll and 7z.exe
    if (!GetEnvironmentVariable("FMUSDK_HOME", binPath, BUFSIZE)) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            printf ("error: Environment variable FMUSDK_HOME not defined\n");
        }
        else {
            printf ("error: Could not get value of FMUSDK_HOME\n");
        }
        return 0; // error
    }
    strcat(binPath, "\\fmu10\\bin");
    // run the unzip command
    // remove "> NUL" to see the unzip protocol
    sprintf(cmd, "%s\\%s\"%s\" \"%s\" > NUL", binPath, UNZIP_CMD, outPath, zipPath);
    // printf("cmd='%s'\n", cmd);
    code = system(cmd);
    free(cmd);
    if (code!=SEVEN_ZIP_NO_ERROR) {
        printf("7z: ");
        switch (code) {
            case SEVEN_ZIP_WARNING:            printf("warning\n"); break;
            case SEVEN_ZIP_ERROR:              printf("error\n"); break;
            case SEVEN_ZIP_COMMAND_LINE_ERROR: printf("command line error\n"); break;
            case SEVEN_ZIP_OUT_OF_MEMORY:      printf("out of memory\n"); break;
            case SEVEN_ZIP_STOPPED_BY_USER:    printf("stopped by user\n"); break;
            default: printf("unknown problem\n");
        }
    }

    return (code==SEVEN_ZIP_NO_ERROR || code==SEVEN_ZIP_WARNING) ? 1 : 0;
}

#else /* WINDOWS */

int unzip(const char *zipPath, const char *outPath) {
    int code;
    int n;
    char* cmd;

    // run the unzip command
    n = strlen(UNZIP_CMD) + strlen(outPath) + 1 +  strlen(zipPath) + 16;
    cmd = (char*)calloc(sizeof(char), n);
    sprintf(cmd, "%s%s \"%s\" > /dev/null", UNZIP_CMD, outPath, zipPath);
    printf("cmd='%s'\n", cmd);
    code = system(cmd);
    free(cmd);
    if (code!=SEVEN_ZIP_NO_ERROR) {
        printf("%s: ", UNZIP_CMD);
        switch (code) {
            case 1:            printf("warning\n"); break;
            case 2:            printf("error\n"); break;
            case 3:            printf("severe error\n"); break;
            case 4:
            case 5:
            case 6:
            case 7:
                               printf("out of memory\n"); break;
            case 10:           printf("command line error\n"); break;
            default:           printf("unknown problem %d\n", code);
        }
    }
    return (code==SEVEN_ZIP_NO_ERROR || code==SEVEN_ZIP_WARNING) ? 1 : 0;
}
#endif /* WINDOWS */

#if WINDOWS
// fileName is an absolute path, e.g. C:\test\a.fmu
// or relative to the current dir, e.g. ..\test\a.fmu
// Does not check for existence of the file
static char* getFmuPath(const char* fileName){
    char pathName[MAX_PATH];
    int n = GetFullPathName(fileName, MAX_PATH, pathName, NULL);
    return n ? strdup(pathName) : NULL;
}

static char* getTmpPath() {
    char tmpPath[BUFSIZE];
    if(! GetTempPath(BUFSIZE, tmpPath)) {
        printf ("error: Could not find temporary disk space\n");
        return NULL;
    }
    strcat(tmpPath, "fmu\\");
    return strdup(tmpPath);
}

#else /* WINDOWS */
// fmuFileName is an absolute path, e.g. "C:\test\a.fmu"
// or relative to the current dir, e.g. "..\test\a.fmu"
static char* getFmuPath(const char* fmuFileName){
  /* Not sure why this is useful.  Just returning the filename. */
  return strdup(fmuFileName);
}
static char* getTmpPath() {
  char template[13];  // Lenght of "fmuTmpXXXXXX" + null
  sprintf(template, "%s", "fmuTmpXXXXXX");
  //char *tmp = mkdtemp(strdup("fmuTmpXXXXXX"));
  char *tmp = mkdtemp(template);
  if (tmp==NULL) {
    fprintf(stderr, "Couldn't create temporary directory\n");
    exit(1);
  }
  char * results = calloc(sizeof(char), strlen(tmp) + 2);
  strncat(results, tmp, strlen(tmp));
  return strcat(results, "/");
}
#endif /* WINDOWS */

char *getTempFmuLocation() {
    char *tempPath = getTmpPath();
    char *fmuLocation = (char *)calloc(sizeof(char), 8 + strlen(tempPath));
    strcpy(fmuLocation, "file://");
    strcat(fmuLocation, tempPath);
    free(tempPath);
    return fmuLocation;
}

static void* getAdr(int* s, FMU *fmu, const char* functionName){
    char name[BUFSIZE];
    void* fp;
    sprintf(name, "%s_%s", getModelIdentifier(fmu->modelDescription), functionName);
#if WINDOWS
    fp = GetProcAddress(fmu->dllHandle, name);
#else /* WINDOWS */
    fp = dlsym(fmu->dllHandle, name);
#endif /* WINDOWS */
    if (!fp) {
        printf ("warning: Function %s not found in dll\n", name);
#if WINDOWS
#else /* WINDOWS */
        printf ("Error was: %s\n", dlerror());
#endif /* WINDOWS */
        *s = 0; // mark dll load as 'failed'
    }
    return fp;
}

// Load the given dll and set function pointers in fmu
// Return 0 to indicate failure
static int loadDll(const char* dllPath, FMU *fmu) {
    int s = 1;
#if WINDOWS
    HANDLE h = LoadLibrary(dllPath);
#else /* WINDOWS */
    HANDLE h = dlopen(dllPath, RTLD_LAZY);
#endif /* WINDOWS */
    if (!h) {
#if WINDOWS
#else
        printf("The error was: %s\n", dlerror());
#endif
        printf("error: Could not load %s\n", dllPath);
        return 0; // failure
    }
    fmu->dllHandle = h;

#ifdef FMI_COSIMULATION   
    fmu->getTypesPlatform        = (fGetTypesPlatform)   getAdr(&s, fmu, "fmiGetTypesPlatform");
    if (s==0) { 
        s = 1; // work around bug for FMUs exported using Dymola 2012 and SimulationX 3.x
        fmu->getTypesPlatform    = (fGetTypesPlatform)   getAdr(&s, fmu, "fmiGetModelTypesPlatform");
        if (s==1) printf("  using fmiGetModelTypesPlatform instead\n");
    }
    fmu->instantiateSlave        = (fInstantiateSlave)   getAdr(&s, fmu, "fmiInstantiateSlave");
    fmu->initializeSlave         = (fInitializeSlave)    getAdr(&s, fmu, "fmiInitializeSlave");
    fmu->terminateSlave          = (fTerminateSlave)     getAdr(&s, fmu, "fmiTerminateSlave");
    fmu->resetSlave              = (fResetSlave)         getAdr(&s, fmu, "fmiResetSlave");
    fmu->freeSlaveInstance       = (fFreeSlaveInstance)  getAdr(&s, fmu, "fmiFreeSlaveInstance");
    fmu->setRealInputDerivatives = (fSetRealInputDerivatives) getAdr(&s, fmu, "fmiSetRealInputDerivatives");
    fmu->getRealOutputDerivatives = (fGetRealOutputDerivatives) getAdr(&s, fmu, "fmiGetRealOutputDerivatives");
    fmu->cancelStep              = (fCancelStep)         getAdr(&s, fmu, "fmiCancelStep");
    fmu->doStep                  = (fDoStep)             getAdr(&s, fmu, "fmiDoStep");
    // SimulationX 3.4 and 3.5 do not yet export getStatus and getXStatus: do not count this as failure here
    fmu->getStatus               = (fGetStatus)          getAdr(&s, fmu, "fmiGetStatus");
    fmu->getRealStatus           = (fGetRealStatus)      getAdr(&s, fmu, "fmiGetRealStatus");
    fmu->getIntegerStatus        = (fGetIntegerStatus)   getAdr(&s, fmu, "fmiGetIntegerStatus");
    fmu->getBooleanStatus        = (fGetBooleanStatus)   getAdr(&s, fmu, "fmiGetBooleanStatus");
    fmu->getStringStatus         = (fGetStringStatus)    getAdr(&s, fmu, "fmiGetStringStatus");

#else // FMI for Model Exchange 1.0
    fmu->getModelTypesPlatform   = (fGetModelTypesPlatform) getAdr(&s, fmu, "fmiGetModelTypesPlatform");
    fmu->instantiateModel        = (fInstantiateModel)   getAdr(&s, fmu, "fmiInstantiateModel");
    fmu->freeModelInstance       = (fFreeModelInstance)  getAdr(&s, fmu, "fmiFreeModelInstance");
    fmu->setTime                 = (fSetTime)            getAdr(&s, fmu, "fmiSetTime");
    fmu->setContinuousStates     = (fSetContinuousStates)getAdr(&s, fmu, "fmiSetContinuousStates");
    fmu->completedIntegratorStep = (fCompletedIntegratorStep)getAdr(&s, fmu, "fmiCompletedIntegratorStep");
    fmu->initialize              = (fInitialize)         getAdr(&s, fmu, "fmiInitialize");
    fmu->getDerivatives          = (fGetDerivatives)     getAdr(&s, fmu, "fmiGetDerivatives");
    fmu->getEventIndicators      = (fGetEventIndicators) getAdr(&s, fmu, "fmiGetEventIndicators");
    fmu->eventUpdate             = (fEventUpdate)        getAdr(&s, fmu, "fmiEventUpdate");
    fmu->getContinuousStates     = (fGetContinuousStates)getAdr(&s, fmu, "fmiGetContinuousStates");
    fmu->getNominalContinuousStates = (fGetNominalContinuousStates)getAdr(&s, fmu, "fmiGetNominalContinuousStates");
    fmu->getStateValueReferences = (fGetStateValueReferences)getAdr(&s, fmu, "fmiGetStateValueReferences");
    fmu->terminate               = (fTerminate)          getAdr(&s, fmu, "fmiTerminate");
#endif 
    fmu->getVersion              = (fGetVersion)         getAdr(&s, fmu, "fmiGetVersion");
    fmu->setDebugLogging         = (fSetDebugLogging)    getAdr(&s, fmu, "fmiSetDebugLogging");
    fmu->setReal                 = (fSetReal)            getAdr(&s, fmu, "fmiSetReal");
    fmu->setInteger              = (fSetInteger)         getAdr(&s, fmu, "fmiSetInteger");
    fmu->setBoolean              = (fSetBoolean)         getAdr(&s, fmu, "fmiSetBoolean");
    fmu->setString               = (fSetString)          getAdr(&s, fmu, "fmiSetString");
    fmu->getReal                 = (fGetReal)            getAdr(&s, fmu, "fmiGetReal");
    fmu->getInteger              = (fGetInteger)         getAdr(&s, fmu, "fmiGetInteger");
    fmu->getBoolean              = (fGetBoolean)         getAdr(&s, fmu, "fmiGetBoolean");
    fmu->getString               = (fGetString)          getAdr(&s, fmu, "fmiGetString");
    return s; 
}

static void printModelDescription(ModelDescription* md){
    Element* e = (Element*)md;  
    int i;
    printf("%s\n", elmNames[e->type]);
    for (i=0; i<e->n; i+=2) 
        printf("  %s=%s\n", e->attributes[i], e->attributes[i+1]);
#ifdef FMI_COSIMULATION   
    if (!md->cosimulation) {
        printf("error: No Implementation element found in model description. This FMU is not for Co-Simulation.\n");
        exit(EXIT_FAILURE);
    }
    e = md->cosimulation->capabilities;
    printf("%s\n", elmNames[e->type]);
    for (i=0; i<e->n; i+=2) 
        printf("  %s=%s\n", e->attributes[i], e->attributes[i+1]);
#endif // FMI_COSIMULATION  
}

void loadFMU(const char* fmuFileName) {
    char* fmuPath;
    char* tmpPath;
    char* xmlPath;
    char* dllPath;
    
    // get absolute path to FMU, NULL if not found
    fmuPath = getFmuPath(fmuFileName);
    if (!fmuPath) exit(EXIT_FAILURE);

    // unzip the FMU to the tmpPath directory
    tmpPath = getTmpPath();
    if (!unzip(fmuPath, tmpPath)) exit(EXIT_FAILURE);

    // parse tmpPath\modelDescription.xml
    xmlPath = calloc(sizeof(char), strlen(tmpPath) + strlen(XML_FILE) + 1);
    sprintf(xmlPath, "%s%s", tmpPath, XML_FILE);

    // parse tmpPath\modelDescription.xml
    xmlPath = calloc(sizeof(char), strlen(tmpPath) + strlen(XML_FILE) + 1);
    sprintf(xmlPath, "%s%s", tmpPath, XML_FILE);
    // check FMI version of the FMU to match current simulator version
    if (!checkFmiVersion(xmlPath)) {
        free(xmlPath);
        free(fmuPath);
        free(tmpPath);
        exit(EXIT_FAILURE);
    }

    fmu.modelDescription = parse(xmlPath);
    free(xmlPath);
    if (!fmu.modelDescription) exit(EXIT_FAILURE);
    printModelDescription(fmu.modelDescription);

    // load the FMU dll
    dllPath = calloc(sizeof(char), strlen(tmpPath) + strlen(DLL_DIR)
            + strlen( getModelIdentifier(fmu.modelDescription)) +  strlen(DLL_SUFFIX) + 1);
    sprintf(dllPath,"%s%s%s%s", tmpPath, DLL_DIR, getModelIdentifier(fmu.modelDescription), DLL_SUFFIX);
    if (!loadDll(dllPath, &fmu)) {
        free(dllPath);
        free(fmuPath);
        free(tmpPath);
        exit(EXIT_FAILURE);
    }
    free(dllPath);
    free(fmuPath);
    free(tmpPath);
}

int checkFmiVersion(const char *xmlPath) {
    char *xmlFmiVersion = extractVersion(xmlPath);
    if (xmlFmiVersion == NULL) {
        printf("The FMI version of the FMU could not be read: %s", xmlPath);
        return 0;
    } else if (strcmp(xmlFmiVersion, fmiVersion) == 0) {
        free(xmlFmiVersion);
        return 1;
    }
    printf("The FMU to simulate is FMI %s standard, but expected a FMI %s standard FMU", fmiVersion, xmlFmiVersion);
    free(xmlFmiVersion);
    return 0;
}

void deleteUnzippedFiles() {
    char *fmuTempPath = getTmpPath();
    char *cmd = (char *)calloc(15 + strlen(fmuTempPath), sizeof(char));
#if WINDOWS
    sprintf(cmd, "rmdir /S /Q %s", fmuTempPath);
#else /* WINDOWS */
    sprintf(cmd, "rm -rf %s", fmuTempPath);
#endif /* WINDOWS */
    system(cmd);
    free(cmd);
    free(fmuTempPath);
}

static void doubleToCommaString(char* buffer, double r){
    char* comma;
    sprintf(buffer, "%.16g", r);
    comma = strchr(buffer, '.');
    if (comma) *comma = ',';
}

// output time and all non-alias variables in CSV format
// if separator is ',', columns are separated by ',' and '.' is used for floating-point numbers.
// otherwise, the given separator (e.g. ';' or '\t') is to separate columns, and ',' is used
// as decimal dot in floating-point numbers.
void outputRow(FMU *fmu, fmiComponent c, double time, FILE* file, char separator, fmiBoolean header) {
    int k;
    fmiReal r;
    fmiInteger i;
    fmiBoolean b;
    fmiString s;
    fmiValueReference vr;
    ScalarVariable** vars = fmu->modelDescription->modelVariables;
    char buffer[32];

    // print first column
    if (header) 
        fprintf(file, "time");
    else {
        if (separator==',')
            fprintf(file, "%.16g", time);
        else {
            // separator is e.g. ';' or '\t'
            doubleToCommaString(buffer, time);
            fprintf(file, "%s", buffer);
        }
    }

    // print all other columns(void *)
    for (k=0; vars[k]; k++) {
        ScalarVariable* sv = vars[k];
        if (getAlias(sv)!=enu_noAlias) continue;
        if (header) {
            // output names only
            if (separator==',') {
                // treat array element, e.g. print a[1, 2] as a[1.2]
                const char* s = getName(sv);
                fprintf(file, "%c", separator);
                while (*s) {
                   if (*s!=' ') fprintf(file, "%c", *s==',' ? '.' : *s);
                   s++;
                }
             }
            else
                fprintf(file, "%c%s", separator, getName(sv));
        }
        else {
            // output values
            vr = getValueReference(sv);
            switch (sv->typeSpec->type){
                case elm_Real:
                    fmu->getReal(c, &vr, 1, &r);
                    if (separator==',') 
                        fprintf(file, ",%.16g", r);
                    else {
                        // separator is e.g. ';' or '\t'
                        doubleToCommaString(buffer, r);
                        fprintf(file, "%c%s", separator, buffer);
                    }
                    break;
                case elm_Integer:
                case elm_Enumeration:
                    fmu->getInteger(c, &vr, 1, &i);
                    fprintf(file, "%c%d", separator, i);
                    break;
                case elm_Boolean:
                    fmu->getBoolean(c, &vr, 1, &b);
                    fprintf(file, "%c%d", separator, b);
                    break;
                case elm_String:
                    fmu->getString(c, &vr, 1, &s);
                    fprintf(file, "%c%s", separator, s);
                    break;
                default: 
                    fprintf(file, "%cNoValueForType=%d", separator,sv->typeSpec->type);
            }
        }
    } // for

    // terminate this row
    fprintf(file, "\n");
}

static const char* fmiStatusToString(fmiStatus status){
    switch (status){
        case fmiOK:      return "ok";
        case fmiWarning: return "warning";
        case fmiDiscard: return "discard";
        case fmiError:   return "error";
        case fmiFatal:   return "fatal";
#ifdef FMI_COSIMULATION
        case fmiPending: return "fmiPending";
#endif
        default:         return "?";
    }
}

// search a fmu for the given variable
// return NULL if not found or vr = fmiUndefinedValueReference
static ScalarVariable* getSV(FMU* fmu, char type, fmiValueReference vr) {
    int i;
    Elm tp;
    ScalarVariable** vars = fmu->modelDescription->modelVariables;
    if (vr==fmiUndefinedValueReference) return NULL;
    switch (type) {
        case 'r': tp = elm_Real;    break;
        case 'i': tp = elm_Integer; break;
        case 'b': tp = elm_Boolean; break;
        case 's': tp = elm_String;  break;
        default:  tp = elm_BAD_DEFINED; break;
    }
    for (i=0; vars[i]; i++) {
        ScalarVariable* sv = vars[i];
        if (vr==getValueReference(sv) && tp==sv->typeSpec->type)
            return sv;
    }
    return NULL;
}

// replace e.g. #r1365# by variable name and ## by # in message
// copies the result to buffer
static void replaceRefsInMessage(const char* msg, char* buffer, int nBuffer, FMU* fmu){
    int i=0; // position in msg
    int k=0; // position in buffer
    int n;
    char c = msg[i];
    while (c!='\0' && k < nBuffer) {
        if (c!='#') {
            buffer[k++]=c;
            i++;
            c = msg[i];
        } else if (strlen(msg + i + 1) >= 3
               && (strncmp(msg + i + 1, "IND", 3) == 0 || strncmp(msg + i + 1, "INF", 3) == 0)) {
            // 1.#IND, 1.#INF
            buffer[k++]=c;
            i++;
            c = msg[i];
        } else {
            char* end = strchr(msg+i+1, '#');
            if (!end) {
                printf("unmatched '#' in '%s'\n", msg);
                buffer[k++]='#';
                break;
            }
            n = end - (msg+i);
            if (n==1) {
                // ## detected, output #
                buffer[k++]='#';
                i += 2;
                c = msg[i];
            }
            else {
                char type = msg[i+1]; // one of ribs
                fmiValueReference vr;
                int nvr = sscanf(msg+i+2, "%u", &vr);
                if (nvr==1) {
                    // vr of type detected, e.g. #r12#
                    ScalarVariable* sv = getSV(fmu, type, vr);
                    const char* name = sv ? getName(sv) : "?";
                    sprintf(buffer+k, "%s", name);
                    k += strlen(name);
                    i += (n+1);
                    c = msg[i]; 
                }
                else {
                    // could not parse the number
                    printf("illegal value reference at position %d in '%s'\n", i+2, msg);
                    buffer[k++]='#';
                    break;
                }
            }
        }
    } // while
    buffer[k] = '\0';
}

#define MAX_MSG_SIZE 1000
void fmuLogger(fmiComponent c, fmiString instanceName, fmiStatus status,
               fmiString category, fmiString message, ...) {
    char msg[MAX_MSG_SIZE];
    char* copy;
    va_list argp;

    // replace C format strings
    va_start(argp, message);
    vsprintf(msg, message, argp);
    va_end(argp);

    // replace e.g. ## and #r12#  
    copy = strdup(msg);
    replaceRefsInMessage(copy, msg, MAX_MSG_SIZE, &fmu);
    free(copy);
    
    // print the final message
    if (!instanceName) instanceName = "?";
    if (!category) category = "?";
    printf("%s %s (%s): %s\n", fmiStatusToString(status), instanceName, category, msg);
}

int error(const char* message){
    printf("%s\n", message);
    return 0;
}

void parseArguments(int argc, char *argv[], const char** fmuFileName, double* tEnd, double* h, int* loggingOn, char* csv_separator) {
    // parse command line arguments
    if (argc>1) {
        *fmuFileName = argv[1];
    }
    else {
        printf("error: no fmu file\n");
        printHelp(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc>2) {
        if (sscanf(argv[2],"%lf", tEnd) != 1) {
            printf("error: The given end time (%s) is not a number\n", argv[2]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc>3) {
        if (sscanf(argv[3],"%lf", h) != 1) {
            printf("error: The given stepsize (%s) is not a number\n", argv[3]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc>4) {
        if (sscanf(argv[4],"%d", loggingOn) != 1 || *loggingOn<0 || *loggingOn>1) {
            printf("error: The given logging flag (%s) is not boolean\n", argv[4]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc>5) {
        if (strlen(argv[5]) != 1) {
            printf("error: The given CSV separator char (%s) is not valid\n", argv[5]);
            exit(EXIT_FAILURE);
        }
        switch (argv[5][0]) {
            case 'c': *csv_separator = ','; break; // comma
            case 's': *csv_separator = ';'; break; // semicolon
            default:  *csv_separator = argv[5][0]; break; // any other char
        }
    }
    if (argc>6) {
        printf("warning: Ignoring %d additional arguments: %s ...\n", argc-6, argv[6]);
        printHelp(argv[0]);
    }
}

void printHelp(const char* fmusim) {
    printf("command syntax: %s <model.fmu> <tEnd> <h> <loggingOn> <csv separator>\n", fmusim);
    printf("   <model.fmu> .... path to FMU, relative to current dir or absolute, required\n");
    printf("   <tEnd> ......... end  time of simulation, optional, defaults to 1.0 sec\n");
    printf("   <h> ............ step size of simulation, optional, defaults to 0.1 sec\n");
    printf("   <loggingOn> .... 1 to activate logging,   optional, defaults to 0\n");
    printf("   <csv separator>. separator in csv file,   optional, c for ',', s for';', defaults to c\n");
}
