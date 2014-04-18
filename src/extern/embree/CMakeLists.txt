include_directories(${CMAKE_CURRENT_SOURCE_DIR})

SET(SOURCES

    sys/platform.cpp
    sys/sysinfo.cpp
    sys/filename.cpp
    sys/thread.cpp
    sys/network.cpp
    sys/taskscheduler.cpp
    sys/taskscheduler_sys.cpp
    sys/taskscheduler_mic.cpp
    sys/sync/mutex.cpp
    sys/sync/condition.cpp
    sys/stl/string.cpp

    embree.cpp
    common/alloc.cpp
    common/stat.cpp 
    common/accel.cpp
    common/registry_accel.cpp
    common/registry_builder.cpp
    common/registry_intersector.cpp

    geometry/triangle_mesh.cpp
    geometry/virtual_scene.cpp

    builders/primrefgen.cpp 
    builders/heuristic_binning.cpp 
    builders/heuristic_spatial.cpp 
    builders/splitter.cpp 
    builders/splitter_parallel.cpp 
    builders/splitter_fallback.cpp 

    bvh2/bvh2.cpp   
    bvh2/bvh2_intersector1.cpp   
    bvh2/bvh2_intersector4.cpp   
    bvh2/bvh2_intersector8.cpp   

    bvh4/bvh4.cpp   
    bvh4/bvh4_intersector1.cpp   
    bvh4/bvh4_intersector4_single.cpp   
    bvh4/bvh4_intersector4_chunk.cpp   
    bvh4/bvh4_intersector4_hybrid.cpp   
    bvh4/bvh4_intersector8_single.cpp   
    bvh4/bvh4_intersector8_chunk.cpp   
    bvh4/bvh4_intersector8_hybrid.cpp   

    bvh4i/bvh4i.cpp   
    bvh4i/bvh4i_intersector1.cpp   
                           
    bvh4mb/bvh4mb.cpp
    bvh4mb/bvh4mb_builder.cpp   
    bvh4mb/bvh4mb_intersector1.cpp   
    bvh4mb/bvh4mb_intersector4.cpp   
    bvh4mb/bvh4mb_intersector8.cpp
    )

IF (__AVX__)
    SET(SOURCES ${SOURCES} 
    bvh4/bvh4_intersector1_avx.cpp
    bvh8/bvh8.cpp   
    bvh8/bvh8_intersector1.cpp 
    )
ENDIF ()

add_library(embree STATIC ${SOURCES})
set_target_properties(embree PROPERTIES COMPILE_FLAGS "-ffast-math -fvisibility-inlines-hidden -fvisibility=hidden")
