#include <ntifs.h>
#include <string.h>

int run_device_cases(void *function, unsigned int *count)
{
    PDEVICE_OBJECT (NTAPI *get_device)(PFILE_OBJECT) = function;
    FILE_OBJECT file;
    DEVICE_OBJECT base, upper, fs_device, other_fs;
    VPB file_vpb, device_vpb;
    unsigned int failures = 0;

#define CHECK_DEVICE(expected) do { ++*count; if (get_device(&file) != (expected)) ++failures; } while (0)
    memset(&file, 0, sizeof(file));
    memset(&base, 0, sizeof(base));
    memset(&upper, 0, sizeof(upper));
    memset(&fs_device, 0, sizeof(fs_device));
    memset(&other_fs, 0, sizeof(other_fs));
    memset(&file_vpb, 0, sizeof(file_vpb));
    memset(&device_vpb, 0, sizeof(device_vpb));
    file.DeviceObject = &base;
    CHECK_DEVICE(&base);
    base.AttachedDevice = &upper;
    CHECK_DEVICE(&base);  /* This API returns the base, not an attached filter. */
    base.Vpb = &device_vpb;
    device_vpb.DeviceObject = &fs_device;
    CHECK_DEVICE(&fs_device);
    file.Vpb = &file_vpb;
    CHECK_DEVICE(&fs_device);
    file_vpb.DeviceObject = &other_fs;
    CHECK_DEVICE(&other_fs);
    file.Flags = FO_DIRECT_DEVICE_OPEN;
    CHECK_DEVICE(&other_fs);
    file_vpb.DeviceObject = NULL;
    CHECK_DEVICE(&base);
    file.Flags = 0;
    device_vpb.DeviceObject = NULL;
    CHECK_DEVICE(&base);
    file.DeviceObject = NULL;
    CHECK_DEVICE(NULL);  /* Wine file objects can currently have no device. */
    return failures;
}
