/* ---------------------------------------------------------------------------*
 * fmuTemplate.c
 * Implementation of the FMI interface based on functions and macros to
 * be defined by the includer of this file.
 * The "FMI for Co-Simulation 2.0", implementation assumes that exactly the
 * following capability flags are set to fmi2True:
 *    canHandleVariableCommunicationStepSize, i.e. fmi2DoStep step size can vary
 * and all other capability flags are set to default, i.e. to fmi2False or 0.
 *
 * Revision history
 *  07.03.2014 initial version released in FMU SDK 2.0.0
 *  02.04.2014 allow modules to request termination of simulation, better time
 *             event handling, initialize() moved from fmi2EnterInitialization to
 *             fmi2ExitInitialization, correct logging message format in fmi2DoStep.
 *  10.04.2014 use FMI 2.0 headers that prefix function and types names with 'fmi2'.
 *  13.06.2014 when fmi2setDebugLogging is called with 0 categories, set all
 *             categories to loggingOn value.
 *  09.07.2014 track all states of Model-exchange and Co-simulation and check
 *             the allowed calling sequences, explicit isTimeEvent parameter for
 *             eventUpdate function of the model, lazy computation of computed values.
 *
 * Author: Adrian Tirea
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

// macro to be used to log messages. The macro check if current 
// log category is valid and, if true, call the logger provided by simulator.
#define FILTERED_LOG(instance, status, categoryIndex, message, ...) if (status == fmi2Error || status == fmi2Fatal || isCategoryLogged(instance, categoryIndex)) \
        instance->functions->logger(instance->functions->componentEnvironment, instance->instanceName, status, \
        logCategoriesNames[categoryIndex], message, ##__VA_ARGS__);

static fmi2String logCategoriesNames[] = {"logAll", "logError", "logFmiCall", "logEvent"};

// array of value references of states
#if NUMBER_OF_STATES>0
fmi2ValueReference vrStates[NUMBER_OF_STATES] = STATES;
#endif

#ifndef max
#define max(a,b) ((a)>(b) ? (a) : (b))
#endif

#ifndef DT_EVENT_DETECT
#define DT_EVENT_DETECT 1e-10
#endif

// ---------------------------------------------------------------------------
// Private helpers used below to validate function arguments
// ---------------------------------------------------------------------------

fmi2Boolean isCategoryLogged(ModelInstance *comp, int categoryIndex);

static fmi2Boolean invalidNumber(ModelInstance *comp, const char *f, const char *arg, int n, int nExpected) {
    if (n != nExpected) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "%s: Invalid argument %s = %d. Expected %d.", f, arg, n, nExpected)
        return fmi2True;
    }
    return fmi2False;
}

static fmi2Boolean invalidState(ModelInstance *comp, const char *f, int statesExpected) {
    if (!comp)
        return fmi2True;
    if (!(comp->state & statesExpected)) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "%s: Illegal call sequence.", f)
        return fmi2True;
    }
    return fmi2False;
}

static fmi2Boolean nullPointer(ModelInstance* comp, const char *f, const char *arg, const void *p) {
    if (!p) {
        comp->state = modelError;
        FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "%s: Invalid argument %s = NULL.", f, arg)
        return fmi2True;
    }
    return fmi2False;
}

static fmi2Boolean vrOutOfRange(ModelInstance *comp, const char *f, fmi2ValueReference vr, int end) {
    if (vr >= end) {
        FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "%s: Illegal value reference %u.", f, vr)
        comp->state = modelError;
        return fmi2True;
    }
    return fmi2False;
}

static fmi2Status unsupportedFunction(fmi2Component c, const char *fName, int statesExpected) {
    ModelInstance *comp = (ModelInstance *)c;
    fmi2CallbackLogger log = comp->functions->logger;
    if (invalidState(comp, fName, statesExpected))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, fName);
    FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "%s: Function not implemented.", fName)
    return fmi2Error;
}

fmi2Status setString(fmi2Component comp, fmi2ValueReference vr, fmi2String value) {
    return fmi2SetString(comp, &vr, 1, &value);
}

// ---------------------------------------------------------------------------
// Private helpers logger
// ---------------------------------------------------------------------------

// return fmi2True if logging category is on. Else return fmi2False.
fmi2Boolean isCategoryLogged(ModelInstance *comp, int categoryIndex) {
    if (categoryIndex < NUMBER_OF_CATEGORIES
        && (comp->logCategories[categoryIndex] || comp->logCategories[LOG_ALL])) {
        return fmi2True;
    }
    return fmi2False;
}

// ---------------------------------------------------------------------------
// FMI functions
// ---------------------------------------------------------------------------
fmi2Component fmi2Instantiate(fmi2String instanceName, fmi2Type fmuType, fmi2String fmuGUID,
                            fmi2String fmuResourceLocation, const fmi2CallbackFunctions *functions,
                            fmi2Boolean visible, fmi2Boolean loggingOn) {
    // ignoring arguments: fmuResourceLocation, visible
    ModelInstance *comp;
    if (!functions->logger) {
        return NULL;
    }

    if (!functions->allocateMemory || !functions->freeMemory) {
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, "error",
                "fmi2Instantiate: Missing callback function.");
        return NULL;
    }
    if (!instanceName || strlen(instanceName) == 0) {
        functions->logger(functions->componentEnvironment, "?", fmi2Error, "error",
                "fmi2Instantiate: Missing instance name.");
        return NULL;
    }
    if (!fmuGUID || strlen(fmuGUID) == 0) {
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, "error",
                "fmi2Instantiate: Missing GUID.");
        return NULL;
    }
    if (strcmp(fmuGUID, MODEL_GUID)) {
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, "error",
                "fmi2Instantiate: Wrong GUID %s. Expected %s.", fmuGUID, MODEL_GUID);
        return NULL;
    }
    comp = (ModelInstance *)functions->allocateMemory(1, sizeof(ModelInstance));
    if (comp) {
        int i;
        comp->r = (fmi2Real *)   functions->allocateMemory(NUMBER_OF_REALS,    sizeof(fmi2Real));
        comp->i = (fmi2Integer *)functions->allocateMemory(NUMBER_OF_INTEGERS, sizeof(fmi2Integer));
        comp->b = (fmi2Boolean *)functions->allocateMemory(NUMBER_OF_BOOLEANS, sizeof(fmi2Boolean));
        comp->s = (fmi2String *) functions->allocateMemory(NUMBER_OF_STRINGS,  sizeof(fmi2String));
        comp->isPositive = (fmi2Boolean *)functions->allocateMemory(NUMBER_OF_EVENT_INDICATORS,
            sizeof(fmi2Boolean));
        comp->instanceName = (char *)functions->allocateMemory(1 + strlen(instanceName), sizeof(char));
        comp->GUID = (char *)functions->allocateMemory(1 + strlen(fmuGUID), sizeof(char));

        // set all categories to on or off. fmi2SetDebugLogging should be called to choose specific categories.
        for (i = 0; i < NUMBER_OF_CATEGORIES; i++) {
            comp->logCategories[i] = loggingOn;
        }
    }
    if (!comp || !comp->r || !comp->i || !comp->b || !comp->s || !comp->isPositive
        || !comp->instanceName || !comp->GUID) {

        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, "error",
            "fmi2Instantiate: Out of memory.");
        return NULL;
    }
    comp->time = 0; // overwrite in fmi2SetupExperiment, fmi2SetTime
    strcpy((char *)comp->instanceName, (char *)instanceName);
    comp->type = fmuType;
    strcpy((char *)comp->GUID, (char *)fmuGUID);
    comp->functions = functions;
    comp->componentEnvironment = functions->componentEnvironment;
    comp->loggingOn = loggingOn;
    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    comp->isDirtyValues = fmi2True; // because we just called setStartValues
    comp->isNewEventIteration = fmi2False;

    comp->eventInfo.newDiscreteStatesNeeded = fmi2False;
    comp->eventInfo.terminateSimulation = fmi2False;
    comp->eventInfo.nominalsOfContinuousStatesChanged = fmi2False;
    comp->eventInfo.valuesOfContinuousStatesChanged = fmi2False;
    comp->eventInfo.nextEventTimeDefined = fmi2False;
    comp->eventInfo.nextEventTime = 0;

    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2Instantiate: GUID=%s", fmuGUID)

    return comp;
}

fmi2Status fmi2SetupExperiment(fmi2Component c, fmi2Boolean toleranceDefined, fmi2Real tolerance,
                            fmi2Real startTime, fmi2Boolean stopTimeDefined, fmi2Real stopTime) {

    // ignore arguments: stopTimeDefined, stopTime
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetupExperiment", MASK_fmi2SetupExperiment))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetupExperiment: toleranceDefined=%d tolerance=%g",
        toleranceDefined, tolerance)

    comp->time = startTime;
    return fmi2OK;
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2EnterInitializationMode", MASK_fmi2EnterInitializationMode))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2EnterInitializationMode")

    comp->state = modelInitializationMode;
    return fmi2OK;
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2ExitInitializationMode", MASK_fmi2ExitInitializationMode))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2ExitInitializationMode")

    // if values were set and no fmi2GetXXX triggered update before,
    // ensure calculated values are updated now
    if (comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi2False;
    }

    if (comp->type == fmi2ModelExchange) {
        comp->state = modelEventMode;
        comp->isNewEventIteration = fmi2True;
    }
    else comp->state = modelStepComplete;
    return fmi2OK;
}

fmi2Status fmi2Terminate(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2Terminate", MASK_fmi2Terminate))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2Terminate")

    comp->state = modelTerminated;
    return fmi2OK;
}

fmi2Status fmi2Reset(fmi2Component c) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2Reset", MASK_fmi2Reset))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2Reset")

    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    comp->isDirtyValues = fmi2True; // because we just called setStartValues
    return fmi2OK;
}

void fmi2FreeInstance(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (!comp) return;
    if (invalidState(comp, "fmi2FreeInstance", MASK_fmi2FreeInstance))
        return;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2FreeInstance")

    if (comp->r) comp->functions->freeMemory(comp->r);
    if (comp->i) comp->functions->freeMemory(comp->i);
    if (comp->b) comp->functions->freeMemory(comp->b);
    if (comp->s) {
        int i;
        for (i = 0; i < NUMBER_OF_STRINGS; i++){
            if (comp->s[i]) comp->functions->freeMemory((void *)comp->s[i]);
        }
        comp->functions->freeMemory((void *)comp->s);
    }
    if (comp->isPositive) comp->functions->freeMemory(comp->isPositive);
    if (comp->instanceName) comp->functions->freeMemory((void *)comp->instanceName);
    if (comp->GUID) comp->functions->freeMemory((void *)comp->GUID);
    comp->functions->freeMemory(comp);
}

// ---------------------------------------------------------------------------
// FMI functions: class methods not depending of a specific model instance
// ---------------------------------------------------------------------------

const char* fmi2GetVersion() {
    return fmi2Version;
}

const char* fmi2GetTypesPlatform() {
    return fmi2TypesPlatform;
}

// ---------------------------------------------------------------------------
// FMI functions: logging control, setters and getters for Real, Integer,
// Boolean, String
// ---------------------------------------------------------------------------

fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn, size_t nCategories, const fmi2String categories[]) {
    int i, j;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetDebugLogging", MASK_fmi2SetDebugLogging))
        return fmi2Error;
    comp->loggingOn = loggingOn;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetDebugLogging")

    // reset all categories
    for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
        comp->logCategories[j] = fmi2False;
    }

    if (nCategories == 0) {
        // no category specified, set all categories to have loggingOn value
        for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
            comp->logCategories[j] = loggingOn;
        }
    } else {
        // set specific categories on
        for (i = 0; i < nCategories; i++) {
            fmi2Boolean categoryFound = fmi2False;
            for (j = 0; j < NUMBER_OF_CATEGORIES; j++) {
                if (strcmp(logCategoriesNames[j], categories[i]) == 0) {
                    comp->logCategories[j] = loggingOn;
                    categoryFound = fmi2True;
                    break;
                }
            }
            if (!categoryFound) {
                comp->functions->logger(comp->componentEnvironment, comp->instanceName, fmi2Warning,
                    logCategoriesNames[LOG_ERROR],
                    "logging category '%s' is not supported by model", categories[i]);
            }
        }
    }
    return fmi2OK;
}

fmi2Status fmi2GetReal (fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetReal", MASK_fmi2GetReal))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetReal", "vr[]", vr))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetReal", "value[]", value))
        return fmi2Error;
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi2False;
    }
#if NUMBER_OF_REALS > 0
    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2GetReal", vr[i], NUMBER_OF_REALS))
            return fmi2Error;
        value[i] = getReal(comp, vr[i]); // to be implemented by the includer of this file

        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetReal: #r%u# = %.16g", vr[i], value[i])
    }
#endif
    return fmi2OK;
}

fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetInteger", MASK_fmi2GetInteger))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetInteger", "vr[]", vr))
            return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetInteger", "value[]", value))
            return fmi2Error;
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi2False;
    }
    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2GetInteger", vr[i], NUMBER_OF_INTEGERS))
            return fmi2Error;
        value[i] = comp->i[vr[i]];
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetInteger: #i%u# = %d", vr[i], value[i])
    }
    return fmi2OK;
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetBoolean", MASK_fmi2GetBoolean))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetBoolean", "vr[]", vr))
            return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2GetBoolean", "value[]", value))
            return fmi2Error;
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi2False;
    }
    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2GetBoolean", vr[i], NUMBER_OF_BOOLEANS))
            return fmi2Error;
        value[i] = comp->b[vr[i]];
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetBoolean: #b%u# = %s", vr[i], value[i]? "true" : "false")
    }
    return fmi2OK;
}

fmi2Status fmi2GetString (fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetString", MASK_fmi2GetString))
        return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2GetString", "vr[]", vr))
            return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2GetString", "value[]", value))
            return fmi2Error;
    if (nvr > 0 && comp->isDirtyValues) {
        calculateValues(comp);
        comp->isDirtyValues = fmi2False;
    }
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmi2GetString", vr[i], NUMBER_OF_STRINGS))
            return fmi2Error;
        value[i] = comp->s[vr[i]];
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetString: #s%u# = '%s'", vr[i], value[i])
    }
    return fmi2OK;
}

fmi2Status fmi2SetReal (fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetReal", MASK_fmi2SetReal))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2SetReal", "vr[]", vr))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2SetReal", "value[]", value))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetReal: nvr = %d", nvr)
    // no check whether setting the value is allowed in the current state
    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2SetReal", vr[i], NUMBER_OF_REALS))
            return fmi2Error;
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetReal: #r%d# = %.16g", vr[i], value[i])
        comp->r[vr[i]] = value[i];
    }
    if (nvr > 0) comp->isDirtyValues = fmi2True;
    return fmi2OK;
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetInteger", MASK_fmi2SetInteger))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2SetInteger", "vr[]", vr))
        return fmi2Error;
    if (nvr > 0 && nullPointer(comp, "fmi2SetInteger", "value[]", value))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetInteger: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2SetInteger", vr[i], NUMBER_OF_INTEGERS))
            return fmi2Error;
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetInteger: #i%d# = %d", vr[i], value[i])
        comp->i[vr[i]] = value[i];
    }
    if (nvr > 0) comp->isDirtyValues = fmi2True;
    return fmi2OK;
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetBoolean", MASK_fmi2SetBoolean))
        return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2SetBoolean", "vr[]", vr))
        return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2SetBoolean", "value[]", value))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetBoolean: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        if (vrOutOfRange(comp, "fmi2SetBoolean", vr[i], NUMBER_OF_BOOLEANS))
            return fmi2Error;
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetBoolean: #b%d# = %s", vr[i], value[i] ? "true" : "false")
        comp->b[vr[i]] = value[i];
    }
    if (nvr > 0) comp->isDirtyValues = fmi2True;
    return fmi2OK;
}

fmi2Status fmi2SetString (fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    int i, n;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetString", MASK_fmi2SetString))
        return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2SetString", "vr[]", vr))
        return fmi2Error;
    if (nvr>0 && nullPointer(comp, "fmi2SetString", "value[]", value))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetString: nvr = %d", nvr)

    for (i = 0; i < nvr; i++) {
        char *string = (char *)comp->s[vr[i]];
        if (vrOutOfRange(comp, "fmi2SetString", vr[i], NUMBER_OF_STRINGS))
            return fmi2Error;
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetString: #s%d# = '%s'", vr[i], value[i])

        if (value[i] == NULL) {
            if (string) comp->functions->freeMemory(string);
            comp->s[vr[i]] = NULL;
            FILTERED_LOG(comp, fmi2Warning, LOG_ERROR, "fmi2SetString: string argument value[%d] = NULL.", i);
        } else {
            if (string == NULL || strlen(string) < strlen(value[i])) {
                if (string) comp->functions->freeMemory(string);
                comp->s[vr[i]] = (char *)comp->functions->allocateMemory(1 + strlen(value[i]), sizeof(char));
                if (!comp->s[vr[i]]) {
                    comp->state = modelError;
                    FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "fmi2SetString: Out of memory.")
                    return fmi2Error;
                }
            }
            strcpy((char *)comp->s[vr[i]], (char *)value[i]);
        }
    }
    if (nvr > 0) comp->isDirtyValues = fmi2True;
    return fmi2OK;
}

fmi2Status fmi2GetFMUstate (fmi2Component c, fmi2FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi2GetFMUstate", MASK_fmi2GetFMUstate);
}
fmi2Status fmi2SetFMUstate (fmi2Component c, fmi2FMUstate FMUstate) {
    return unsupportedFunction(c, "fmi2SetFMUstate", MASK_fmi2SetFMUstate);
}
fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi2FreeFMUstate", MASK_fmi2FreeFMUstate);
}
fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t *size) {
    return unsupportedFunction(c, "fmi2SerializedFMUstateSize", MASK_fmi2SerializedFMUstateSize);
}
fmi2Status fmi2SerializeFMUstate (fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size) {
    return unsupportedFunction(c, "fmi2SerializeFMUstate", MASK_fmi2SerializeFMUstate);
}
fmi2Status fmi2DeSerializeFMUstate (fmi2Component c, const fmi2Byte serializedState[], size_t size,
                                    fmi2FMUstate* FMUstate) {
    return unsupportedFunction(c, "fmi2DeSerializeFMUstate", MASK_fmi2DeSerializeFMUstate);
}

fmi2Status fmi2GetDirectionalDerivative(fmi2Component c, const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
                                        const fmi2ValueReference vKnown_ref[] , size_t nKnown,
                                        const fmi2Real dvKnown[], fmi2Real dvUnknown[]) {
    return unsupportedFunction(c, "fmi2GetDirectionalDerivative", MASK_fmi2GetDirectionalDerivative);
}

// ---------------------------------------------------------------------------
// Functions for FMI for Co-Simulation
// ---------------------------------------------------------------------------
/* Simulating the slave */
fmi2Status fmi2SetRealInputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr,
                                     const fmi2Integer order[], const fmi2Real value[]) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetRealInputDerivatives", MASK_fmi2SetRealInputDerivatives)) {
        return fmi2Error;
    }
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetRealInputDerivatives: nvr= %d", nvr)
    FILTERED_LOG(comp, fmi2Error, LOG_ERROR, "fmi2SetRealInputDerivatives: ignoring function call."
        " This model cannot interpolate inputs: canInterpolateInputs=\"fmi2False\"")
    return fmi2Error;
}

fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr,
                                      const fmi2Integer order[], fmi2Real value[]) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetRealOutputDerivatives", MASK_fmi2GetRealOutputDerivatives))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetRealOutputDerivatives: nvr= %d", nvr)
    FILTERED_LOG(comp, fmi2Error, LOG_ERROR,"fmi2GetRealOutputDerivatives: ignoring function call."
        " This model cannot compute derivatives of outputs: MaxOutputDerivativeOrder=\"0\"")
    for (i = 0; i < nvr; i++) value[i] = 0;
    return fmi2Error;
}

fmi2Status fmi2CancelStep(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2CancelStep", MASK_fmi2CancelStep)) {
        // always fmi2CancelStep is invalid, because model is never in modelStepInProgress state.
        return fmi2Error;
    }
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2CancelStep")
    FILTERED_LOG(comp, fmi2Error, LOG_ERROR,"fmi2CancelStep: Can be called when fmi2DoStep returned fmi2Pending."
        " This is not the case.");
    // comp->state = modelStepCanceled;
    return fmi2Error;
}

fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint,
                    fmi2Real communicationStepSize, fmi2Boolean noSetFMUStatePriorToCurrentPoint) {
    ModelInstance *comp = (ModelInstance *)c;
    double h = communicationStepSize / 10;
    int k,i;
    const int n = 10; // how many Euler steps to perform for one do step
    double prevState[max(NUMBER_OF_STATES, 1)];
    double prevEventIndicators[max(NUMBER_OF_EVENT_INDICATORS, 1)];
    int stateEvent = 0;
    int timeEvent = 0;

    if (invalidState(comp, "fmi2DoStep", MASK_fmi2DoStep))
        return fmi2Error;

    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2DoStep: "
        "currentCommunicationPoint = %g, "
        "communicationStepSize = %g, "
        "noSetFMUStatePriorToCurrentPoint = fmi2%s",
        currentCommunicationPoint, communicationStepSize, noSetFMUStatePriorToCurrentPoint ? "True" : "False")

    if (communicationStepSize <= 0) {
        FILTERED_LOG(comp, fmi2Error, LOG_ERROR,
            "fmi2DoStep: communication step size must be > 0. Fount %g.", communicationStepSize)
        comp->state = modelError;
        return fmi2Error;
    }

