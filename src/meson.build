# Library sources
lib_sources = [
  'rounded-blur-effect.c',
  'rounded-blur-mode.c',
]

# Library headers
lib_headers = [
  'rounded-blur-effect.h',
]

# Version information
version_split = meson.project_version().split('.')
major_version = version_split[0].to_int()
minor_version = version_split[1].to_int()
micro_version = version_split[2].to_int()

# Shared library
gnome_rounded_blur = shared_library(
  'blur-effect-' + api_version,
  lib_sources,
  dependencies: deps,
  version: meson.project_version(),
  soversion: major_version,
  install: true,
  install_dir: libdir,
)

# Install headers
install_headers(
  lib_headers,
  subdir: 'blur-effect-' + api_version,
)

# Generate GObject Introspection data
gnome_rounded_blur_gir = gnome.generate_gir(
  gnome_rounded_blur,
  sources: lib_sources + lib_headers,
  nsversion: api_version,
  namespace: 'Blur',
  symbol_prefix: 'gb',
  identifier_prefix: 'Gb',
  includes: ['GObject-2.0'],
  install: true,
  install_dir_gir: join_paths(datadir, 'gir-1.0'),
  install_dir_typelib: join_paths(libdir, 'girepository-1.0'),
  fatal_warnings: false,
)

# pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  gnome_rounded_blur,
  name: project_name,
  description: 'Library providing Blur.BlurEffect with corner radius support',
  version: project_version,
  filebase: 'blur-effect-' + api_version,
  subdirs: 'blur-effect-' + api_version,
)
