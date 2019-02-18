/* ------------------------------------------------------------------------- 
 * sim_support.h
 * Functions used by the FMU simulations fmusim_me and fmusim_cs.
 * Copyright QTronic GmbH. All rights reserved.
 * -------------------------------------------------------------------------*/

#if WINDOWS
// Used 7z options, version 4.57:
// -x   Extracts files from an archive with their full paths in the current dir, or in an output dir if specified
// -aoa Overwrite All existing files without prompt
// -o   Specifies a destination directory where files are to be extracted
#define UNZIP_CMD "7z x -aoa -o"
#else /* WINDOWS */
// -o   Overwrite existing files without prompting
// -d   The directory in which to write files.
#define UNZIP_CMD "unzip -o -d "
#endif /* WINDOWS */

#define XML_FILE  "modelDescription.xml"
#define RESULT_FILE "result.csv"
#define BUFSIZE 4096

#if WINDOWS
#ifdef _WIN64
#define DLL_DIR   "binaries\\win64\\"
#else
#define DLL_DIR   "binaries\\win32\\"
#endif

#define DLL_SUFFIX ".dll"

#else /* WINDOWS */
#if __APPLE__

// Use these for platforms other than OpenModelica
#define DLL_DIR   "binaries/darwin64/"
#define DLL_SUFFIX ".dylib"

#else /* __APPLE__ */
// Linux
#ifdef __x86_64
#define DLL_DIR   "binaries/linux64/"
#else
// It may be necessary to compile with -m32, see ../Makefile
#define DLL_DIR   "binaries/linux32/"
#endif /* __x86_64 */
#define DLL_SUFFIX ".so"
#endif /* __APPLE__ */
#endif /* WINDOWS */

#define RESOURCES_DIR "resources"

// return codes of the 7z command line tool
#define SEVEN_ZIP_NO_ERROR 0 // success
#define SEVEN_ZIP_WARNING 1  // e.g., one or more files were locked during zip
#define SEVEN_ZIP_ERROR 2
#define SEVEN_ZIP_COMMAND_LINE_ERROR 7
#define SEVEN_ZIP_OUT_OF_MEMORY 8
#define SEVEN_ZIP_STOPPED_BY_USER 255

void fmuLogger(fmi2Component c, fmi2String instanceName, fmi2Status status, fmi2String category, fmi2String message, ...);
int unzip(const char *zipPath, const char *outPath);
void parseArguments(int argc, char *argv[], const char **fmuFileName, double *tEnd, double *h,
                    int *loggingOn, char *csv_separator, int *nCategories, char **logCategories[]);
void loadFMU(const char *fmuFileName);
int checkFmiVersion(const char *xmlPath);
void deleteUnzippedFiles();
void outputRow(FMU *fmu, fmi2Component c, double time, FILE* file, char separator, fmi2Boolean header);
int error(const char *message);
void printHelp(const char *fmusim);
char *getTempResourcesLocation(); // caller has to free the result
