#include "common_utils.hpp"

#include <ctime>
#include <cpuid.h>
#include <util/c99defs.h>
#include <util/dstr.h>
#include <va/va_drm.h>
#include <va/va_str.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <dirent.h>

#include <mfxvideo++.h>
#include <mfxdispatcher.h>

#define INTEL_VENDOR_ID 0x8086

mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
		       mfxFrameAllocResponse *response)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(request);
	UNUSED_PARAMETER(response);
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(mid);
	UNUSED_PARAMETER(ptr);
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(mid);
	UNUSED_PARAMETER(ptr);
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(mid);
	UNUSED_PARAMETER(handle);
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(response);
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
			 mfxU64 lock_key, mfxU64 *next_key)
{
	UNUSED_PARAMETER(pthis);
	UNUSED_PARAMETER(mid);
	UNUSED_PARAMETER(tex_handle);
	UNUSED_PARAMETER(lock_key);
	UNUSED_PARAMETER(next_key);
	return MFX_ERR_UNSUPPORTED;
}

#if 0
void ClearYUVSurfaceVMem(mfxMemId memId);
void ClearRGBSurfaceVMem(mfxMemId memId);
#endif

// Release resources (device/display)
void Release(){};

void mfxGetTime(mfxTime *timestamp)
{
	clock_gettime(CLOCK_MONOTONIC, timestamp);
}

double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
	UNUSED_PARAMETER(tfinish);
	UNUSED_PARAMETER(tstart);
	//TODO, unused so far it seems
	return 0.0;
}

extern "C" void util_cpuid(int cpuinfo[4], int level)
{
	__get_cpuid(level, (unsigned int *)&cpuinfo[0],
		    (unsigned int *)&cpuinfo[1], (unsigned int *)&cpuinfo[2],
		    (unsigned int *)&cpuinfo[3]);
}

struct vaapi_device {
	int fd;
	VADisplay display;
	const char *driver;
};

void vaapi_open(char *device_path, struct vaapi_device *device)
{
	int fd = open(device_path, O_RDWR);
	if (fd < 0) {
		return;
	}

	VADisplay display = vaGetDisplayDRM(fd);
	if (!display) {
		close(fd);
		return;
	}

	// VA-API is noisy by default.
	vaSetInfoCallback(display, nullptr, nullptr);
	vaSetErrorCallback(display, nullptr, nullptr);

	int major;
	int minor;
	if (vaInitialize(display, &major, &minor) != VA_STATUS_SUCCESS) {
		vaTerminate(display);
		close(fd);
		return;
	}

	const char *driver = vaQueryVendorString(display);
	if (strstr(driver, "Intel i965 driver") != nullptr) {
		blog(LOG_WARNING,
		     "Legacy intel-vaapi-driver detected, incompatible with QSV");
		vaTerminate(display);
		close(fd);
		return;
	}

	device->fd = fd;
	device->display = display;
	device->driver = driver;
}

void vaapi_close(struct vaapi_device *device)
{
	vaTerminate(device->display);
	close(device->fd);
}

static uint32_t vaapi_check_support(VADisplay display, VAProfile profile,
				    VAEntrypoint entrypoint)
{
	bool ret = false;
	VAConfigAttrib attrib[1];
	attrib->type = VAConfigAttribRateControl;

	VAStatus va_status =
		vaGetConfigAttributes(display, profile, entrypoint, attrib, 1);

	uint32_t rc = 0;
	switch (va_status) {
	case VA_STATUS_SUCCESS:
		rc = attrib->value;
		break;
	case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
	case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
	default:
		break;
	}

	return (rc & VA_RC_CBR || rc & VA_RC_CQP || rc & VA_RC_VBR);
}

bool vaapi_supports_h264(VADisplay display)
{
	bool ret = false;
	ret |= vaapi_check_support(display, VAProfileH264ConstrainedBaseline,
				   VAEntrypointEncSlice);
	ret |= vaapi_check_support(display, VAProfileH264Main,
				   VAEntrypointEncSlice);
	ret |= vaapi_check_support(display, VAProfileH264High,
				   VAEntrypointEncSlice);

	if (!ret) {
		ret |= vaapi_check_support(display,
					   VAProfileH264ConstrainedBaseline,
					   VAEntrypointEncSliceLP);
		ret |= vaapi_check_support(display, VAProfileH264Main,
					   VAEntrypointEncSliceLP);
		ret |= vaapi_check_support(display, VAProfileH264High,
					   VAEntrypointEncSliceLP);
	}

	return ret;
}

bool vaapi_supports_av1(VADisplay display)
{
	bool ret = false;
	// Are there any devices with non-LowPower entrypoints?
	ret |= vaapi_check_support(display, VAProfileAV1Profile0,
				   VAEntrypointEncSlice);
	ret |= vaapi_check_support(display, VAProfileAV1Profile0,
				   VAEntrypointEncSliceLP);
	return ret;
}

bool vaapi_supports_hevc(VADisplay display)
{
	bool ret = false;
	ret |= vaapi_check_support(display, VAProfileHEVCMain,
				   VAEntrypointEncSlice);
	ret |= vaapi_check_support(display, VAProfileHEVCMain,
				   VAEntrypointEncSliceLP);
	return ret;
}

bool vaapi_supports_vp9(VADisplay display)
{
	bool ret = false;
	ret |= vaapi_check_support(display, VAProfileVP9Profile0,
				   VAEntrypointEncSlice);
	ret |= vaapi_check_support(display, VAProfileVP9Profile0,
				   VAEntrypointEncSliceLP);
	return ret;
}

void check_adapters(struct adapter_info *adapters, size_t *adapter_count)
{
	dstr full_path {};
	dirent **namelist;
	int no;
	int adapter_idx;
	const char *base_dir = "/dev/dri/";

	dstr_init(&full_path);
	if ((no = scandir(base_dir, &namelist, nullptr, alphasort)) > 0) {
		for (int i = 0; i < no; i++) {
			struct adapter_info *adapter;
			struct dirent *dp;
			struct vaapi_device device = {0};

			dp = namelist[i];
			if (strstr(dp->d_name, "renderD") == nullptr)
				goto next_entry;

			adapter_idx = atoi(&dp->d_name[7]) - 128;
			if (adapter_idx >= (ssize_t)*adapter_count ||
			    adapter_idx < 0)
				goto next_entry;

			*adapter_count = adapter_idx + 1;
			dstr_copy(&full_path, base_dir);
			dstr_cat(&full_path, dp->d_name);
			vaapi_open(full_path.array, &device);
			if (!device.display)
				goto next_entry;

			adapter = &adapters[adapter_idx];
			adapter->is_intel = strstr(device.driver, "Intel") !=
					    nullptr;
			// Apparently dgpu is broken?
			adapter->is_dgpu = false;
			adapter->supports_av1 =
				vaapi_supports_av1(device.display);
			adapter->supports_hevc =
				vaapi_supports_hevc(device.display);
			adapter->supports_vp9 =
				vaapi_supports_vp9(device.display);
			vaapi_close(&device);

		next_entry:
			free(dp);
		}
		free(namelist);
	}
	dstr_free(&full_path);
}