#if NUMBER_OF_EVENT_INDICATORS>0
    // initialize previous event indicators with current values
    for (i = 0; i < NUMBER_OF_EVENT_INDICATORS; i++) {
        prevEventIndicators[i] = getEventIndicator(comp, i);
    }
#endif

    // break the step into n steps and do forward Euler.
    comp->time = currentCommunicationPoint;
    for (k = 0; k < n; k++) {
        comp->time += h;

#if NUMBER_OF_STATES>0
        for (i = 0; i < NUMBER_OF_STATES; i++) {
            prevState[i] = r(vrStates[i]);
        }
        for (i = 0; i < NUMBER_OF_STATES; i++) {
            fmi2ValueReference vr = vrStates[i];
            r(vr) += h * getReal(comp, vr + 1); // forward Euler step
        }
#endif

#if NUMBER_OF_EVENT_INDICATORS>0
        // check for state event
        for (i = 0; i < NUMBER_OF_EVENT_INDICATORS; i++) {
            double ei = getEventIndicator(comp, i);
            if (ei * prevEventIndicators[i] < 0) {
                FILTERED_LOG(comp, fmi2OK, LOG_EVENT,
                    "fmi2DoStep: state event at %g, z%d crosses zero -%c-", comp->time, i, ei < 0 ? '\\' : '/')
                stateEvent++;
            }
            prevEventIndicators[i] = ei;
        }
#endif
        // check for time event
        if (comp->eventInfo.nextEventTimeDefined && (comp->time - comp->eventInfo.nextEventTime > -DT_EVENT_DETECT)) {
            FILTERED_LOG(comp, fmi2OK, LOG_EVENT, "fmi2DoStep: time event detected at %g", comp->time)
            timeEvent = 1;
        }

        if (stateEvent || timeEvent) {
            eventUpdate(comp, &comp->eventInfo, timeEvent, fmi2True);
            timeEvent = 0;
            stateEvent = 0;
        }

        // terminate simulation, if requested by the model in the previous step
        if (comp->eventInfo.terminateSimulation) {
            FILTERED_LOG(comp, fmi2Discard, LOG_ALL, "fmi2DoStep: model requested termination at t=%g", comp->time)
            comp->state = modelStepFailed;
            return fmi2Discard; // enforce termination of the simulation loop
        }
    }
    return fmi2OK;
}

