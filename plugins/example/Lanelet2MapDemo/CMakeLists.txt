option( PLUGIN_EXAMPLE_LANELET2_MAP_DEMO "Install Lanelet2 map demo plugin" OFF )

if ( PLUGIN_EXAMPLE_LANELET2_MAP_DEMO )
	project( Lanelet2MapDemo )

	AddPlugin( NAME ${PROJECT_NAME} )

	add_subdirectory( include )
	add_subdirectory( src )
endif()
