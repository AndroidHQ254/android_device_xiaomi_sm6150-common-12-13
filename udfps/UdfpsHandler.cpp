/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHander.xiaomi_sm6150"

#include "UdfpsHandler.h"
#include "UdfpsHandlersm6150.h"

#include <android-base/logging.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <unistd.h>

#define COMMAND_NIT 10
#define PARAM_NIT_FOD 1
#define PARAM_NIT_NONE 0

#define FOD_STATUS_ON 1
#define FOD_STATUS_OFF -1

#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define Touch_Fod_Enable 10
#define TOUCH_MAGIC 0x5400
#define TOUCH_IOC_SETMODE TOUCH_MAGIC + 0

#define FOD_UI_PATH "/sys/devices/platform/soc/soc:qcom,dsi-display/fod_ui"

static bool readBool(int fd) {
    char c;
    int rc;

    rc = lseek(fd, 0, SEEK_SET);
    if (rc) {
        LOG(ERROR) << "failed to seek fd, err: " << rc;
        return false;
    }

    rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        LOG(ERROR) << "failed to read bool from fd, err: " << rc;
        return false;
    }

    return c != '0';
}

class XiaomiUdfpsHander : public UdfpsHandler {
  public:
    void init(fingerprint_device_t *device) {
        mDevice = device;

        touch_fd_ = android::base::unique_fd(open(TOUCH_DEV_PATH, O_RDWR));

        std::thread([this]() {
            int fd = open(FOD_UI_PATH, O_RDONLY);
            if (fd < 0) {
                LOG(ERROR) << "failed to open fd, err: " << fd;
                return;
            }

            struct pollfd fodUiPoll = {
                    .fd = fd,
                    .events = POLLERR | POLLPRI,
                    .revents = 0,
            };

            while (true) {
                int rc = poll(&fodUiPoll, 1, -1);
                if (rc < 0) {
                    LOG(ERROR) << "failed to poll fd, err: " << rc;
                    continue;
                }

                bool fingerDown = readBool(fd);
                LOG(INFO) << "fod_ui status: %d" << fingerDown;
                mDevice->extCmd(mDevice, COMMAND_NIT, fingerDown ? PARAM_NIT_FOD : PARAM_NIT_NONE);
                if (!fingerDown) {
                    int arg[2] = {Touch_Fod_Enable, FOD_STATUS_OFF};
                    ioctl(touch_fd_.get(), TOUCH_IOC_SETMODE, &arg);
                }
            }
        }).detach();
    }
/**
 * Notifies about a touch occurring within the under-display fingerprint
 * sensor area.
 *
 * It it assumed that the device can only have one active under-display
 * fingerprint sensor at a time.
 *
 * If multiple fingers are detected within the sensor area, only the
 * chronologically first event will be reported.
 *
 * @param x The screen x-coordinate of the center of the touch contact area, in
 * display pixels.
 * @param y The screen y-coordinate of the center of the touch contact area, in
 * display pixels.
 * @param minor The length of the minor axis of an ellipse that describes the
 * touch area, in display pixels.
 * @param major The length of the major axis of an ellipse that describes the
 * touch area, in display pixels.
 */
    void onFingerDown(uint32_t /*x*/, uint32_t /*y*/, float /*minor*/, float /*major*/) {
        int arg[2] = {Touch_Fod_Enable, FOD_STATUS_ON};
        ioctl(touch_fd_.get(), TOUCH_IOC_SETMODE, &arg);
    }
/**
 * Notifies about a finger leaving the under-display fingerprint sensor area.
 *
 * It it assumed that the device can only have one active under-display
 * fingerprint sensor at a time.
 *
 * If multiple fingers have left the sensor area, only the finger which
 * previously caused a "finger down" event will be reported.
 */
    void onFingerUp() {
        // nothing
    }
  private:
    fingerprint_device_t *mDevice;
};

static UdfpsHandler* create() {
    return new XiaomiUdfpsHander();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
    .create = create,
    .destroy = destroy,
};