/* Inquire slave status */
static fmi2Status getStatus(char* fname, fmi2Component c, const fmi2StatusKind s) {
    const char *statusKind[3] = {"fmi2DoStepStatus","fmi2PendingStatus","fmi2LastSuccessfulTime"};
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, fname, MASK_fmi2GetStatus)) // all get status have the same MASK_fmi2GetStatus
            return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "$s: fmi2StatusKind = %s", fname, statusKind[s])

    switch(s) {
        case fmi2DoStepStatus: FILTERED_LOG(comp, fmi2Error, LOG_ERROR,
            "%s: Can be called with fmi2DoStepStatus when fmi2DoStep returned fmi2Pending."
            " This is not the case.", fname)
            break;
        case fmi2PendingStatus: FILTERED_LOG(comp, fmi2Error, LOG_ERROR,
            "%s: Can be called with fmi2PendingStatus when fmi2DoStep returned fmi2Pending."
            " This is not the case.", fname)
            break;
        case fmi2LastSuccessfulTime: FILTERED_LOG(comp, fmi2Error, LOG_ERROR,
            "%s: Can be called with fmi2LastSuccessfulTime when fmi2DoStep returned fmi2Discard."
            " This is not the case.", fname)
            break;
        case fmi2Terminated: FILTERED_LOG(comp, fmi2Error, LOG_ERROR,
            "%s: Can be called with fmi2Terminated when fmi2DoStep returned fmi2Discard."
            " This is not the case.", fname)
            break;
    }
    return fmi2Discard;
}

fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status *value) {
    return getStatus("fmi2GetStatus", c, s);
}

fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real *value) {
    if (s == fmi2LastSuccessfulTime) {
        ModelInstance *comp = (ModelInstance *)c;
        if (invalidState(comp, "fmi2GetRealStatus", MASK_fmi2GetRealStatus))
            return fmi2Error;
        *value = comp->time;
        return fmi2OK;
    }
    return getStatus("fmi2GetRealStatus", c, s);
}

fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer *value) {
    return getStatus("fmi2GetIntegerStatus", c, s);
}

fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean *value) {
    if (s == fmi2Terminated) {
        ModelInstance *comp = (ModelInstance *)c;
        if (invalidState(comp, "fmi2GetBooleanStatus", MASK_fmi2GetBooleanStatus))
            return fmi2Error;
        *value = comp->eventInfo.terminateSimulation;
        return fmi2OK;
    }
    return getStatus("fmi2GetBooleanStatus", c, s);
}

fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String *value) {
    return getStatus("fmi2GetStringStatus", c, s);
}

// ---------------------------------------------------------------------------
// Functions for FMI2 for Model Exchange
// ---------------------------------------------------------------------------
/* Enter and exit the different modes */
fmi2Status fmi2EnterEventMode(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2EnterEventMode", MASK_fmi2EnterEventMode))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2EnterEventMode")

    comp->state = modelEventMode;
    comp->isNewEventIteration = fmi2True;
    return fmi2OK;
}

