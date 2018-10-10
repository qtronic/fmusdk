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

if (${FMI_VERSION} EQUAL 10)
  if (${FMI_TYPE} STREQUAL "cs")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu10/src/co_simulation/include")
  else ()
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu10/src/model_exchange/include")
  endif ()
else ()
  target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fmu20/src/shared/include")
endif()

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
