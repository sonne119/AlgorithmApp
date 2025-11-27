# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "app/CMakeFiles/app_autogen.dir/AutogenUsed.txt"
  "app/CMakeFiles/app_autogen.dir/ParseCache.txt"
  "app/app_autogen"
  "lib/CMakeFiles/collatzlib_autogen.dir/AutogenUsed.txt"
  "lib/CMakeFiles/collatzlib_autogen.dir/ParseCache.txt"
  "lib/collatzlib_autogen"
  )
endif()
