BUILD_SCRIPT := $(CURDIR)/cmake/CMakeBuild/bin/cmake.py

# Define here a list of generic targets to be built separately using a suffix to select the variant and link option.
# Examples: <project> must be replaced by a make target defined below.
#
# How to build a single target:
#  make <project>-a  => build variant=debug,release,relwithdebinfo
#  make <project>-r  => build variant=release
#  make <project>-d  => build variant=debug
#  make <project>-p  => build variant=relwithdebinfo
#
# How to clean and build a single target:
#  make <project>-ca  => clean + build variant=debug,release,relwithdebinfo
#  make <project>-cr  => clean + build variant=release
#  make <project>-cd  => clean + build variant=debug
#  make <project>-cp  => clean + build variant=relwithdebinfo
#

TARGETS := TLibCommon TAppDecoder TAppDecoderAnalyser TLibDecoder 
TARGETS += TAppEncoder TLibEncoder Utilities MCTSExtractor SEIFilmGrainApp

ifeq ($(OS),Windows_NT)
  ifneq ($(MSYSTEM),)
    # MSYS runtime environment
    UNAME_S := $(shell uname -s)
    PYTHON_LAUNCHER := python3
    BUILD_CMD := $(PYTHON_LAUNCHER) $(BUILD_SCRIPT)
  else
    UNAME_S := Windows
    PY := $(wildcard c:/windows/py.*)
    ifeq ($(PY),)
      PYTHON_LAUNCHER := python
    else
      PYTHON_LAUNCHER := $(notdir $(PY))
    endif
    # If a plain cmake.py is used, the exit codes won't arrive in make; i.e. build failures are reported as success by make.
    BUILD_CMD := $(PYTHON_LAUNCHER) $(BUILD_SCRIPT)
    ifeq ($(toolset),gcc)
      g := mgwmake
    endif
  endif
else
  UNAME_S := $(shell uname -s)
  BUILD_CMD := $(BUILD_SCRIPT)
  ifeq ($(UNAME_S),Linux)
    # for Jenkins: run trace build only on Linux
    LINUXBUILD := TRUE
  endif
  ifeq ($(UNAME_S),Darwin)
    # MAC
  endif
endif

ifeq ($(j),)
BUILD_JOBS += -j
else
BUILD_JOBS += -j$(j)
endif

ifneq ($(g),)
CONFIG_OPTIONS += -g $(g)
endif

ifneq ($(toolset),)
  # aarch64 and mingw are two shortcuts to simplify toolset specification.
  ifeq ($(toolset),mingw)
    CONFIG_OPTIONS += toolset=x86_64-w64-mingw32-gcc-posix
  else
    CONFIG_OPTIONS += toolset=$(toolset)
  endif
endif

ifneq ($(address-model),)
CONFIG_OPTIONS += address-model=$(address-model)
endif

ifneq ($(address-sanitizer),)
CMAKE_OPTIONS += -DUSE_ADDRESS_SANITIZER=ON
endif

ifneq ($(high-bitdepth),)
CMAKE_OPTIONS += -DHIGH_BITDEPTH=ON
endif

ifneq ($(verbose),)
CMAKE_OPTIONS += -DCMAKE_VERBOSE_MAKEFILE=ON
endif

ifneq ($(enable-tracing),)
CONFIG_OPTIONS += -DSET_ENABLE_TRACING=ON -DENABLE_TRACING=$(enable-tracing)
endif

ifneq ($(static),)
CONFIG_OPTIONS += -DBUILD_STATIC=$(static)
endif

BUILD_OPTIONS := $(CONFIG_OPTIONS) -b

debug:
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug

all:
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug,release,relwithdebinfo

release:
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=release

relwithdebinfo:
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=relwithdebinfo

clean:
	#  clean is equal to realclean to ensure that CMake options are reset
	$(RM) -rf bin build lib
#	$(BUILD_CMD) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug,release,relwithdebinfo --target clean

clean-r:
	$(BUILD_CMD) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=release --target clean

clean-d:
	$(BUILD_CMD) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug --target clean

clean-p:
	$(BUILD_CMD) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=relwithdebinfo --target clean

configure:
	$(BUILD_CMD) $(CONFIG_OPTIONS) $(CMAKE_OPTIONS) variant=debug,release,relwithdebinfo

linuxbuild:
ifeq ($(LINUXBUILD),TRUE)
	# option for automated jenkins build
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug
endif

#
# project specific targets
#

# build the list of release, debug targets given the generic targets
TARGETS_ALL := $(foreach t,$(TARGETS),$(t)-a)
TARGETS_RELEASE := $(foreach t,$(TARGETS),$(t)-r)
TARGETS_DEBUG := $(foreach t,$(TARGETS),$(t)-d)
TARGETS_RELWITHDEBINFO := $(foreach t,$(TARGETS),$(t)-p)

TARGETS_ALL_CLEAN_FIRST := $(foreach t,$(TARGETS),$(t)-ca)
TARGETS_RELEASE_CLEAN_FIRST := $(foreach t,$(TARGETS),$(t)-cr)
TARGETS_DEBUG_CLEAN_FIRST := $(foreach t,$(TARGETS),$(t)-cd)
TARGETS_RELWITHDEBINFO_CLEAN_FIRST := $(foreach t,$(TARGETS),$(t)-cp)

$(TARGETS_ALL):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug,release,relwithdebinfo --target $(patsubst %-a,%,$@)

$(TARGETS_ALL_CLEAN_FIRST):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug,release,relwithdebinfo --clean-first --target $(patsubst %-ca,%,$@)

$(TARGETS_RELEASE):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=release --target $(patsubst %-r,%,$@)

$(TARGETS_RELEASE_CLEAN_FIRST):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=release --clean-first --target $(patsubst %-cr,%,$@)

$(TARGETS_DEBUG):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug --target $(patsubst %-d,%,$@)

$(TARGETS_DEBUG_CLEAN_FIRST):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=debug --target $(patsubst %-cd,%,$@) --clean-first

$(TARGETS_RELWITHDEBINFO):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=relwithdebinfo --target $(patsubst %-p,%,$@)

$(TARGETS_RELWITHDEBINFO_CLEAN_FIRST):
	$(BUILD_CMD) $(BUILD_JOBS) $(BUILD_OPTIONS) $(CMAKE_OPTIONS) variant=relwithdebinfo --target $(patsubst %-cp,%,$@) --clean-first

realclean:
	$(RM) -rf bin build lib

.NOTPARALLEL:
