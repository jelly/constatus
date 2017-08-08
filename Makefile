# (C) 2017 by Folkert van Heusden, released under AGPL v3.0

DEBUG=-ggdb3
NAME="constatus"
VERSION="0.1"
CXXFLAGS=$(DEBUG) -std=c++11 -DNAME=\"$(NAME)\" -DVERSION=\"$(VERSION)\" -O3 #-march=native -mtune=native -fomit-frame-pointer -flto
LDFLAGS=$(DEBUG) -ljpeg -lpng -lcurl -ljansson -lgwavi #-flto
OBJS=frame.o source.o main.o io_buffer.o error.o source_v4l.o utils.o picio.o motion_trigger.o write_stream_to_file.o filter.o filter_mirror_v.o filter_noise_neighavg.o http_client.o source_http_jpeg.o filter_mirror_h.o filter_add_text.o source_http_mjpeg.o filter_grayscale.o exec.o filter_boost_contrast.o filter_marker_simple.o http_server.o push_to_vloopback.o filter_overlay.o

constatus: $(OBJS)
	$(CXX) $(LDFLAGS) -o constatus -pthread $(OBJS)

clean:
	rm -f $(OBJS) constatus
