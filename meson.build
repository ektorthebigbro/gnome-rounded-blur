project('gnome-rounded-blur', 'c',
  version: '1.1.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>= 0.56.0',
  default_options: [
    'warning_level=2',
    'c_std=gnu11',
  ],
)

# Project information
project_name = meson.project_name()
project_version = meson.project_version()

# API version
api_version = '1.0'

# Directories
prefix = get_option('prefix')
libdir = join_paths(prefix, get_option('libdir'))
includedir = join_paths(prefix, get_option('includedir'))
datadir = join_paths(prefix, get_option('datadir'))

mutter_api_version = '18'
mutter_req = '>= 50.0'

if not dependency('mutter-clutter-' + mutter_api_version, version: mutter_req, required: false).found()
  mutter_api_version = '17'
  mutter_req = '>= 49.0'
endif

clutter_pc = 'mutter-clutter-' + mutter_api_version
cogl_pc = 'mutter-cogl-' + mutter_api_version
libmutter_pc = 'libmutter-' + mutter_api_version

# Dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.66')
gobject_dep = dependency('gobject-2.0', version: '>= 2.66')
clutter_dep = dependency(clutter_pc, version: mutter_req,
  required: true,
)

cogl_dep = dependency(cogl_pc, version: mutter_req,
  required: true,
)
libmutter_dep = dependency(libmutter_pc, version: mutter_req)
mutter_private_libdir = join_paths(
  clutter_dep.get_variable(pkgconfig: 'libdir'),
  'mutter-' + mutter_api_version,
)

# Required dependencies
deps = [
  glib_dep,
  gobject_dep,
  clutter_dep,
  cogl_dep,
  libmutter_dep
]

# GObject Introspection
gnome = import('gnome')
gir = find_program('g-ir-scanner', required: true)
gir_compiler = find_program('g-ir-compiler', required: true)

# Source subdirectory
subdir('src')
