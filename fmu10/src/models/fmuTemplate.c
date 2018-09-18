/* ---------------------------------------------------------------------------*
 * Implementation of the FMI interface based on functions and macros to
 * be defined by the includer of this file. 
 * If FMI_COSIMULATION is defined, this implements "FMI for Co-Simulation 1.0",
 * otherwise "FMI for Model Exchange 1.0".
 * The "FMI for Co-Simulation 1.0", implementation assumes that exactly the 
 * following capability flags are set to fmiTrue:
 *    canHandleVariableCommunicationStepSize, i.e. fmiDoStep step size can vary
 *    canHandleEvents, i.e. fmiDoStep step size can be zero
 * and all other capability flags are set to default, i.e. to fmiFalse or 0.
 *
 * Revision history
 *  07.02.2010 initial version for "Model Exchange 1.0" released in FMU SDK 1.0
 *  05.03.2010 bug fix: fmiSetString now copies the passed string argument
 *     and fmiFreeModelInstance frees all string copies
 *  11.12.2010 replaced calloc by functions.allocateMemory in fmiInstantiateModel
 *  04.08.2011 added support for "FMI for Co-Simulation 1.0"
 *  02.08.2013 fixed a bug in instantiateModel reported by Markus Ende, TH Nuernberg
 *  02.04.2014 better time event handling
 *  02.06.2014 copy instanceName and GUID at instantiation
 *
 * Copyright QTronic GmbH. All rights reserved.
 * ---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

// array of value references of states
#if NUMBER_OF_STATES>0
fmiValueReference vrStates[NUMBER_OF_STATES] = STATES; 
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

static fmiBoolean invalidNumber(ModelInstance* comp, const char* f, const char* arg, int n, int nExpected){
    if (n != nExpected) {
        comp->state = modelError;
        comp->functions.logger(comp, comp->instanceName, fmiError, "error", 
                "%s: Invalid argument %s = %d. Expected %d.", f, arg, n, nExpected);
        return fmiTrue;
    }
    return fmiFalse;
}

static fmiBoolean invalidState(ModelInstance* comp, const char* f, int statesExpected){
    if (!comp) 
        return fmiTrue;
    if (!(comp->state & statesExpected)) {
        comp->state = modelError;
        comp->functions.logger(comp, comp->instanceName, fmiError, "error",
                "%s: Illegal call sequence.", f);
        return fmiTrue;
    }
    return fmiFalse;
}

static fmiBoolean nullPointer(ModelInstance* comp, const char* f, const char* arg, const void* p){
    if (!p) {
        comp->state = modelError;
        comp->functions.logger(comp, comp->instanceName, fmiError, "error",
                "%s: Invalid argument %s = NULL.", f, arg);
        return fmiTrue;
    }
    return fmiFalse;
}

static fmiBoolean vrOutOfRange(ModelInstance* comp, const char* f, fmiValueReference vr, int end) {
    if (vr >= end) {
        comp->functions.logger(comp, comp->instanceName, fmiError, "error",
                "%s: Illegal value reference %u.", f, vr);
        comp->state = modelError;
        return fmiTrue;
    }
    return fmiFalse;
}  

// ---------------------------------------------------------------------------
// Private helpers used below to implement functions
// ---------------------------------------------------------------------------

fmiStatus setString(fmiComponent comp, fmiValueReference vr, fmiString value){
    return fmiSetString(comp, &vr, 1, &value);
}

// fname is fmiInstantiateModel or fmiInstantiateSlave
static fmiComponent instantiateModel(char* fname, fmiString instanceName, fmiString GUID,
        fmiCallbackFunctions functions, fmiBoolean loggingOn) {
    ModelInstance* comp;
    if (!functions.logger) 
        return NULL; // we cannot even log this problem
    if (!instanceName || strlen(instanceName)==0) { 
        functions.logger(NULL, "?", fmiError, "error",
                "%s: Missing instance name.", fname);
        return NULL;
    }
    if (!GUID || strlen(GUID)==0) {
        functions.logger(NULL, instanceName, fmiError, "error",
                "%s: Missing GUID.", fname);
        return NULL;
    }
    if (!functions.allocateMemory || !functions.freeMemory){
        functions.logger(NULL, instanceName, fmiError, "error",
                "%s: Missing callback function.", fname);
        return NULL;
    }
    if (strcmp(GUID, MODEL_GUID)) {
        functions.logger(NULL, instanceName, fmiError, "error",
                "%s: Wrong GUID %s. Expected %s.", fname, GUID, MODEL_GUID);
        return NULL;
    }
    comp = (ModelInstance *)functions.allocateMemory(1, sizeof(ModelInstance));
    if (comp) {
        comp->r = (fmiReal *)functions.allocateMemory(NUMBER_OF_REALS,    sizeof(fmiReal));
        comp->i = (fmiInteger *)functions.allocateMemory(NUMBER_OF_INTEGERS, sizeof(fmiInteger));
        comp->b = (fmiBoolean *)functions.allocateMemory(NUMBER_OF_BOOLEANS, sizeof(fmiBoolean));
        comp->s = (fmiString *)functions.allocateMemory(NUMBER_OF_STRINGS,  sizeof(fmiString));
        comp->isPositive = (fmiBoolean *)functions.allocateMemory(NUMBER_OF_EVENT_INDICATORS, sizeof(fmiBoolean));
        comp->instanceName = (char *)functions.allocateMemory(1 + strlen(instanceName), sizeof(char));
        comp->GUID = (char *)functions.allocateMemory(1 + strlen(GUID), sizeof(char));
    }
    if (!comp || !comp->r || !comp->i || !comp->b || !comp->s || !comp->isPositive
        || !comp->instanceName || !comp->GUID) {
        functions.logger(NULL, instanceName, fmiError, "error",
                "%s: Out of memory.", fname);
        return NULL;
    }
    if (loggingOn) functions.logger(NULL, instanceName, fmiOK, "log",
            "%s: GUID=%s", fname, GUID);

    strcpy((char *)comp->instanceName, (char *)instanceName);
    strcpy((char *)comp->GUID, (char *)GUID);
    comp->functions = functions;
    comp->loggingOn = loggingOn;
    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    return comp;
}

// fname is fmiInitialize or fmiInitializeSlave
static fmiStatus init(char* fname, fmiComponent c, fmiBoolean toleranceControlled, fmiReal relativeTolerance,
    fmiEventInfo* eventInfo) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, fname, modelInstantiated))
         return fmiError;
    if (nullPointer(comp, fname, "eventInfo", eventInfo))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", 
        "%s: toleranceControlled=%d relativeTolerance=%g", 
        fname, toleranceControlled, relativeTolerance);
    eventInfo->iterationConverged  = fmiTrue;
    eventInfo->stateValueReferencesChanged = fmiFalse;
    eventInfo->stateValuesChanged  = fmiFalse;
    eventInfo->terminateSimulation = fmiFalse;
    eventInfo->upcomingTimeEvent   = fmiFalse;
    initialize(comp, eventInfo); // to be implemented by the includer of this file
    comp->state = modelInitialized;
    return fmiOK;
}

// fname is fmiTerminate or fmiTerminateSlave
static fmiStatus terminate(char* fname, fmiComponent c){
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, fname, modelInitialized))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", fname);
    comp->state = modelTerminated;
    return fmiOK;
}

// fname is freeModelInstance of freeSlaveInstance
void freeInstance(char* fname, fmiComponent c) {
    ModelInstance* comp = (ModelInstance *)c;
    if (!comp) return;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", fname);
    if (comp->r) comp->functions.freeMemory(comp->r);
    if (comp->i) comp->functions.freeMemory(comp->i);
    if (comp->b) comp->functions.freeMemory(comp->b);
    if (comp->s) {
        int i;
        for (i=0; i<NUMBER_OF_STRINGS; i++){
            if (comp->s[i]) comp->functions.freeMemory((void *)comp->s[i]);
        }
        comp->functions.freeMemory((void *)comp->s);
    }
    if (comp->isPositive) comp->functions.freeMemory(comp->isPositive);
    if (comp->instanceName) comp->functions.freeMemory((void *)comp->instanceName);
    if (comp->GUID) comp->functions.freeMemory((void *)comp->GUID);
    comp->functions.freeMemory(comp);
}

// ---------------------------------------------------------------------------
// FMI functions: class methods not depending of a specific model instance
// ---------------------------------------------------------------------------

const char* fmiGetVersion() {
    return fmiVersion;
}

// ---------------------------------------------------------------------------
// FMI functions: for FMI Model Exchange 1.0 and for FMI Co-Simulation 1.0
// logging control, setters and getters for Real, Integer, Boolean, String
// ---------------------------------------------------------------------------

fmiStatus fmiSetDebugLogging(fmiComponent c, fmiBoolean loggingOn) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetDebugLogging", not_modelError))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetDebugLogging: loggingOn=%d", loggingOn);
    comp->loggingOn = loggingOn;
    return fmiOK;
}

fmiStatus fmiSetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiReal value[]){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetReal", modelInstantiated|modelInitialized))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetReal", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetReal", "value[]", value))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetReal: nvr = %d", nvr);
    // no check whether setting the value is allowed in the current state
    for (i=0; i<nvr; i++) {
       if (vrOutOfRange(comp, "fmiSetReal", vr[i], NUMBER_OF_REALS))
           return fmiError;
       if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetReal: #r%d# = %.16g", vr[i], value[i]);
       comp->r[vr[i]] = value[i];
    }
    return fmiOK;
}

fmiStatus fmiSetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiInteger value[]){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetInteger", modelInstantiated|modelInitialized))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetInteger", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetInteger", "value[]", value))
         return fmiError;
    if (comp->loggingOn)
        comp->functions.logger(c, comp->instanceName, fmiOK, "log", "fmiSetInteger: nvr = %d",  nvr);
    for (i=0; i<nvr; i++) {
       if (vrOutOfRange(comp, "fmiSetInteger", vr[i], NUMBER_OF_INTEGERS))
           return fmiError;
       if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetInteger: #i%d# = %d", vr[i], value[i]);
        comp->i[vr[i]] = value[i]; 
    }
    return fmiOK;
}

fmiStatus fmiSetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiBoolean value[]){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetBoolean", modelInstantiated|modelInitialized))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetBoolean", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetBoolean", "value[]", value))
         return fmiError;
    if (comp->loggingOn)
        comp->functions.logger(c, comp->instanceName, fmiOK, "log", "fmiSetBoolean: nvr = %d",  nvr);
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmiSetBoolean", vr[i], NUMBER_OF_BOOLEANS))
            return fmiError;
       if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetBoolean: #b%d# = %s", vr[i], value[i] ? "true" : "false");
        comp->b[vr[i]] = value[i]; 
    }
    return fmiOK;
}

fmiStatus fmiSetString(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiString value[]){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetString", modelInstantiated|modelInitialized))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetString", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiSetString", "value[]", value))
         return fmiError;
    if (comp->loggingOn)
        comp->functions.logger(c, comp->instanceName, fmiOK, "log", "fmiSetString: nvr = %d",  nvr);
    for (i=0; i<nvr; i++) {
        char *string = (char *)comp->s[vr[i]];
        if (vrOutOfRange(comp, "fmiSetString", vr[i], NUMBER_OF_STRINGS))
            return fmiError;
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetString: #s%d# = '%s'", vr[i], value[i]);
        if (value[i] == NULL) {
            if (string) comp->functions.freeMemory(string);
            comp->s[vr[i]] = NULL;
            comp->functions.logger(comp, comp->instanceName, fmiWarning, "warning",
                            "fmiSetString: string argument value[%d] = NULL.", i);
        } else {
            if (string==NULL || strlen(string) < strlen(value[i])) {
                if (string) comp->functions.freeMemory(string);
                comp->s[vr[i]] = (char *)comp->functions.allocateMemory(1+strlen(value[i]), sizeof(char));
                if (!comp->s[vr[i]]) {
                    comp->state = modelError;
                    comp->functions.logger(NULL, comp->instanceName, fmiError, "error", "fmiSetString: Out of memory.");
                    return fmiError;
                }
            }
            strcpy((char *)comp->s[vr[i]], (char *)value[i]);
        }
    }
    return fmiOK;
}

fmiStatus fmiGetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiReal value[]) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetReal", not_modelError))
        return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetReal", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetReal", "value[]", value))
         return fmiError;
#if NUMBER_OF_REALS>0
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmiGetReal", vr[i], NUMBER_OF_REALS))
            return fmiError;
        value[i] = getReal(comp, vr[i]); // to be implemented by the includer of this file
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
                "fmiGetReal: #r%u# = %.16g", vr[i], value[i]);
    }
#endif
    return fmiOK;
}

fmiStatus fmiGetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiInteger value[]) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetInteger", not_modelError))
        return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetInteger", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetInteger", "value[]", value))
         return fmiError;
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmiGetInteger", vr[i], NUMBER_OF_INTEGERS))
           return fmiError;
        value[i] = comp->i[vr[i]];
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
                "fmiGetInteger: #i%u# = %d", vr[i], value[i]);
    }
    return fmiOK;
}

fmiStatus fmiGetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiBoolean value[]) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetBoolean", not_modelError))
        return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetBoolean", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetBoolean", "value[]", value))
         return fmiError;
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmiGetBoolean", vr[i], NUMBER_OF_BOOLEANS))
           return fmiError;
        value[i] = comp->b[vr[i]];
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
                "fmiGetBoolean: #b%u# = %s", vr[i], value[i]? "true" : "false");
    }
    return fmiOK;
}

fmiStatus fmiGetString(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiString  value[]) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetString", not_modelError))
        return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetString", "vr[]", vr))
         return fmiError;
    if (nvr>0 && nullPointer(comp, "fmiGetString", "value[]", value))
         return fmiError;
    for (i=0; i<nvr; i++) {
        if (vrOutOfRange(comp, "fmiGetString", vr[i], NUMBER_OF_STRINGS))
           return fmiError;
        value[i] = comp->s[vr[i]];
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
                "fmiGetString: #s%u# = '%s'", vr[i], value[i]);
    }
    return fmiOK;
}

#ifdef FMI_COSIMULATION
// ---------------------------------------------------------------------------
// FMI functions: only for FMI Co-Simulation 1.0
// ---------------------------------------------------------------------------

const char* fmiGetTypesPlatform() {
    return fmiPlatform;
}

fmiComponent fmiInstantiateSlave(fmiString  instanceName, fmiString GUID,
    fmiString fmuLocation, fmiString mimeType, fmiReal timeout, fmiBoolean visible,
    fmiBoolean interactive, fmiCallbackFunctions functions, fmiBoolean loggingOn) {
    // ignoring arguments: fmuLocation, mimeType, timeout, visible, interactive
    return instantiateModel("fmiInstantiateSlave", instanceName, GUID, functions, loggingOn);
}

fmiStatus fmiInitializeSlave(fmiComponent c, fmiReal tStart, fmiBoolean StopTimeDefined, fmiReal tStop) {
    ModelInstance* comp = (ModelInstance *)c;
    fmiBoolean toleranceControlled = fmiFalse;
    fmiReal relativeTolerance = 0;
    fmiStatus flag = fmiOK;
    comp->eventInfo.iterationConverged = 0;
    while (flag==fmiOK && !comp->eventInfo.iterationConverged) {
        // ignoring arguments: tStart, StopTimeDefined, tStop
        flag = init("fmiInitializeSlave", c, toleranceControlled, relativeTolerance, &comp->eventInfo);
    }
    return flag;
}

fmiStatus fmiTerminateSlave(fmiComponent c) {
    return terminate("fmiTerminateSlave", c);
}

fmiStatus fmiResetSlave(fmiComponent c) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiResetSlave", modelInitialized))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", "fmiResetSlave");
    comp->state = modelInstantiated;
    setStartValues(comp); // to be implemented by the includer of this file
    return fmiOK;
}

void fmiFreeSlaveInstance(fmiComponent c) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiFreeSlaveInstance", modelTerminated))
         return;
    freeInstance("fmiFreeSlaveInstance", c);
}

fmiStatus fmiSetRealInputDerivatives(fmiComponent c, const fmiValueReference vr[], size_t nvr,
    const fmiInteger order[], const fmiReal value[]) {
    ModelInstance* comp = (ModelInstance *)c;
    fmiCallbackLogger log = comp->functions.logger;
    if (invalidState(comp, "fmiSetRealInputDerivatives", modelInitialized))
         return fmiError;
    if (comp->loggingOn) log(c, comp->instanceName, fmiOK, "log", "fmiSetRealInputDerivatives: nvr= %d", nvr);
    log(c, comp->instanceName, fmiError, "warning", "fmiSetRealInputDerivatives: ignoring function call."
      " This model cannot interpolate inputs: canInterpolateInputs=\"fmiFalse\"");
    return fmiWarning;
}

fmiStatus fmiGetRealOutputDerivatives(fmiComponent c, const fmiValueReference vr[], size_t nvr,
    const fmiInteger order[], fmiReal value[]) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    fmiCallbackLogger log = comp->functions.logger;
    if (invalidState(comp, "fmiGetRealOutputDerivatives", modelInitialized))
         return fmiError;
    if (comp->loggingOn) log(c, comp->instanceName, fmiOK, "log", "fmiGetRealOutputDerivatives: nvr= %d", nvr);
    log(c, comp->instanceName, fmiError, "warning", "fmiGetRealOutputDerivatives: ignoring function call."
      " This model cannot compute derivatives of outputs: MaxOutputDerivativeOrder=\"0\"");
    for (i=0; i<nvr; i++) value[i] = 0;
    return fmiWarning;
}

fmiStatus fmiCancelStep(fmiComponent c) {
    ModelInstance* comp = (ModelInstance *)c;
    fmiCallbackLogger log = comp->functions.logger;
    if (invalidState(comp, "fmiCancelStep", modelInitialized))
         return fmiError;
    if (comp->loggingOn) log(c, comp->instanceName, fmiOK, "log", "fmiCancelStep");
    log(c, comp->instanceName, fmiError, "error", 
        "fmiCancelStep: Can be called when fmiDoStep returned fmiPending."
        " This is not the case."); 
    return fmiError;
}

fmiStatus fmiDoStep(fmiComponent c, fmiReal currentCommunicationPoint,
    fmiReal communicationStepSize, fmiBoolean newStep) {
    ModelInstance* comp = (ModelInstance *)c;
    fmiCallbackLogger log = comp->functions.logger;
    double h = communicationStepSize / 10;
    int k,i;
    const int n = 10; // how many Euler steps to perform for one do step
    double prevState[max(NUMBER_OF_STATES, 1)];
    double prevEventIndicators[max(NUMBER_OF_EVENT_INDICATORS, 1)];
    int stateEvent = 0;

    if (invalidState(comp, "fmiDoStep", modelInitialized))
         return fmiError;

    if (comp->loggingOn) log(c, comp->instanceName, fmiOK, "log", "fmiDoStep: "
       "currentCommunicationPoint = %g, " 
       "communicationStepSize = %g, " 
       "newStep = fmi%s",
       currentCommunicationPoint, communicationStepSize, newStep ? "True" : "False");
    
    // Treat also case of zero step, i.e. during an event iteration
    if (communicationStepSize == 0) {
        return fmiOK;
    }

#if NUMBER_OF_EVENT_INDICATORS>0
    // initialize previous event indicators with current values
    for (i=0; i<NUMBER_OF_EVENT_INDICATORS; i++) {
        prevEventIndicators[i] = getEventIndicator(comp, i);
    }
#endif

    // break the step into n steps and do forward Euler.
    comp->time = currentCommunicationPoint;
    for (k=0; k<n; k++) {
        comp->time += h;

#if NUMBER_OF_STATES>0
        for (i=0; i<NUMBER_OF_STATES; i++) {
            prevState[i] = r(vrStates[i]);
        }
        for (i=0; i<NUMBER_OF_STATES; i++) {
            fmiValueReference vr = vrStates[i];
            r(vr) += h * getReal(comp, vr+1); // forward Euler step
        }
#endif

#if NUMBER_OF_EVENT_INDICATORS>0
        // check for state event
        for (i=0; i<NUMBER_OF_EVENT_INDICATORS; i++) {
            double ei = getEventIndicator(comp, i);
            if (ei * prevEventIndicators[i] < 0) {
                if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", 
                    "fmiDoStep: state event at %g, z%d crosses zero -%c-", comp->time, i, ei<0 ? '\\' : '/');
                stateEvent++;
            }
            prevEventIndicators[i] = ei;
        }
        if (stateEvent) {
            eventUpdate(comp, &comp->eventInfo);
            stateEvent = 0;
        } 
#endif
        // check for time event
        if (comp->eventInfo.upcomingTimeEvent && (comp->time - comp->eventInfo.nextEventTime > -DT_EVENT_DETECT)) {
            if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
                "fmiDoStep: time event detected at %g", comp->time);
            eventUpdate(comp, &comp->eventInfo);
        }

        // terminate simulation, if requested by the model
        if (comp->eventInfo.terminateSimulation) {
            comp->functions.logger(c, comp->instanceName, fmiOK, "log",
              "fmiDoStep: model requested termination at t=%g", comp->time);
            return fmiError; // enforce termination of the simulation loop
        }
    }
    return fmiOK;
}

static fmiStatus getStatus(char* fname, fmiComponent c, const fmiStatusKind s) {
    const char* statusKind[3] = {"fmiDoStepStatus","fmiPendingStatus","fmiLastSuccessfulTime"};
    ModelInstance* comp = (ModelInstance *)c;
    fmiCallbackLogger log = comp->functions.logger;
    if (invalidState(comp, fname, modelInstantiated|modelInitialized))
         return fmiError;
    if (comp->loggingOn) log(c, comp->instanceName, fmiOK, "log", "$s: fmiStatusKind = %s", fname, statusKind[s]);
    switch(s) {
        case fmiDoStepStatus:  log(c, comp->instanceName, fmiError, "error",
           "%s: Can be called with fmiDoStepStatus when fmiDoStep returned fmiPending."
           " This is not the case.", fname);
           break;
        case fmiPendingStatus:  log(c, comp->instanceName, fmiError, "error", 
           "%s: Can be called with fmiPendingStatus when fmiDoStep returned fmiPending."
           " This is not the case.", fname); 
           break;
        case fmiLastSuccessfulTime:  log(c, comp->instanceName, fmiError, "error",
           "%s: Can be called with fmiLastSuccessfulTime when fmiDoStep returned fmiDiscard."
           " This is not the case.", fname); 
           break;
    }
    return fmiError;
}

fmiStatus fmiGetStatus(fmiComponent c, const fmiStatusKind s, fmiStatus* value) {
    return getStatus("fmiGetStatus", c, s);
}

fmiStatus fmiGetRealStatus(fmiComponent c, const fmiStatusKind s, fmiReal* value){
    return getStatus("fmiGetRealStatus", c, s);
}

fmiStatus fmiGetIntegerStatus(fmiComponent c, const fmiStatusKind s, fmiInteger* value){
    return getStatus("fmiGetIntegerStatus", c, s);
}

fmiStatus fmiGetBooleanStatus(fmiComponent c, const fmiStatusKind s, fmiBoolean* value){
    return getStatus("fmiGetBooleanStatus", c, s);
}

fmiStatus fmiGetStringStatus(fmiComponent c, const fmiStatusKind s, fmiString*  value){
    return getStatus("fmiGetStringStatus", c, s);
}

#else
// ---------------------------------------------------------------------------
// FMI functions: only for Model Exchange 1.0
// ---------------------------------------------------------------------------

const char* fmiGetModelTypesPlatform() {
    return fmiModelTypesPlatform;
}

fmiComponent fmiInstantiateModel(fmiString instanceName, fmiString GUID, 
        fmiCallbackFunctions functions, fmiBoolean loggingOn) {
    return instantiateModel("fmiInstantiateModel", instanceName, GUID, functions, loggingOn);
}

fmiStatus fmiInitialize(fmiComponent c, fmiBoolean toleranceControlled, fmiReal relativeTolerance,
    fmiEventInfo* eventInfo) {
    return init("fmiInitialize", c, toleranceControlled, relativeTolerance, eventInfo);
}

fmiStatus fmiSetTime(fmiComponent c, fmiReal time) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiSetTime", modelInstantiated|modelInitialized))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetTime: time=%.16g", time);
    comp->time = time;
    return fmiOK;
}

fmiStatus fmiSetContinuousStates(fmiComponent c, const fmiReal x[], size_t nx){
    ModelInstance* comp = (ModelInstance *)c;
    int i;
    if (invalidState(comp, "fmiSetContinuousStates", modelInitialized))
         return fmiError;
    if (invalidNumber(comp, "fmiSetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmiError;
    if (nullPointer(comp, "fmiSetContinuousStates", "x[]", x))
         return fmiError;
#if NUMBER_OF_STATES>0
    for (i=0; i<nx; i++) {
        fmiValueReference vr = vrStates[i];
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiSetContinuousStates: #r%d#=%.16g", vr, x[i]);
        assert(vr<NUMBER_OF_REALS);
        comp->r[vr] = x[i];
    }
#endif
    return fmiOK;
}

fmiStatus fmiEventUpdate(fmiComponent c, fmiBoolean intermediateResults, fmiEventInfo* eventInfo) {
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiEventUpdate", modelInitialized))
        return fmiError;
    if (nullPointer(comp, "fmiEventUpdate", "eventInfo", eventInfo))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
        "fmiEventUpdate: intermediateResults = %d", intermediateResults);
    eventInfo->iterationConverged  = fmiTrue;
    eventInfo->stateValueReferencesChanged = fmiFalse;
    eventInfo->stateValuesChanged  = fmiFalse;
    eventInfo->terminateSimulation = fmiFalse;
    eventInfo->upcomingTimeEvent   = fmiFalse;
    eventUpdate(comp, eventInfo); // to be implemented by the includer of this file
    return fmiOK;
}

fmiStatus fmiCompletedIntegratorStep(fmiComponent c, fmiBoolean* callEventUpdate){
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiCompletedIntegratorStep", modelInitialized))
         return fmiError;
    if (nullPointer(comp, "fmiCompletedIntegratorStep", "callEventUpdate", callEventUpdate))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiCompletedIntegratorStep");
    *callEventUpdate = fmiFalse;
    return fmiOK;
}

fmiStatus fmiGetStateValueReferences(fmiComponent c, fmiValueReference vrx[], size_t nx){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetStateValueReferences", not_modelError))
        return fmiError;
    if (invalidNumber(comp, "fmiGetStateValueReferences", "nx", nx, NUMBER_OF_STATES))
        return fmiError;
    if (nullPointer(comp, "fmiGetStateValueReferences", "vrx[]", vrx))
         return fmiError;
#if NUMBER_OF_STATES>0
    for (i=0; i<nx; i++) {
        vrx[i] = vrStates[i];
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiGetStateValueReferences: vrx[%d] = %d", i, vrx[i]);
    }
#endif
    return fmiOK;
}

fmiStatus fmiGetContinuousStates(fmiComponent c, fmiReal states[], size_t nx){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetContinuousStates", not_modelError))
        return fmiError;
    if (invalidNumber(comp, "fmiGetContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmiError;
    if (nullPointer(comp, "fmiGetContinuousStates", "states[]", states))
         return fmiError;
#if NUMBER_OF_STATES>0
    for (i=0; i<nx; i++) {
        fmiValueReference vr = vrStates[i];
        states[i] = getReal(comp, vr); // to be implemented by the includer of this file
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiGetContinuousStates: #r%u# = %.16g", vr, states[i]);
    }
#endif
    return fmiOK;
}

fmiStatus fmiGetNominalContinuousStates(fmiComponent c, fmiReal x_nominal[], size_t nx){
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetNominalContinuousStates", not_modelError))
        return fmiError;
    if (invalidNumber(comp, "fmiGetNominalContinuousStates", "nx", nx, NUMBER_OF_STATES))
        return fmiError;
    if (nullPointer(comp, "fmiGetNominalContinuousStates", "x_nominal[]", x_nominal))
         return fmiError;
    if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
        "fmiGetNominalContinuousStates: x_nominal[0..%d] = 1.0", nx-1);
    for (i=0; i<nx; i++) 
        x_nominal[i] = 1;
    return fmiOK;
}

fmiStatus fmiGetDerivatives(fmiComponent c, fmiReal derivatives[], size_t nx) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetDerivatives", not_modelError))
         return fmiError;
    if (invalidNumber(comp, "fmiGetDerivatives", "nx", nx, NUMBER_OF_STATES)) 
        return fmiError;
    if (nullPointer(comp, "fmiGetDerivatives", "derivatives[]", derivatives))
         return fmiError;
#if NUMBER_OF_STATES>0
    for (i=0; i<nx; i++) {
        fmiValueReference vr = vrStates[i] + 1;
        derivatives[i] = getReal(comp, vr); // to be implemented by the includer of this file
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log",
            "fmiGetDerivatives: #r%d# = %.16g", vr, derivatives[i]);
    }
#endif
    return fmiOK;
}

fmiStatus fmiGetEventIndicators(fmiComponent c, fmiReal eventIndicators[], size_t ni) {
    int i;
    ModelInstance* comp = (ModelInstance *)c;
    if (invalidState(comp, "fmiGetEventIndicators", not_modelError))
        return fmiError;
    if (invalidNumber(comp, "fmiGetEventIndicators", "ni", ni, NUMBER_OF_EVENT_INDICATORS)) 
        return fmiError;
#if NUMBER_OF_EVENT_INDICATORS>0
    for (i=0; i<ni; i++) {
        eventIndicators[i] = getEventIndicator(comp, i); // to be implemented by the includer of this file
        if (comp->loggingOn) comp->functions.logger(c, comp->instanceName, fmiOK, "log", 
            "fmiGetEventIndicators: z%d = %.16g", i, eventIndicators[i]);
    }
#endif
    return fmiOK;
}

fmiStatus fmiTerminate(fmiComponent c){
    return terminate("fmiTerminate", c);
}

void fmiFreeModelInstance(fmiComponent c) {
    freeInstance("fmiFreeModelInstance", c);
}

#endif // Model Exchange 1.0

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