fmi2Status fmi2NewDiscreteStates(fmi2Component c, fmi2EventInfo *eventInfo) {
    ModelInstance *comp = (ModelInstance *)c;
    int timeEvent = 0;
    if (invalidState(comp, "fmi2NewDiscreteStates", MASK_fmi2NewDiscreteStates))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2NewDiscreteStates")

    comp->eventInfo.newDiscreteStatesNeeded = fmi2False;
    comp->eventInfo.terminateSimulation = fmi2False;
    comp->eventInfo.nominalsOfContinuousStatesChanged = fmi2False;
    comp->eventInfo.valuesOfContinuousStatesChanged = fmi2False;

    if (comp->eventInfo.nextEventTimeDefined && comp->eventInfo.nextEventTime <= comp->time) {
        timeEvent = 1;
    }
    eventUpdate(comp, &comp->eventInfo, timeEvent, comp->isNewEventIteration);
    comp->isNewEventIteration = fmi2False;

    // copy internal eventInfo of component to output eventInfo
    eventInfo->newDiscreteStatesNeeded = comp->eventInfo.newDiscreteStatesNeeded;
    eventInfo->terminateSimulation = comp->eventInfo.terminateSimulation;
    eventInfo->nominalsOfContinuousStatesChanged = comp->eventInfo.nominalsOfContinuousStatesChanged;
    eventInfo->valuesOfContinuousStatesChanged = comp->eventInfo.valuesOfContinuousStatesChanged;
    eventInfo->nextEventTimeDefined = comp->eventInfo.nextEventTimeDefined;
    eventInfo->nextEventTime = comp->eventInfo.nextEventTime;

    return fmi2OK;
}

