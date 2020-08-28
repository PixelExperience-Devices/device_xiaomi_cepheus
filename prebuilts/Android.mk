LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE        := MatchmakerPrebuilt
LOCAL_MODULE_TAGS   := optional
LOCAL_SRC_FILES     := product/priv-app/MatchmakerPrebuilt/MatchmakerPrebuilt.apk
LOCAL_CERTIFICATE   := PRESIGNED
LOCAL_MODULE_CLASS  := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_DEX_PREOPT    := false
LOCAL_PRODUCT_MODULE := true
LOCAL_PRIVILEGED_MODULE := true
include $(BUILD_PREBUILT)
