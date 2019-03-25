from os import environ

VariantDir('build/src', 'src', duplicate=0)
VariantDir('build/lib', 'lib', duplicate=0)
flags = ['-O3', '-march=native', '-std=c++11']

env = Environment(ENV       = environ,
                  CXX       = 'g++',
                  CPPFLAGS  = ['-Wno-unused-value'],
                  CXXFLAGS  = flags,
                  LINKFLAGS = flags,
                  CPPPATH   = ['#simpleini', '#lib/include', '#src/include'],
                  LIBS      = ['SDL2', 'SDL2_image', 'SDL2_ttf'])

env.Program('laines', Glob('build/*/*.cpp') + Glob('build/*/*/*.cpp') + Glob('build/*/*.c') + Glob('build/*/*/*.c'))
