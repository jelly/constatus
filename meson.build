project('constatus', 'cpp',
	version: '0',
	license: 'GPLv2')

conf = configuration_data()
conf.set('NAME', '"constatus"')
conf.set('VERSION', '"0"') #meson.project_version())


configure_file(
    output : 'config.h',
    configuration : conf)

add_project_arguments('-include', 'config.h', language : 'cpp')

libjpeg = dependency('libjpeg')
libcurl = dependency('libcurl')
libpng = dependency('libpng')
cairo = dependency('cairo')
jansson = dependency('jansson')

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : true)
netpbm = cc.find_library('netpbm', required : true)


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
    dependencies:  [libjpeg, libpng, libcurl, cairo, jansson, netpbm, m_dep],
    install: true)

