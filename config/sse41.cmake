
# SSE-4.1
message(STATUS "Testing whether sse-4.1 code can be used")
try_run(sse41_runs sse41_compiles
            ${CADO_NFS_BINARY_DIR}/config
            ${CADO_NFS_SOURCE_DIR}/config/sse41.c)
if(sse41_compiles)
    if (sse41_runs MATCHES FAILED_TO_RUN)
        message(STATUS "Testing whether sse-4.1 code can be used -- No")
        set(HAVE_SSE41 0)
    else (sse41_runs MATCHES FAILED_TO_RUN)
        message(STATUS "Testing whether sse-4.1 code can be used -- Yes")
        set(HAVE_SSE41 1)
    endif (sse41_runs MATCHES FAILED_TO_RUN)
else(sse41_compiles)
    try_run(sse41_runs sse41_compiles
        ${CADO_NFS_BINARY_DIR}/config
        ${CADO_NFS_SOURCE_DIR}/config/sse41.c
        COMPILE_DEFINITIONS -msse4.1)
    if(sse41_compiles)
        if (sse41_runs MATCHES FAILED_TO_RUN)
            message(STATUS "Testing whether sse-4.1 code can be used -- No")
            set(HAVE_SSE41 1)
        set(HAVE_SSE41 0)
        else (sse41_runs MATCHES FAILED_TO_RUN)
            message(STATUS "Testing whether sse-4.1 code can be used -- Yes, with -msse4.1")
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
            set(HAVE_SSE41 1)
        endif (sse41_runs MATCHES FAILED_TO_RUN)
    else(sse41_compiles)
        message(STATUS "Testing whether sse-4.1 code can be used -- No")
        set(HAVE_SSE41 0)
    endif(sse41_compiles)
endif(sse41_compiles)
