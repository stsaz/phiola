# libDynamicAudioNormalizer

include ../config.mk

VER := 2.10
URL := https://github.com/lordmulder/DynamicAudioNormalizer/archive/refs/tags/$(VER).tar.gz
PKG := $(notdir $(URL))
DIR := DynamicAudioNormalizer-$(VER)
LIB := libDynamicAudioNormalizer-phi.$(SO)

default: $(DIR)
	$(MAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_GZ) $(PKG)
	touch $(DIR)

# build
CXXFLAGS += -std=gnu++11 \
	-ffast-math -fomit-frame-pointer -fno-tree-vectorize \
	-DNO_JAVA_SUPPORT -DNDEBUG \
	-I$(DIR)/DynamicAudioNormalizerAPI/src -I$(DIR)/DynamicAudioNormalizerAPI/include -I$(DIR)/DynamicAudioNormalizerShared/include

ifeq "$(CPU)" "x86"
	 CXXFLAGS += -mfpmath=sse -msse
endif

SRC := \
	$(DIR)/DynamicAudioNormalizerAPI/src/DynamicAudioNormalizer.cpp \
	$(DIR)/DynamicAudioNormalizerAPI/src/GaussianFilter.cpp \
	$(DIR)/DynamicAudioNormalizerAPI/src/FrameBuffer.cpp
OBJ := DynamicAudioNormalizer-ff.o $(SRC:.cpp=.o)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINKXX) $(LINKXXFLAGS) -shared $+ -o $@

clean:
	$(RM) $(OBJ) $(DIR)
