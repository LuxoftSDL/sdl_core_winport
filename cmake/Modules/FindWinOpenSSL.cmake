#  - Try to find OpenSSL
#
#  OPENSSL_INCLUDE_DIRECTORY - the OpenSSL include directory
#  OPENSSL_LIB_DIRECTORY - the OpenSSL lib directory
  execute_process(
      COMMAND ${CMAKE_SOURCE_DIR}/ms_tools/make_ssl_lib.cmd
  )

set(OPENSSL_INCLUDE_DIRECTORY $ENV{OPENSSL_DIR}/include)
set(OPENSSL_LIB_DIRECTORY $ENV{OPENSSL_DIR}/lib)
