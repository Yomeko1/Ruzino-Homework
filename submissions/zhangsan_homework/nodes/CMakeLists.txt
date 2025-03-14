add_nodes(
	TARGET_NAME ${student_name}_nodes_hw_submissions
	DEP_LIBS stage nodes_system usd geometry usdShade Eigen3::Eigen autodiff igl::core igl_restricted::triangle ${student_name}_${util_lib_target_name}
	COMPILE_DEFS NOMINMAX
)
