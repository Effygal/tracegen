project(
    'trace-gen',
    'cpp',
    default_options: [
        'cpp_std=c++20',
        'buildtype=release',
    ],
)

compiler = meson.get_compiler('cpp')
message('Compiler = ' + compiler.get_id() + ', version: ' + compiler.version())

tracegen_src = [
    'src/tracegen.cc',
]

td_tracegen_src = [
   'src/2d-tracegen.cc', 
]

kd_tracegen_src = [
    'src/kd-tracegen.cc',
]

tracegen_deps = [
    dependency('threads'),
    dependency(
        'boost',
        modules: ['system', 'filesystem', 'program_options', 'thread', 'regex'],
    ),
    dependency('fmt'),
]

executable('trace-gen', tracegen_src, dependencies: [tracegen_deps])

executable('2d-tracegen', td_tracegen_src, dependencies: [tracegen_deps])

executable('kd-tracegen', kd_tracegen_src, dependencies: [tracegen_deps])