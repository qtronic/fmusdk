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
	shared/xmlVersionParser.c

CPP_SRCS = \
	shared/parser/XmlElement.cpp \
	shared/parser/XmlParser.cpp \
	shared/parser/XmlParserCApi.cpp

# Dependencies for only fmusim_cs
CO_SIMULATION_DEPS = \
	co_simulation/main.c

# Dependencies for only fmusim_me
MODEL_EXCHANGE_DEPS = \
	model_exchange/main.c

# Dependencies shared between both fmusim_cs and fmusim_me
SHARED_DEPS = \
	shared/sim_support.c \
	shared/sim_support.h \
	shared/xmlVersionParser.c \
	shared/xmlVersionParser.h \
	shared/fmi2.h \
	shared/include/fmi2Functions.h \
	shared/include/fmi2FunctionTypes.h \
	shared/include/fmi2TypesPlatform.h \
	shared/parser/fmu20/XmlElement.h \
	shared/parser/fmu20/XmlParser.h \
	shared/parser/fmu20/XmlParserException.h \
	shared/parser/XmlParserCApi.h

# Set CFLAGS to -m32 to build for linux32
#CFLAGS=-m32
# See also models/build_fmu

CXX=c++
# Create the binaries in the current directory because co_simulation already has
# a directory named "fmusim_cs"
fmusim_cs: $(CO_SIMULATION_DEPS) $(SHARED_DEPS) ../bin/
	$(CC) $(CFLAGS) -g -Wall -DFMI_COSIMULATION \
		-DSTANDALONE_XML_PARSER -DLIBXML_STATIC \
		-Ishared/include -Ishared/parser -Ishared \
		co_simulation/main.c $(SHARED_SRCS) \
		-c
	$(CXX) $(CFLAGS) -g -Wall -DFMI_COSIMULATION \
		-DSTANDALONE_XML_PARSER -DLIBXML_STATIC \
		-Ishared/include -Ishared/parser -Ishared \
		main.o sim_support.o xmlVersionParser.o $(CPP_SRCS) \
		-o $@ -ldl -lxml2
	cp fmusim_cs ../bin/

fmusim_me: $(MODEL_EXCHANGE_DEPS) $(SHARED_DEPS) ../bin/
	$(CC) $(CFLAGS) -g -Wall \
		-DSTANDALONE_XML_PARSER -DLIBXML_STATIC \
		-Ishared/include -Ishared/parser -Ishared \
		model_exchange/main.c $(SHARED_SRCS) \
		-c
	$(CXX) $(CFLAGS) -g -Wall \
		-DSTANDALONE_XML_PARSER -DLIBXML_STATIC \
		-Ishared/include -Ishared/parser -Ishared \
		main.o sim_support.o xmlVersionParser.o $(CPP_SRCS) \
		-o $@ -ldl -lxml2
	cp fmusim_me ../bin/

../bin/:
	if [ ! -d ../bin ]; then \
		echo "Creating ../bin/"; \
		mkdir ../bin/; \
	fi
