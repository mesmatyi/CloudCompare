function( AddLinuxAppImageTarget )
	if( NOT UNIX OR APPLE )
		return()
	endif()

	find_program( BASH_PROGRAM bash )
	if( NOT BASH_PROGRAM )
		message( STATUS "Skipping AppImage target: bash not found" )
		return()
	endif()

	if( NOT TARGET CloudCompare )
		message( STATUS "Skipping AppImage target: CloudCompare target not available" )
		return()
	endif()

	add_custom_target( appimage
		COMMAND
			${BASH_PROGRAM}
			${CMAKE_SOURCE_DIR}/scripts/linux/build-appimage.sh
			${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}/appimage-install
			${CMAKE_BINARY_DIR}/appimage-out
			${CMAKE_BINARY_DIR}/CloudCompare.AppDir
		DEPENDS
			CloudCompare
		USES_TERMINAL
		COMMENT "Build CloudCompare AppImage"
	)
endfunction()