fmi2Status fmi2EnterContinuousTimeMode(fmi2Component c) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2EnterContinuousTimeMode", MASK_fmi2EnterContinuousTimeMode))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL,"fmi2EnterContinuousTimeMode")

    comp->state = modelContinuousTimeMode;
    return fmi2OK;
}

fmi2Status fmi2CompletedIntegratorStep(fmi2Component c, fmi2Boolean noSetFMUStatePriorToCurrentPoint,
                                     fmi2Boolean *enterEventMode, fmi2Boolean *terminateSimulation) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2CompletedIntegratorStep", MASK_fmi2CompletedIntegratorStep))
        return fmi2Error;
    if (nullPointer(comp, "fmi2CompletedIntegratorStep", "enterEventMode", enterEventMode))
        return fmi2Error;
    if (nullPointer(comp, "fmi2CompletedIntegratorStep", "terminateSimulation", terminateSimulation))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL,"fmi2CompletedIntegratorStep")
    *enterEventMode = fmi2False;
    *terminateSimulation = fmi2False;
    return fmi2OK;
}

/* Providing independent variables and re-initialization of caching */
fmi2Status fmi2SetTime(fmi2Component c, fmi2Real time) {
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2SetTime", MASK_fmi2SetTime))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetTime: time=%.16g", time)
    comp->time = time;
    return fmi2OK;
}

