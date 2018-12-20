# fmusim makefile for Mac OS X

EXECS = \
	fmusim_cs \
	fmusim_me

# Build simulators for co_simulation and model_exchange and then build the .fmu files.
all: $(EXECS)
	(cd models; $(MAKE))

clean:
	rm -f $(EXECS)
	rm -rf  *.dSYM
	rm -f cosimulation/*.o
	rm -f model_exchange/*.o
	(cd models; $(MAKE) clean)

# Sources shared between co-simulation and model exchange
SHARED_SRCS = \
	shared/sim_support.c \
	shared/xmlVersionParser.c \
	shared/parser/stack.c \
	shared/parser/xml_parser.c

# Dependencies for only fmusim_cs
CO_SIMULATION_DEPS = \
	co_simulation/main.c \
	co_simulation/fmi_cs.h \
	shared/include/fmiFunctions.h \
	shared/include/fmiPlatformTypes.h

# Dependencies for only fmusim_me
MODEL_EXCHANGE_DEPS = \
	model_exchange/main.c \
	model_exchange/fmi_me.h \
	shared/include/fmiModelFunctions.h \
	shared/include/fmiModelTypes.h

# Dependencies shared between both fmusim_cs and fmusim_me
SHARED_DEPS = \
	shared/sim_support.c \
	shared/sim_support.h \
	shared/xmlVersionParser.c \
	shared/xmlVersionParser.h \
	shared/parser/expat.h \
	shared/parser/expat_external.h \
	shared/parser/stack.c \
	shared/parser/stack.h \
	shared/parser/xml_parser.c \
	shared/parser/xml_parser.h 

# Set CFLAGS to -m32 to build for linux32
#CFLAGS=-m32
# See also models/build_fmu

# Create the binaries in the current directory because co_simulation already has
# a directory named "fmusim_cs"
fmusim_cs: $(CO_SIMULATION_DEPS) $(SHARED_DEPS) ../bin/
	$(CC) $(CFLAGS) -g -Wall -DFMI_COSIMULATION -DSTANDALONE_XML_PARSER \
		-Ico_simulation -Ishared/include -Ishared/parser -Ishared \
		co_simulation/main.c $(SHARED_SRCS) \
		-o $@ -lexpat -lxml2 -ldl
	cp fmusim_cs ../bin/

fmusim_me: $(MODEL_EXCHANGE_DEPS) $(SHARED_DEPS) ../bin/
	$(CC) $(CFLAGS) -g -Wall -DSTANDALONE_XML_PARSER \
		-Imodel_exchange -Ishared/include -Ishared/parser -Ishared \
		model_exchange/main.c $(SHARED_SRCS) \
		-o $@ -lexpat -lxml2 -ldl
	cp fmusim_me ../bin/

../bin/:
	if [ ! -d ../bin ]; then \
		echo "Creating ../bin/"; \
		mkdir ../bin/; \
	fi