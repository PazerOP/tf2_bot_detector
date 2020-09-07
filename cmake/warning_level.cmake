
function(set_warning_level level)

	get_property(TEMP_COMPILE_OPTIONS DIRECTORY PROPERTY COMPILE_OPTIONS)
	# message("COMPILE_OPTIONS pre-remove = ${TEMP_COMPILE_OPTIONS}")

	if (MSVC)
		list(FILTER TEMP_COMPILE_OPTIONS EXCLUDE REGEX "/W[0-9]")
		# message("COMPILE_OPTIONS post-remove = ${TEMP_COMPILE_OPTIONS}")
		list(APPEND TEMP_COMPILE_OPTIONS "/W${level}")
	endif()

	# message("COMPILE_OPTIONS post-add = ${TEMP_COMPILE_OPTIONS}")
	set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${TEMP_COMPILE_OPTIONS}")

endfunction()
