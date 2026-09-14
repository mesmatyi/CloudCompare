target_sources( ${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/Lanelet2MapDemo.h
		${CMAKE_CURRENT_LIST_DIR}/Lanelet2MapDialog.h
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)
