# libALAC

include ../config.mk

VER :=
URL := https://github.com/macosforge/alac/archive/refs/heads/master.zip
PKG := $(ALIB3)/ALAC/$(notdir $(URL))
DIR := alac-master
LIB := libALAC-phi.$(SO)

default: $(DIR)
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNZIP) -n $(PKG)
	cat $(ALIB3)/ALAC/*.patch | patch -d $(DIR) -p1

# build
CF := -I$(DIR)/codec

SRC_CPP := \
	$(DIR)/codec/ALACDecoder.cpp
SRC := \
	$(DIR)/codec/EndianPortable.c \
	$(DIR)/codec/ALACBitUtilities.c \
	$(DIR)/codec/ag_dec.c \
	$(DIR)/codec/dp_dec.c \
	$(DIR)/codec/matrix_dec.c
# SRC_CPP := \
# 	$(DIR)/codec/ALACEncoder.cpp
# SRC += \
# 	$(DIR)/codec/ag_enc.c \
# 	$(DIR)/codec/dp_enc.c \
# 	$(DIR)/codec/matrix_enc.c
OBJ := ALAC-phi.o $(SRC_CPP:.cpp=.o) $(SRC:.c=.o)

%.o: %.c
	$(C) $(CFLAGS) $(CF) $< -o $@

%.o: $(ALIB3)/ALAC/%.cpp
	$(CXX) $(CXXFLAGS) $(CF) $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CF) $< -o $@

$(LIB): $(OBJ)
	$(LINKXX) $(LINKXXFLAGS) -shared $+ -o $@

clean:
	$(RM) $(OBJ) $(DIR)
