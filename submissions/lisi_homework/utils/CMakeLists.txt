aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} THISSRCS)
add_library(
        ${student_name}_${util_lib_target_name} STATIC
        ${THISSRCS}
)
target_include_directories(${student_name}_${util_lib_target_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${student_name}_${util_lib_target_name}
    stage nodes_system usd geometry usdShade Eigen3::Eigen autodiff igl::core igl_restricted::triangle
)