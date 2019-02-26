/* -------------------------------------------------------------------------
 * main.c
 * Implements simulation of a single FMU instance
 * that implements the "FMI for Co-Simulation 2.0" interface.
 * Command syntax: see printHelp()
 * Simulates the given FMU from t = 0 .. tEnd with fixed step size h and
 * writes the computed solution to file 'result.csv'.
 * The CSV file (comma-separated values) may e.g. be plotted using
 * OpenOffice Calc or Microsoft Excel.
 * This program demonstrates basic use of an FMU.
 * Real applications may use advanced master algorithms to co-simulate
 * many FMUs, limit the numerical error using error estimation
 * and back-stepping, provide graphical plotting utilities, debug support,
 * and user control of parameter and start values, or perform a clean
 * error handling (e.g. call freeSlaveInstance when a call to the fmu
 * returns with error). All this is missing here.
 *
 * Revision history
 *  07.03.2014 initial version released in FMU SDK 2.0.0
 *
 * Free libraries and tools used to implement this simulator:
 *  - header files from the FMI specification
 *  - libxml2 XML parser, see http://xmlsoft.org
 *  - 7z.exe 4.57 zip and unzip tool, see http://www.7-zip.org
 * Author: Adrian Tirea
 * Copyright QTronic GmbH. All rights reserved.
 * -------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fmi2.h"
#include "sim_support.h"

FMU fmu; // the fmu to simulate

// simulate the given FMU from tStart = 0 to tEnd.
static int simulate(FMU* fmu, double tEnd, double h, fmi2Boolean loggingOn, char separator,
                    int nCategories, char **categories) {
    double time;
    double tStart = 0;                      // start time
    const char *guid;                       // global unique id of the fmu
    const char *instanceName;               // instance name
    fmi2Component c;                        // instance of the fmu
    fmi2Status fmi2Flag;                    // return code of the fmu functions
    char *fmuResourceLocation = getTempResourcesLocation(); // path to the fmu resources as URL, "file://C:\QTronic\sales"
    fmi2Boolean visible = fmi2False;        // no simulator user interface

    fmi2CallbackFunctions callbacks = {fmuLogger, calloc, free, NULL, fmu};  // called by the model during simulation
    ModelDescription* md;                      // handle to the parsed XML file
    fmi2Boolean toleranceDefined = fmi2False;  // true if model description define tolerance
    fmi2Real tolerance = 0;                    // used in setting up the experiment
    ValueStatus vs = valueMissing;
    int nSteps = 0;
    double hh = h;
    Element *defaultExp;
    FILE* file;

    // instantiate the fmu
    md = fmu->modelDescription;
    guid = getAttributeValue((Element *)md, att_guid);
    instanceName = getAttributeValue((Element *)getCoSimulation(md), att_modelIdentifier);
    c = fmu->instantiate(instanceName, fmi2CoSimulation, guid, fmuResourceLocation,
                    &callbacks, visible, loggingOn);
    free(fmuResourceLocation);
    if (!c) return error("could not instantiate model");

    if (nCategories > 0) {
        fmi2Flag = fmu->setDebugLogging(c, fmi2True, nCategories, categories);
        if (fmi2Flag > fmi2Warning) {
            return error("could not initialize model; failed FMI set debug logging");
        }
    }

    defaultExp = getDefaultExperiment(md);
    if (defaultExp) tolerance = getAttributeDouble(defaultExp, att_tolerance, &vs);
    if (vs == valueDefined) {
        toleranceDefined = fmi2True;
    }

    fmi2Flag = fmu->setupExperiment(c, toleranceDefined, tolerance, tStart, fmi2True, tEnd);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI setup experiment");
    }
    fmi2Flag = fmu->enterInitializationMode(c);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI enter initialization mode");
    }
    fmi2Flag = fmu->exitInitializationMode(c);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI exit initialization mode");
    }

    // open result file
    if (!(file = fopen(RESULT_FILE, "w"))) {
        printf("could not write %s because:\n", RESULT_FILE);
        printf("    %s\n", strerror(errno));
        return 0; // failure
    }

    // output solution for time t0
    outputRow(fmu, c, tStart, file, separator, fmi2True);  // output column names
    outputRow(fmu, c, tStart, file, separator, fmi2False); // output values

    // enter the simulation loop
    time = tStart;
    while (time < tEnd) {
        // check not to pass over end time
        if (h > tEnd - time) {
            hh = tEnd - time;
        }
        fmi2Flag = fmu->doStep(c, time, hh, fmi2True);
        if (fmi2Flag == fmi2Discard) {
            fmi2Boolean b;
            // check if model requests to end simulation
            if (fmi2OK != fmu->getBooleanStatus(c, fmi2Terminated, &b)) {
                return error("could not complete simulation of the model. getBooleanStatus return other than fmi2OK");
            }
            if (b == fmi2True) {
                return error("the model requested to end the simulation");
            }
            return error("could not complete simulation of the model");
        }
        if (fmi2Flag != fmi2OK) return error("could not complete simulation of the model");
        time += hh;
        outputRow(fmu, c, time, file, separator, fmi2False); // output values for this step
        nSteps++;
    }

    // end simulation
    fmu->terminate(c);
    fmu->freeInstance(c);
    fclose(file);

    // print simulation summary
    printf("Simulation from %g to %g terminated successful\n", tStart, tEnd);
    printf("  steps ............ %d\n", nSteps);
    printf("  fixed step size .. %g\n", h);
    return 1; // success
}

int main(int argc, char *argv[]) {
    const char* fmuFileName;
    int i;

    // parse command line arguments and load the FMU
    // default arguments value
    double tEnd = 1.0;
    double h=0.1;
    int loggingOn = 0;
    char csv_separator = ',';
    char **categories = NULL;
    int nCategories = 0;

    parseArguments(argc, argv, &fmuFileName, &tEnd, &h, &loggingOn, &csv_separator, &nCategories, &categories);
    loadFMU(fmuFileName);

  // run the simulation
    printf("FMU Simulator: run '%s' from t=0..%g with step size h=%g, loggingOn=%d, csv separator='%c' ",
            fmuFileName, tEnd, h, loggingOn, csv_separator);
    printf("log categories={ ");
    for (i = 0; i < nCategories; i++) printf("%s ", categories[i]);
    printf("}\n");

    simulate(&fmu, tEnd, h, loggingOn, csv_separator, nCategories, categories);
    printf("CSV file '%s' written\n", RESULT_FILE);

    // release FMU
#if WINDOWS
    FreeLibrary(fmu.dllHandle);
#else /* WINDOWS */
    dlclose(fmu.dllHandle);
#endif /* WINDOWS */
    freeModelDescription(fmu.modelDescription);
    if (categories) free(categories);

    // delete temp files obtained by unzipping the FMU
    deleteUnzippedFiles();

    return EXIT_SUCCESS;
}
