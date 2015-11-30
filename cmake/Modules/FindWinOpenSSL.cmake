#  - Try to find OpenSSL
#
#  OPENSSL_INCLUDE_DIRECTORY - the OpenSSL include directory
#  OPENSSL_LIB_DIRECTORY - the OpenSSL lib directory
  if (${QT_PORT})
    set(build_type x86)
  else()
    set(build_type x64)
  endif()
  execute_process(
      COMMAND ${CMAKE_SOURCE_DIR}/ms_tools/make_ssl_lib.cmd ${build_type}
  )

set(OPENSSL_INCLUDE_DIRECTORY $ENV{OPENSSL_DIR}/include)
set(OPENSSL_LIB_DIRECTORY $ENV{OPENSSL_DIR}/lib)
