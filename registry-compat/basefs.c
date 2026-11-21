/* File-system device selection for Wine objects. SPDX-License-Identifier: MIT */
#include <ntifs.h>
#include <stddef.h>

/* Also document the x64 DDK layout used by the executable overlay. */
_Static_assert(offsetof(FILE_OBJECT, DeviceObject) == 8, "FILE_OBJECT device layout");
_Static_assert(offsetof(FILE_OBJECT, Vpb) == 16, "FILE_OBJECT VPB layout");
_Static_assert(offsetof(FILE_OBJECT, Flags) == 80, "FILE_OBJECT flags layout");
_Static_assert(offsetof(DEVICE_OBJECT, Vpb) == 56, "DEVICE_OBJECT VPB layout");
_Static_assert(offsetof(VPB, DeviceObject) == 8, "VPB device layout");

PDEVICE_OBJECT NTAPI BaseFsDeviceObject(PFILE_OBJECT file)
{
    PDEVICE_OBJECT device = file->DeviceObject;

    if (file->Vpb && file->Vpb->DeviceObject)
        return file->Vpb->DeviceObject;
    if (device && !(file->Flags & FO_DIRECT_DEVICE_OPEN)
        && device->Vpb && device->Vpb->DeviceObject)
        return device->Vpb->DeviceObject;
    return device;
}
