USTC_CG_ADD_LIB(
	rzconsole
	SHARED
	PRIVATE_LIBS 
		GUI glm::glm spdlog::spdlog
	COMPILE_DEFS
		NOMINMAX=1
)