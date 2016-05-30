# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

from waflib import Logs, Utils, Context
import os

VERSION = '0.0'
APPNAME = 'partialsync'

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['boost', 'doxygen', 'sphinx_build', 'default-compiler-flags',
              'pch'],
             tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs', 'boost', 'pch',
               'doxygen', 'sphinx_build', 'default-compiler-flags'])

    if not os.environ.has_key('PKG_CONFIG_PATH'):
        os.environ['PKG_CONFIG_PATH'] = ':'.join([
            '/usr/lib/pkgconfig',
            '/usr/local/lib/pkgconfig',
            '/opt/local/lib/pkgconfig'])
    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
                   uselib_store='NDN_CXX', mandatory=True)

def build(bld):
    libpartialsync = bld(
        target='PartialSync',
        features=['cxx', 'cxxshlib'],
        source =  bld.path.ant_glob(['src/**/*.cpp', 'src/**/*.proto']),
        use = 'NDN_CXX',
        includes = ['src', '.'],
        export_includes=['src', '.'],
        )

    bld.install_files(
        dest = "%s/PartialSync" % bld.env['INCLUDEDIR'],
        files = bld.path.ant_glob(['src/**/*.hpp', 'src/**/*.h']),
        cwd = bld.path.find_dir("src"),
        relative_trick = False,
        )

    bld.install_files(
        dest = "%s/PartialSync" % bld.env['INCLUDEDIR'],
        files = bld.path.get_bld().ant_glob(['src/**/*.hpp', 'src/**/*.h']),
        cwd = bld.path.get_bld().find_dir("src"),
        relative_trick = False,
        )

    pc = bld(
        features = "subst",
        source='PartialSync.pc.in',
        target='PartialSync.pc',
        install_path = '${LIBDIR}/pkgconfig',
        PREFIX       = bld.env['PREFIX'],
        INCLUDEDIR   = "%s/PartialSync" % bld.env['INCLUDEDIR'],
        VERSION      = VERSION,
        )