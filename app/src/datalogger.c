#include <zephyr.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(datalogger, LOG_LEVEL_DBG);

#include "datalogger.h"

#include <disk_access.h>
#include <fs.h>
#include <ff.h>
#include <stdio.h>
#include "global.h"

static const char *disk_mount_pt = "/SD:";
static int lsdir(const char *path);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

static struct fs_file_t zfp;

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]", log_strdup(path), res);
		return res;
	}

	LOG_INF("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			LOG_INF("[DIR ] %s", log_strdup(entry.name));
		} else {
			LOG_INF("[FILE] %s (size = %zu)",
				log_strdup(entry.name), entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int datalogger_init(void)
{
	int ret;
	char filename[50];
	const char *disk_pdrv = "SD";

	if (disk_access_init(disk_pdrv) != 0) {
		LOG_WRN("No SD card found");
		return EIO;
	}

	mp.mnt_point = disk_mount_pt;
	ret = fs_mount(&mp);

	if (ret == FR_OK) {
		LOG_DBG("Disk mounted.\n");
		//lsdir(disk_mount_pt);
	} else {
		LOG_ERR("Error mounting disk.\n");
		return EIO;
	}

	snprintf(filename, sizeof(filename), "%s/%"PRIu32".log", disk_mount_pt, global.config.boot_count);
	LOG_DBG("filename:%s", log_strdup(filename));

	ret = fs_open(&zfp, filename);
	if (ret != 0)
	{
		LOG_ERR("fs_open failed");
		return EIO;
	}

	const char *firstline = "#datalogger first line\n";
	ret = fs_write(&zfp, firstline, strlen(firstline));
	if (ret <= 0)
	{
		LOG_ERR("fs_write failed");
		return EIO;
	}

	ret = lsdir(disk_mount_pt);
	if (ret != 0)
	{
		LOG_ERR("fs_close failed");
		return EIO;
	}

	return 0;
}

int datalogger_nprintf(size_t n, const char *fmt, ...)
{
	char buffer[n];
	int ret;

	va_list	args;
	va_start(args, fmt);
	ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (ret == n)
	{
		LOG_WRN("no space left in buffer");
	}

	ret = fs_write(&zfp, buffer, ret);
	if (ret < 0)
	{
		LOG_ERR("fs_write failed");
		return ret;
	}

	return ret;
}

int datalogger_sync(void)
{
	int ret;

	ret = fs_sync(&zfp);
	if (ret != 0)
	{
		LOG_ERR("fs_unmount failed");
		return EIO;
	}

	return 0;
}

int datalogger_close(void)
{
	int ret;

	ret = fs_close(&zfp);
	if (ret != 0)
	{
		LOG_ERR("fs_close failed");
		return EIO;
	}

	ret = fs_unmount(&mp);
	if (ret != 0)
	{
		LOG_ERR("fs_unmount failed");
		return EIO;
	}

	return 0;
}
