#include <android/sensor.h>
#include <android/looper.h>
#include <stdio.h>

struct Target {
    int type;
    const char *label;
    const ASensor *sensor;
};

int main() {
    ASensorManager *sm = ASensorManager_getInstance();
    if (!sm) {
        printf("SensorManager failed\n");
        return 1;
    }

    Target t[] = {
        {22,       "tilt",        nullptr},
        {33171029, "AodWakeup",   nullptr},
        {33171030, "FodWakeup",   nullptr},
        {33171036, "PickupWakeup",nullptr},
        {33171027, "NonUiWakeup", nullptr},
    };

    ALooper *looper =
        ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    ASensorEventQueue *q =
        ASensorManager_createEventQueue(sm, looper, 1, nullptr, nullptr);

    for (auto &x : t) {
        x.sensor = ASensorManager_getDefaultSensor(sm, x.type);

        if (!x.sensor) {
            printf("%-12s type=%d : NOT FOUND\n",
                   x.label, x.type);
            continue;
        }

        printf("%-12s type=%d name=\"%s\" vendor=\"%s\"\n",
               x.label,
               x.type,
               ASensor_getName(x.sensor),
               ASensor_getVendor(x.sensor));

        int r = ASensorEventQueue_enableSensor(q, x.sensor);
        printf("  enable=%d\n", r);
    }

    printf("\n===== LOCK PHONE / PICK UP / TAP / TOUCH FOD =====\n");
    fflush(stdout);

    while (1) {
        ALooper_pollOnce(1000, nullptr, nullptr, nullptr);

        ASensorEvent e;
        while (ASensorEventQueue_getEvents(q, &e, 1) > 0) {
            printf("EVENT type=%d  v0=%f v1=%f v2=%f\n",
                   e.type,
                   e.data[0],
                   e.data[1],
                   e.data[2]);
            fflush(stdout);
        }
    }
}