fmi2Status fmi2SetContinuousStates(fmi2Component c, const fmi2Real x[], size_t nx){
    ModelInstance *comp = (ModelInstance *)c;
    int i;
    if (invalidState(comp, "fmi2SetContinuousStates", MASK_fmi2SetContinuousStates))
        return fmi2Error;
    if (invalidNumber(comp, "fmi2SetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi2Error;
    if (nullPointer(comp, "fmi2SetContinuousStates", "x[]", x))
        return fmi2Error;
#if NUMBER_OF_STATES>0
    for (i = 0; i < nx; i++) {
        fmi2ValueReference vr = vrStates[i];
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2SetContinuousStates: #r%d#=%.16g", vr, x[i])
        assert(vr < NUMBER_OF_REALS);
        comp->r[vr] = x[i];
    }
#endif
    return fmi2OK;
}

/* Evaluation of the model equations */
fmi2Status fmi2GetDerivatives(fmi2Component c, fmi2Real derivatives[], size_t nx) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetDerivatives", MASK_fmi2GetDerivatives))
        return fmi2Error;
    if (invalidNumber(comp, "fmi2GetDerivatives", "nx", nx, NUMBER_OF_STATES))
        return fmi2Error;
    if (nullPointer(comp, "fmi2GetDerivatives", "derivatives[]", derivatives))
        return fmi2Error;
#if NUMBER_OF_STATES>0
    for (i = 0; i < nx; i++) {
        fmi2ValueReference vr = vrStates[i] + 1;
        derivatives[i] = getReal(comp, vr); // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetDerivatives: #r%d# = %.16g", vr, derivatives[i])
    }
#endif
    return fmi2OK;
}

fmi2Status fmi2GetEventIndicators(fmi2Component c, fmi2Real eventIndicators[], size_t ni) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetEventIndicators", MASK_fmi2GetEventIndicators))
        return fmi2Error;
    if (invalidNumber(comp, "fmi2GetEventIndicators", "ni", ni, NUMBER_OF_EVENT_INDICATORS))
        return fmi2Error;
#if NUMBER_OF_EVENT_INDICATORS>0
    for (i = 0; i < ni; i++) {
        eventIndicators[i] = getEventIndicator(comp, i); // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetEventIndicators: z%d = %.16g", i, eventIndicators[i])
    }
#endif
    return fmi2OK;
}

fmi2Status fmi2GetContinuousStates(fmi2Component c, fmi2Real states[], size_t nx) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetContinuousStates", MASK_fmi2GetContinuousStates))
        return fmi2Error;
    if (invalidNumber(comp, "fmi2GetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi2Error;
    if (nullPointer(comp, "fmi2GetContinuousStates", "states[]", states))
        return fmi2Error;
#if NUMBER_OF_STATES>0
    for (i = 0; i < nx; i++) {
        fmi2ValueReference vr = vrStates[i];
        states[i] = getReal(comp, vr); // to be implemented by the includer of this file
        FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetContinuousStates: #r%u# = %.16g", vr, states[i])
    }
#endif
    return fmi2OK;
}

fmi2Status fmi2GetNominalsOfContinuousStates(fmi2Component c, fmi2Real x_nominal[], size_t nx) {
    int i;
    ModelInstance *comp = (ModelInstance *)c;
    if (invalidState(comp, "fmi2GetNominalsOfContinuousStates", MASK_fmi2GetNominalsOfContinuousStates))
        return fmi2Error;
    if (invalidNumber(comp, "fmi2GetNominalContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmi2Error;
    if (nullPointer(comp, "fmi2GetNominalContinuousStates", "x_nominal[]", x_nominal))
        return fmi2Error;
    FILTERED_LOG(comp, fmi2OK, LOG_FMI_CALL, "fmi2GetNominalContinuousStates: x_nominal[0..%d] = 1.0", nx-1)
    for (i = 0; i < nx; i++)
        x_nominal[i] = 1;
    return fmi2OK;
}

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
