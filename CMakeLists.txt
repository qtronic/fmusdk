cmake_minimum_required (VERSION 3.2)

FUNCTION(cat IN_FILE OUT_FILE)
  file(READ ${IN_FILE} CONTENTS)
  file(APPEND ${OUT_FILE} "${CONTENTS}")
ENDFUNCTION()

project (FMUSDK)

if (WIN32)
   set(FMI_PLATFORM win)
elseif (APPLE)
   set(FMI_PLATFORM darwin)
else ()
   set(FMI_PLATFORM linux)
endif ()

if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    set (FMI_PLATFORM ${FMI_PLATFORM}64)
else ()
    set (FMI_PLATFORM ${FMI_PLATFORM}32)
endif ()

MESSAGE("FMI_PLATFORM: " ${FMI_PLATFORM})

# --------------------- FMU models ---------------------
foreach (FMI_VERSION 10 20)
foreach (FMI_TYPE cs me)
foreach (MODEL_NAME bouncingBall dq inc values vanDerPol)

set(TARGET_NAME ${MODEL_NAME}_${FMI_VERSION}_${FMI_TYPE})

add_library(${TARGET_NAME} SHARED fmu${FMI_VERSION}/src/models/${MODEL_NAME}/${MODEL_NAME}.c)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist/fmi${FMI_VERSION}/${FMI_TYPE})

target_compile_definitions(${TARGET_NAME} PRIVATE DISABLE_PREFIX)

if (${FMI_TYPE} MATCHES "cs")
  target_compile_definitions(${TARGET_NAME} PRIVATE FMI_COSIMULATION)
endif()

target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/models")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/include")

set(FMU_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/temp/fmi${FMI_VERSION}/${FMI_TYPE}/${MODEL_NAME})

set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    LIBRARY_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    ARCHIVE_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}/binaries/${FMI_PLATFORM}"
)

set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME ${MODEL_NAME})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

if (${FMI_VERSION} EQUAL 10)

file(WRITE ${FMU_BUILD_DIR}/modelDescription.xml "")
cat(${CMAKE_CURRENT_SOURCE_DIR}/fmu10/src/models/${MODEL_NAME}/modelDescription.xml ${FMU_BUILD_DIR}/modelDescription.xml )
cat(${CMAKE_CURRENT_SOURCE_DIR}/fmu10/src/models/${FMI_TYPE}.xml ${FMU_BUILD_DIR}/modelDescription.xml )

else()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/fmu20/src/models/${MODEL_NAME}/modelDescription_${FMI_TYPE}.xml
  "${FMU_BUILD_DIR}/modelDescription.xml"
)

endif()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/models/${MODEL_NAME}/${MODEL_NAME}.c"
  "${FMU_BUILD_DIR}/sources/${MODEL_NAME}.c"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/models/fmuTemplate.c"
  "${FMU_BUILD_DIR}/sources/fmuTemplate.c"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/models/fmuTemplate.h"
  "${FMU_BUILD_DIR}/sources/fmuTemplate.h"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_SOURCE_DIR}/dist/fmi${FMI_VERSION}/${FMI_TYPE}/${MODEL_NAME}.fmu" --format=zip
  "modelDescription.xml"
  "binaries"
  "sources"
  WORKING_DIRECTORY ${FMU_BUILD_DIR}
)

endforeach(MODEL_NAME)
endforeach(FMI_TYPE)
endforeach(FMI_VERSION)

# --------------------- FMU simulators ---------------------
foreach (FMI_VERSION 10 20)
foreach (FMI_TYPE cs me)
set(TARGET_NAME fmusim_${FMI_VERSION}_${FMI_TYPE})

if (${FMI_TYPE} STREQUAL "cs")
  set(SIM_TYPE co_simulation)
else ()
  set(SIM_TYPE model_exchange)
endif ()

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/sim_support.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/xmlVersionParser.c")

if (${FMI_VERSION} EQUAL 10)
  set(SRCS ${SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/stack.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/xml_parser.c")
else ()
  set(SRCS ${SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/XmlElement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/XmlParser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/XmlParserCApi.cpp")
endif ()

add_executable(${TARGET_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/${SIM_TYPE}/main.c" ${SRCS})

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist/fmi${FMI_VERSION}/${FMI_TYPE})

target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/${SIM_TYPE}")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/include")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser")

if (${FMI_TYPE} STREQUAL "cs")
  target_compile_definitions(${TARGET_NAME} PRIVATE FMI_COSIMULATION)
endif ()
target_compile_definitions(${TARGET_NAME} PRIVATE STANDALONE_XML_PARSER)
target_compile_definitions(${TARGET_NAME} PRIVATE LIBXML_STATIC)

if (WIN32)
  set(TARGET_OUTPUT_NAME, "${TARGET_NAME}.exe")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

  target_link_libraries (${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/${FMI_PLATFORM}/libxml2.lib")
  if (${FMI_VERSION} EQUAL 10)
    target_link_libraries (${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu${FMI_VERSION}/src/shared/parser/${FMI_PLATFORM}/libexpatMT.lib")
  endif ()
else ()
  set(TARGET_OUTPUT_NAME, "${TARGET_NAME}")
  target_link_libraries (${TARGET_NAME} PRIVATE "xml2")
  target_link_libraries (${TARGET_NAME} PRIVATE "expat")
endif ()


set(FMU_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/temp/fmi${FMI_VERSION}/${FMI_TYPE})

set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}"
    LIBRARY_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY         "${FMU_BUILD_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${FMU_BUILD_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${FMU_BUILD_DIR}"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
  "${FMU_BUILD_DIR}/${TARGET_OUTPUT_NAME}"
  "${CMAKE_CURRENT_SOURCE_DIR}/dist/fmi${FMI_VERSION}/${FMI_TYPE}/"
)

endforeach(FMI_TYPE)
endforeach(FMI_VERSION)
