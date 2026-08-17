project(
  'simplex',
  'cpp',
  default_options: [
    'cpp_std=c++20',
    'buildtype=debug',
    'warning_level=3',
    'b_ndebug=if-release',
    'b_lto=true',
  ],
)

cpp = meson.get_compiler('cpp')

deps = [
  dependency('fmt', required: true),
  dependency('cxxopts', required: true),
  dependency('eigen3', required: true),
]

# SuiteSparse
suitesparse_dep = [
  cpp.find_library('umfpack', required: true),
  cpp.find_library('cholmod', required: true),
  cpp.find_library('amd', required: true),
  cpp.find_library('suitesparseconfig', required: true),
]

deps += suitesparse_dep


subdir('src')

incdir = include_directories('include', '/usr/include/suitesparse')

# Executable
executable(
  'simplex',
  sources,
  include_directories: incdir,
  dependencies: deps,
  cpp_args: ['-march=native', '-O3'],
  install: true,
)

