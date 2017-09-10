project('constatus', 'cpp',
	version: '1.6',
	license: 'GPLv2',
	default_options: ['cpp_std=c++11'])

conf = configuration_data()
conf.set('NAME', '"constatus"')
conf.set_quoted('VERSION', meson.project_version())


configure_file(
    output : 'config.h',
    configuration : conf)

add_project_arguments('-include', 'config.h', language : 'cpp')

libjpeg = dependency('libjpeg')
libcurl = dependency('libcurl')
libpng = dependency('libpng')
cairo = dependency('cairo')
jansson = dependency('jansson')
libswscale = dependency('libswscale')
libavutil_dep = dependency('libavutil')
libavformat_dep = dependency('libavformat')
libavcodec_dep = dependency('libavcodec')
libavresample_dep = dependency('libavresample')
thread_dep = dependency('threads')
swresample = dependency('libswresample')


cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : true)
dl_dep = cc.find_library('dl', required : true)
netpbm = cc.find_library('netpbm', required : true)
netpbm_inc = include_directories('/usr/include/netpbm')
gwavi = cc.find_library('gwavi', required: true)

constatus_sources = '''
  cairo.cpp error.cpp exec.cpp filter_add_scaled_text.cpp filter_add_text.cpp
  filter_boost_contrast.cpp filter.cpp filter_grayscale.cpp
  filter_marker_simple.cpp filter_mirror_h.cpp filter_mirror_v.cpp
  filter_motion_only.cpp filter_noise_neighavg.cpp filter_overlay.cpp
  filter_plugin.cpp http_client.cpp http_server.cpp interface.cpp log.cpp
  main.cpp motion_trigger.cpp picio.cpp resize_cairo.cpp resize.cpp source.cpp
  source_http_jpeg.cpp source_http_mjpeg.cpp source_plugin.cpp source_rtsp.cpp
  source_v4l.cpp target_avi.cpp target.cpp target_ffmpeg.cpp target_jpeg.cpp
  target_plugin.cpp utils.cpp v4l2_loopback.cpp
'''.split()

executable(
    'constatus', 
    constatus_sources,
    cpp_args: ['-O3', '-pedantic'],
    dependencies:  [libjpeg, libpng, libcurl, cairo, jansson, netpbm, m_dep,
      libavutil_dep, libavcodec_dep, libavformat_dep, libavresample_dep,
      libswscale, thread_dep, dl_dep, swresample, gwavi],
    include_directories: [netpbm_inc],
    install: true)

# Install configuration files
install_data('constatus-v4l.conf', install_dir: '/usr/share/doc/constatus')
install_data('constatus-v4l-loopback.conf', install_dir: '/usr/share/doc/constatus')
install_data('constatus-rtsp.conf', install_dir: '/usr/share/doc/constatus')
install_data('constatus-jpeg.conf', install_dir: '/usr/share/doc/constatus')
install_data('constatus-mjpeg.conf', install_dir: '/usr/share/doc/constatus')
