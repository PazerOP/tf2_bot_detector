cmake_minimum_required(VERSION 3.0)

project(glad2)

add_library(glad2
	"include/glad/gl.h"
	"include/KHR/khrplatform.h"
	"src/gl.c"
)

target_include_directories(glad2 PUBLIC "include")

add_library(glad2::glad2 ALIAS glad2)
