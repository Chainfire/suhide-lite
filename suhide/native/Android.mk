LOCAL_PATH:= $(call my-dir)

FLAGS := -std=c11
LDLIBS := -ldl -llog

#FLAGS += -DDEBUG

include $(CLEAR_VARS)

LOCAL_SRC_FILES := util.c trace.c config.c suhide.c

LOCAL_MODULE := suhide64
LOG_TAG := suhide64
ifneq ($(TARGET_ARCH_ABI),arm64-v8a)
ifneq ($(TARGET_ARCH_ABI),x86_64)
ifneq ($(TARGET_ARCH_ABI),mips64)
    LOCAL_MODULE := suhide32
    LOG_TAG := suhide32
endif
endif
endif

LOCAL_CFLAGS := $(FLAGS) -DLOG_TAG=\"$(LOG_TAG)\"
LOCAL_LDLIBS := $(LDLIBS)

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := util.c getevent.c suhide_launcher.c

LOCAL_MODULE := suhide
LOG_TAG := suhide

LOCAL_CFLAGS := $(FLAGS) -DLOG_TAG=\"$(LOG_TAG)\"
LOCAL_LDLIBS := $(LDLIBS)

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    setpropex/setpropex.c \
    setpropex/system_properties.c \
    setpropex/system_properties_compat.c
LOCAL_MODULE := setpropex
LOCAL_CFLAGS += -std=c99 -I setpropex/inc -DNDEBUG
LOCAL_LDLIBS := -llog -pie -fPIE

include $(BUILD_EXECUTABLE)
