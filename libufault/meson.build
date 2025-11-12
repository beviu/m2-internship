project(
  'libufault',
  'c',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3'],
)

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_LIBUFAULT']

dependencies = [
]

lib = library(
  'libufault',
  'libufault.c',
  install : true,
  c_shared_args : lib_args,
  gnu_symbol_visibility : 'hidden',
  dependencies : dependencies,
)

test_exe = executable(
  'libufault_test',
  'libufault_test.c',
  dependencies : dependencies,
  link_with : lib,
)
test('libufault', test_exe)

# Make this library usable as a Meson subproject.
libufault_dep = declare_dependency(
  include_directories : include_directories('.'),
  dependencies : dependencies,
  link_with : lib,
)
meson.override_dependency('libufault', libufault_dep)

# Make this library usable from the system's
# package manager.
install_headers('libufault.h', subdir : 'libufault')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  lib,
  description : 'Meson sample project.',
  subdirs : 'libufault',
)
