project(
  'dracula',
  'cpp',
  default_options: [
    'cpp_std=c++20',
    'buildtype=release',
    'warning_level=3',
    'b_ndebug=if-release',
    'b_lto=true',
  ],
)


deps = [] 
fmt_dep = dependency('fmt', required: true)
deps += fmt_dep

argparse = dependency('cxxopts', required: true)
deps += argparse

eigen = depencency('eigen', required: true)
deps += eigen

subdir('src')

incdir = include_directories('include')

extra_args = ['-march=native','-O3']

executable('dracula', sources,
  include_directories: incdir,
  cpp_args: extra_args,
  install: true,
  dependencies: deps
)
