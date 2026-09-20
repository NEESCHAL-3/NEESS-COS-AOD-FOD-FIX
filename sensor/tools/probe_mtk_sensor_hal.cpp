#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

struct hw_module_t;
struct hw_device_t;

struct hw_module_methods_t {
    int (*open)(const hw_module_t* module,
                const char* id,
                hw_device_t** device);
};

struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
    uint16_t hal_api_version;
    const char* id;
    const char* name;
    const char* author;
    hw_module_methods_t* methods;
    void* dso;
    uint64_t reserved[32 - 7];
};

struct hw_device_t {
    uint32_t tag;
    uint32_t version;
    hw_module_t* module;
    uint64_t reserved[12];
    int (*close)(hw_device_t* device);
};

struct sensor_t {
    const char* name;
    const char* vendor;
    int32_t version;
    int32_t handle;
    int32_t type;
    float maxRange;
    float resolution;
    float power;
    int32_t minDelay;
    uint32_t fifoReservedEventCount;
    uint32_t fifoMaxEventCount;
    const char* stringType;
    const char* requiredPermission;
    int32_t maxDelay;
    uint32_t flags;
    void* reserved[2];
};

struct sensors_module_t {
    hw_module_t common;

    int (*get_sensors_list)(
        sensors_module_t* module,
        const sensor_t** list);

    int (*set_operation_mode)(unsigned int mode);
};

int main() {
    const char* path =
        "/vendor/lib64/hw/sensors.mediatek.V2.0.so";

    void* h = dlopen(path, RTLD_NOW);

    if (!h) {
        printf("dlopen failed: %s\n", dlerror());
        return 1;
    }

    auto* mod = reinterpret_cast<sensors_module_t*>(
        dlsym(h, "HMI")
    );

    if (!mod) {
        printf("HMI missing: %s\n", dlerror());
        return 2;
    }

    printf("===== HMI =====\n");
    printf("tag                = 0x%08x\n",
           mod->common.tag);
    printf("module_api_version = 0x%04x\n",
           mod->common.module_api_version);
    printf("hal_api_version    = 0x%04x\n",
           mod->common.hal_api_version);

    printf("id                 = %s\n",
           mod->common.id ?: "(null)");
    printf("name               = %s\n",
           mod->common.name ?: "(null)");
    printf("author             = %s\n",
           mod->common.author ?: "(null)");

    printf("methods            = %p\n",
           mod->common.methods);
    printf("get_sensors_list   = %p\n",
           reinterpret_cast<void*>(
               mod->get_sensors_list
           ));

    if (!mod->get_sensors_list) {
        printf("ERROR: get_sensors_list NULL\n");
        return 3;
    }

    const sensor_t* list = nullptr;

    int count =
        mod->get_sensors_list(
            mod,
            &list
        );

    printf("\n===== SENSOR LIST =====\n");
    printf("count=%d list=%p\n",
           count,
           list);

    if (count <= 0 || !list)
        return 4;

    for (int i = 0; i < count; ++i) {
        const sensor_t& s = list[i];

        if (s.type == 22 ||
            s.type == 33171036 ||
            s.type == 33171030 ||
            s.type == 33171029 ||
            s.type == 33171027 ||
            s.type == 65611) {

            printf(
                "[%d] handle=%d type=%d "
                "name=\"%s\" vendor=\"%s\" "
                "flags=0x%x minDelay=%d maxDelay=%d\n",
                i,
                s.handle,
                s.type,
                s.name ?: "(null)",
                s.vendor ?: "(null)",
                s.flags,
                s.minDelay,
                s.maxDelay
            );
        }
    }

    printf("\n===== OPEN DEVICE =====\n");

    if (!mod->common.methods ||
        !mod->common.methods->open) {
        printf("ERROR: open missing\n");
        return 5;
    }

    hw_device_t* dev = nullptr;

    int ret =
        mod->common.methods->open(
            &mod->common,
            "poll",
            &dev
        );

    printf("open ret=%d dev=%p\n",
           ret,
           dev);

    if (ret == 0 && dev) {
        printf("device tag     = 0x%08x\n",
               dev->tag);
        printf("device version = 0x%08x\n",
               dev->version);
        printf("device module  = %p\n",
               dev->module);

        if (dev->close) {
            int cr = dev->close(dev);
            printf("close ret=%d\n", cr);
        }
    }

    return 0;
}
