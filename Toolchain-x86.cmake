SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR "i686")

SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)

SET(BITS 32)

if (EXISTS "/etc/debian_version")
	SET (CMAKE_INSTALL_LIBDIR "lib/i386-linux-gnu")
else (EXISTS "/etc/debian_version")
	SET (CMAKE_INSTALL_LIBDIR "lib32")
endif (EXISTS "/etc/debian_version")
