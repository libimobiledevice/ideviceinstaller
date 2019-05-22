/*
 * ideviceinstaller - Manage apps on iOS devices.
 *
 * Copyright (C) 2010-2023 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2010-2015 Martin Szulecki <m.szulecki@libimobiledevice.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef WIN32
#include <signal.h>
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/notification_proxy.h>
#include <libimobiledevice/afc.h>

#include <plist/plist.h>

#include <zip.h>

#ifdef WIN32
#include <windows.h>
#define wait_ms(x) Sleep(x)
#else
#define wait_ms(x) { struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = x * 1000000; nanosleep(&ts, NULL); }
#endif

#ifndef HAVE_VASPRINTF
static int vasprintf(char **PTR, const char *TEMPLATE, va_list AP)
{
	int res;
	char buf[16];
	res = vsnprintf(buf, 16, TEMPLATE, AP);
	if (res > 0) {
		*PTR = (char*)malloc(res+1);
		res = vsnprintf(*PTR, res+1, TEMPLATE, AP);
	}
	return res;
}
#endif

#ifndef HAVE_ASPRINTF
static int asprintf(char **PTR, const char *TEMPLATE, ...)
{
	int res;
	va_list AP;
	va_start(AP, TEMPLATE);
	res = vasprintf(PTR, TEMPLATE, AP);
	va_end(AP);
	return res;
}
#endif

#define ITUNES_METADATA_PLIST_FILENAME "iTunesMetadata.plist"

const char PKG_PATH[] = "PublicStaging";
const char APPARCH_PATH[] = "ApplicationArchives";

char *udid = NULL;
char *cmdarg = NULL;
char *extsinf = NULL;
char *extmeta = NULL;

enum cmd_mode {
	CMD_NONE = 0,
	CMD_LIST_APPS,
	CMD_INSTALL,
	CMD_UNINSTALL,
	CMD_UPGRADE,
	CMD_LIST_ARCHIVES,
	CMD_ARCHIVE,
	CMD_RESTORE,
	CMD_REMOVE_ARCHIVE
};

int cmd = CMD_NONE;

char *last_status = NULL;
int wait_for_command_complete = 0;
int use_network = 0;
int use_notifier = 0;
int notification_expected = 0;
int is_device_connected = 0;
int command_completed = 0;
int ignore_events = 0;
int err_occurred = 0;
int notified = 0;
plist_t bundle_ids = NULL;
plist_t return_attrs = NULL;
#define FORMAT_XML 1
#define FORMAT_JSON 2
int output_format = 0;
int opt_list_user = 0;
int opt_list_system = 0;
char *copy_path = NULL;
int remove_after_copy = 0;
int skip_uninstall = 1;
int app_only = 0;
int docs_only = 0;

static void print_apps_header()
{
	if (!return_attrs) {
		return;
	}
	uint32_t i = 0;
	for (i = 0; i < plist_array_get_size(return_attrs); i++) {
		plist_t node = plist_array_get_item(return_attrs, i);
		if (i > 0) {
			printf(", ");
		}
		printf("%s", plist_get_string_ptr(node, NULL));
	}
	printf("\n");
}

static void print_apps(plist_t apps)
{
	if (!return_attrs) {
		return;
	}
	uint32_t i = 0;
	for (i = 0; i < plist_array_get_size(apps); i++) {
		plist_t app = plist_array_get_item(apps, i);
		uint32_t j = 0;
		for (j = 0; j < plist_array_get_size(return_attrs); j++) {
			plist_t node = plist_array_get_item(return_attrs, j);
			if (j > 0) {
				printf(", ");
			}
			const char* key = plist_get_string_ptr(node, NULL);
			node = plist_dict_get_item(app, key);
			if (node) {
				if (!strcmp(key, "CFBundleIdentifier")) {
					printf("%s", plist_get_string_ptr(node, NULL));
				} else {
					uint64_t uval = 0;
					switch (plist_get_node_type(node)) {
						case PLIST_STRING:
							printf("\"%s\"", plist_get_string_ptr(node, NULL));
							break;
						case PLIST_INT:
							plist_get_uint_val(node, &uval);
							printf("%" PRIu64, uval);
							break;
						case PLIST_BOOLEAN:
							printf("%s", plist_bool_val_is_true(node) ? "true" : "false");
							break;
						case PLIST_ARRAY:
							printf("(array)");
							break;
						case PLIST_DICT:
							printf("(dict)");
							break;
						default:
							break;
					}
				}
			}
		}
		printf("\n");
	}
}

static void notifier(const char *notification, void *unused)
{
	notified = 1;
}

static void status_cb(plist_t command, plist_t status, void *unused)
{
	if (command && status) {
		char* command_name = NULL;
		instproxy_command_get_name(command, &command_name);

		/* get status */
		char *status_name = NULL;
		instproxy_status_get_name(status, &status_name);

		if (status_name) {
			if (!strcmp(status_name, "Complete")) {
				command_completed = 1;
			}
		}

		/* get error if any */
		char* error_name = NULL;
		char* error_description = NULL;
		uint64_t error_code = 0;
		instproxy_status_get_error(status, &error_name, &error_description, &error_code);

		/* output/handling */
		if (!error_name) {
			if (!strcmp(command_name, "Browse")) {
				uint64_t total = 0;
				uint64_t current_index = 0;
				uint64_t current_amount = 0;
				plist_t current_list = NULL;
				instproxy_status_get_current_list(status, &total, &current_index, &current_amount, &current_list);
				if (current_list) {
					print_apps(current_list);
					plist_free(current_list);
				}
			} else if (status_name) {
				/* get progress if any */
				int percent = -1;
				instproxy_status_get_percent_complete(status, &percent);

				if (last_status && (strcmp(last_status, status_name))) {
					printf("\n");
				}

				if (percent >= 0) {
					printf("\r%s: %s (%d%%)", command_name, status_name, percent);
				} else {
					printf("\r%s: %s", command_name, status_name);
				}
				if (command_completed) {
					printf("\n");
				}
			}
		} else {
			/* report error to the user */
			if (error_description)
				fprintf(stderr, "ERROR: %s failed. Got error \"%s\" with code 0x%08"PRIx64": %s\n", command_name, error_name, error_code, error_description ? error_description: "N/A");
			else
				fprintf(stderr, "ERROR: %s failed. Got error \"%s\".\n", command_name, error_name);
			err_occurred = 1;
		}

		/* clean up */
		free(error_name);
		free(error_description);

		free(last_status);
		last_status = status_name;

		free(command_name);
		command_name = NULL;
	} else {
		fprintf(stderr, "ERROR: %s was called with invalid arguments!\n", __func__);
	}
}

static int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len)
{
	struct zip_stat zs;
	struct zip_file *zfile;
	int zindex = zip_name_locate(zf, filename, locate_flags);

	*buffer = NULL;
	*len = 0;

	if (zindex < 0) {
		return -1;
	}

	zip_stat_init(&zs);

	if (zip_stat_index(zf, zindex, 0, &zs) != 0) {
		fprintf(stderr, "ERROR: zip_stat_index '%s' failed!\n", filename);
		return -2;
	}

	if (zs.size > 10485760) {
		fprintf(stderr, "ERROR: file '%s' is too large!\n", filename);
		return -3;
	}

	zfile = zip_fopen_index(zf, zindex, 0);
	if (!zfile) {
		fprintf(stderr, "ERROR: zip_fopen '%s' failed!\n", filename);
		return -4;
	}

	*buffer = malloc(zs.size);
	if (zs.size > LLONG_MAX || zip_fread(zfile, *buffer, zs.size) != (zip_int64_t)zs.size) {
		fprintf(stderr, "ERROR: zip_fread %" PRIu64 " bytes from '%s'\n", (uint64_t)zs.size, filename);
		free(*buffer);
		*buffer = NULL;
		zip_fclose(zfile);
		return -5;
	}
	*len = zs.size;
	zip_fclose(zfile);
	return 0;
}

static int zip_get_app_directory(struct zip* zf, char** path)
{
	zip_int64_t i = 0;
	zip_int64_t c = (zip_int64_t)zip_get_num_entries(zf, 0);
	int len = 0;
	const char* name = NULL;

	/* look through all filenames in the archive */
	do {
		/* get filename at current index */
		name = zip_get_name(zf, i++, 0);
		if (name != NULL) {
			/* check if we have a "Payload/.../" name */
			len = strlen(name);
			if (!strncmp(name, "Payload/", 8) && (len > 8)) {
				/* skip hidden files */
				if (name[8] == '.')
					continue;

				/* locate the second directory delimiter */
				const char* p = name + 8;
				do {
					if (*p == '/') {
						break;
					}
				} while(p++ != NULL);

				/* try next entry if not found */
				if (p == NULL)
					continue;

				len = p - name + 1;

				if (path != NULL) {
					free(*path);
					*path = NULL;
				}

				/* allocate and copy filename */
				*path = (char*)malloc(len + 1);
				strncpy(*path, name, len);

				/* add terminating null character */
				char* t = *path + len;
				*t = '\0';
				break;
			}
		}
	} while(i < c);

	if (*path == NULL) {
		return -1;
	}

	return 0;
}

static void idevice_event_callback(const idevice_event_t* event, void* userdata)
{
	if (ignore_events) {
		return;
	}
	if (event->event == IDEVICE_DEVICE_REMOVE) {
		if (!strcmp(udid, event->udid)) {
			fprintf(stderr, "ideviceinstaller: Device removed\n");
			is_device_connected = 0;
		}
	}
}

static void idevice_wait_for_command_to_complete()
{
	is_device_connected = 1;
	ignore_events = 0;

	/* subscribe to make sure to exit on device removal */
	idevice_event_subscribe(idevice_event_callback, NULL);

	/* wait for command to complete */
	while (wait_for_command_complete && !command_completed && !err_occurred
		   && is_device_connected) {
		wait_ms(50);
	}

	/* wait some time if a notification is expected */
	while (use_notifier && notification_expected && !notified && !err_occurred && is_device_connected) {
		wait_ms(50);
	}

	ignore_events = 1;
	idevice_event_unsubscribe();
}

static void print_usage(int argc, char **argv, int is_error)
{
	char *name = strrchr(argv[0], '/');
	fprintf((is_error) ? stderr : stdout, "Usage: %s OPTIONS\n", (name ? name + 1 : argv[0]));
	fprintf((is_error) ? stderr : stdout,
	"\n"
	"Manage apps on iOS devices.\n"
	"\n"
	"COMMANDS:\n"
	"  list                List installed apps. Options:\n"
	"        --user          List user apps only (this is the default)\n"
	"        --system        List system apps only\n"
	"        --all           List all types of apps\n"
	"        --xml           Print output as XML Property List\n"
	"        -a, --attribute ATTR  Specify attribute to return - see man page\n"
	"            (can be passed multiple times)\n"
	"        -b, --bundle-identifier BUNDLEID  Only query given bundle identifier\n"
	"            (can be passed multiple times)\n"
	"  install PATH        Install app from package file specified by PATH.\n"
	"                      PATH can also be a .ipcc file for carrier bundles.\n"
	"        -s, --sinf PATH  Pass an external SINF file\n"
	"        -m, --metadata PATH  Pass an external iTunesMetadata file\n"
	"  uninstall BUNDLEID  Uninstall app specified by BUNDLEID.\n"
	"  upgrade PATH        Upgrade app from package file specified by PATH.\n"
        "\n"
        "LEGACY COMMANDS (non-functional with iOS 7 or later):\n"
	"  archive BUNDLEID    Archive app specified by BUNDLEID. Options:\n"
	"        --uninstall     Uninstall the package after making an archive\n"
	"        --app-only      Archive application data only\n"
	"        --docs-only     Archive documents (user data) only\n"
	"        --copy=PATH     Copy the app archive to directory PATH when done\n"
	"        --remove        Only valid when copy=PATH is used: remove after copy\n"
	"  restore BUNDLEID    Restore archived app specified by BUNDLEID\n"
	"  list-archives       List archived apps. Options:\n"
	"        --xml           Print output as XML Property List\n"
	"  remove-archive BUNDLEID    Remove app archive specified by BUNDLEID\n"
	"\n"
	"OPTIONS:\n"
	"  -u, --udid UDID     Target specific device by UDID\n"
	"  -n, --network       Connect to network device\n"
	"  -w, --notify-wait   Wait for app installed/uninstalled notification\n"
	"                      before reporting success of operation\n"
	"  -h, --help          Print usage information\n"
	"  -d, --debug         Enable communication debugging\n"
	"  -v, --version       Print version information\n"
	"\n"
	"Homepage:    <" PACKAGE_URL ">\n"
	"Bug Reports: <" PACKAGE_BUGREPORT ">\n"
	);
}

enum numerical_opts {
	LIST_USER = 1,
	LIST_SYSTEM,
	LIST_ALL,
	ARCHIVE_UNINSTALL,
	ARCHIVE_APP_ONLY,
	ARCHIVE_DOCS_ONLY,
	ARCHIVE_COPY_PATH,
	ARCHIVE_COPY_REMOVE,
	OUTPUT_XML,
	OUTPUT_JSON
};

static void parse_opts(int argc, char **argv)
{
	static struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "udid", required_argument, NULL, 'u' },
		{ "network", no_argument, NULL, 'n' },
		{ "notify-wait", no_argument, NULL, 'w' },
		{ "debug", no_argument, NULL, 'd' },
		{ "version", no_argument, NULL, 'v' },
		{ "bundle-identifier", required_argument, NULL, 'b' },
		{ "attribute", required_argument, NULL, 'a' },
		{ "user", no_argument, NULL, LIST_USER },
		{ "system", no_argument, NULL, LIST_SYSTEM },
		{ "all", no_argument, NULL, LIST_ALL },
		{ "xml", no_argument, NULL, OUTPUT_XML },
		{ "json", no_argument, NULL, OUTPUT_JSON },
		{ "sinf", required_argument, NULL, 's' },
		{ "metadata", required_argument, NULL, 'm' },
		{ "uninstall", no_argument, NULL, ARCHIVE_UNINSTALL },
		{ "app-only", no_argument, NULL, ARCHIVE_APP_ONLY },
		{ "docs-only", no_argument, NULL, ARCHIVE_DOCS_ONLY },
		{ "copy", required_argument, NULL, ARCHIVE_COPY_PATH },
		{ "remove", no_argument, NULL, ARCHIVE_COPY_REMOVE },
		{ NULL, 0, NULL, 0 }
	};
	int c;

	while (1) {
		c = getopt_long(argc, argv, "hu:nwdvb:a:s:m:", longopts, (int*)0);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_usage(argc, argv, 0);
			exit(0);
		case 'u':
			if (!*optarg) {
				printf("ERROR: UDID must not be empty!\n");
				print_usage(argc, argv, 1);
				exit(2);
			}
			udid = strdup(optarg);
			break;
		case 'n':
			use_network = 1;
			break;
		case 'a':
			if (!*optarg) {
				printf("ERROR: attribute must not be empty!\n");
				print_usage(argc, argv, 1);
				exit(2);
			}
			if (return_attrs == NULL) {
				return_attrs = plist_new_array();
			}
			plist_array_append_item(return_attrs, plist_new_string(optarg));
			break;
		case 'b':
			if (!*optarg) {
				printf("ERROR: bundle identifier must not be empty!\n");
				print_usage(argc, argv, 1);
				exit(2);
			}
			if (bundle_ids == NULL) {
				bundle_ids = plist_new_array();
			}
			plist_array_append_item(bundle_ids, plist_new_string(optarg));
			break;
		case 's':
			if (!*optarg) {
				printf("ERROR: path for --sinf must not be empty!\n");
				print_usage(argc, argv, 1);
				exit(2);
			}
			extsinf = strdup(optarg);
			break;
		case 'm':
			if (!*optarg) {
				printf("ERROR: path for --metadata must not be empty!\n");
				print_usage(argc, argv, 1);
				exit(2);
			}
			extmeta = strdup(optarg);
			break;
		case 'w':
			use_notifier = 1;
			break;
		case 'd':
			idevice_set_debug_level(1);
			break;
		case 'v':
			printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
			exit(0);
		case LIST_USER:
			opt_list_user = 1;
			break;
		case LIST_SYSTEM:
			opt_list_system = 1;
			break;
		case LIST_ALL:
			opt_list_user = 1;
			opt_list_system = 1;
			break;
		case OUTPUT_XML:
			output_format = FORMAT_XML;
			break;
		case OUTPUT_JSON:
			output_format = FORMAT_JSON;
			break;
		case ARCHIVE_UNINSTALL:
			skip_uninstall = 0;
			break;
		case ARCHIVE_APP_ONLY:
			app_only = 1;
			docs_only = 0;
			break;
		case ARCHIVE_DOCS_ONLY:
			docs_only = 1;
			app_only = 0;
			break;
		case ARCHIVE_COPY_PATH:
			copy_path = strdup(optarg);
			break;
		case ARCHIVE_COPY_REMOVE:
			remove_after_copy = 1;
			break;
		default:
			print_usage(argc, argv, 1);
			exit(2);
		}
	}

        argv += optind;
	argc -= optind;

	if (argc == 0) {
		fprintf(stderr, "ERROR: Missing command.\n\n");
		print_usage(argc+optind, argv-optind, 1);
		exit(2);
	}

	char *cmdstr = argv[0];

	if (!strcmp(cmdstr, "list")) {
		cmd = CMD_LIST_APPS;
	} else if (!strcmp(cmdstr, "install")) {
		cmd = CMD_INSTALL;
	} else if (!strcmp(cmdstr, "upgrade")) {
		cmd = CMD_UPGRADE;
	} else if (!strcmp(cmdstr, "uninstall") || !strcmp(cmdstr, "remove")) {
		cmd = CMD_UNINSTALL;
	} else if (!strcmp(cmdstr, "archives") || !strcmp(cmdstr, "list-archives")) {
		cmd = CMD_LIST_ARCHIVES;
	} else if (!strcmp(cmdstr, "archive")) {
		cmd = CMD_ARCHIVE;
	} else if (!strcmp(cmdstr, "restore")) {
		cmd = CMD_RESTORE;
	} else if (!strcmp(cmdstr, "remove-archive")) {
		cmd = CMD_REMOVE_ARCHIVE;
	}

	switch (cmd) {
		case CMD_LIST_APPS:
		case CMD_LIST_ARCHIVES:
			break;
		case CMD_INSTALL:
		case CMD_UPGRADE:
			if (argc < 2) {
				fprintf(stderr, "ERROR: Missing filename for '%s' command.\n\n", cmdstr);
				print_usage(argc+optind, argv-optind, 1);
				exit(2);
			}
			cmdarg = argv[1];
			break;
		case CMD_UNINSTALL:
		case CMD_ARCHIVE:
		case CMD_RESTORE:
		case CMD_REMOVE_ARCHIVE:
			if (argc < 2) {
				fprintf(stderr, "ERROR: Missing bundle ID for '%s' command.\n\n", cmdstr);
				print_usage(argc+optind, argv-optind, 1);
				exit(2);
			}
			cmdarg = argv[1];
			break;
		default:
			fprintf(stderr, "ERROR: Invalid command '%s'.\n\n", cmdstr);
			print_usage(argc+optind, argv-optind, 1);
			exit(2);
	}
}

static int afc_upload_file(afc_client_t afc, const char* filename, const char* dstfn)
{
	FILE *f = NULL;
	uint64_t af = 0;
	char buf[1048576];

	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return -1;
	}

	if ((afc_file_open(afc, dstfn, AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
		fclose(f);
		fprintf(stderr, "afc_file_open on '%s' failed!\n", dstfn);
		return -1;
	}

	size_t amount = 0;
	do {
		amount = fread(buf, 1, sizeof(buf), f);
		if (amount > 0) {
			uint32_t written, total = 0;
			while (total < amount) {
				written = 0;
				afc_error_t aerr = afc_file_write(afc, af, buf, amount, &written);
				if (aerr != AFC_E_SUCCESS) {
					fprintf(stderr, "AFC Write error: %d\n", aerr);
					break;
				}
				total += written;
			}
			if (total != amount) {
				fprintf(stderr, "Error: wrote only %u of %u\n", total, (uint32_t)amount);
				afc_file_close(afc, af);
				fclose(f);
				return -1;
			}
		}
	} while (amount > 0);

	afc_file_close(afc, af);
	fclose(f);

	return 0;
}

static void afc_upload_dir(afc_client_t afc, const char* path, const char* afcpath)
{
	afc_make_directory(afc, afcpath);

	DIR *dir = opendir(path);
	if (dir) {
		struct dirent* ep;
		while ((ep = readdir(dir))) {
			if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0)) {
				continue;
			}
			char *fpath = (char*)malloc(strlen(path)+1+strlen(ep->d_name)+1);
			char *apath = (char*)malloc(strlen(afcpath)+1+strlen(ep->d_name)+1);

			struct stat st;

			strcpy(fpath, path);
			strcat(fpath, "/");
			strcat(fpath, ep->d_name);

			strcpy(apath, afcpath);
			strcat(apath, "/");
			strcat(apath, ep->d_name);

#ifdef HAVE_LSTAT
			if ((lstat(fpath, &st) == 0) && S_ISLNK(st.st_mode)) {
				char *target = (char *)malloc(st.st_size+1);
				if (readlink(fpath, target, st.st_size+1) < 0) {
					fprintf(stderr, "ERROR: readlink: %s (%d)\n", strerror(errno), errno);
				} else {
					target[st.st_size] = '\0';
					afc_make_link(afc, AFC_SYMLINK, target, fpath);
				}
				free(target);
			} else
#endif
			if ((stat(fpath, &st) == 0) && S_ISDIR(st.st_mode)) {
				afc_upload_dir(afc, fpath, apath);
			} else {
				afc_upload_file(afc, fpath, apath);
			}
			free(fpath);
			free(apath);
		}
		closedir(dir);
	}
}

static char *buf_from_file(const char *filename, size_t *size)
{
	struct stat st;
	FILE *fp = NULL;

	if (stat(filename, &st) == -1 || (fp = fopen(filename, "r")) == NULL) {
		return NULL;
	}
	size_t filesize = st.st_size;
	if (filesize == 0) {
		return NULL;
	}
	char *ibuf = malloc(filesize * sizeof(char));
	if (ibuf == NULL) {
		return NULL;
	}
	size_t amount = fread(ibuf, 1, filesize, fp);
	if (amount != filesize) {
		fprintf(stderr, "ERROR: could not read %ld bytes from %s\n", filesize, filename);
		free(ibuf);
		return NULL;
	}
	fclose(fp);

	if (size) {
		*size = filesize;
	}

	return ibuf;
}

int main(int argc, char **argv)
{
	idevice_t device = NULL;
	lockdownd_client_t client = NULL;
	instproxy_client_t ipc = NULL;
	instproxy_error_t err;
	np_client_t np = NULL;
	afc_client_t afc = NULL;
	lockdownd_service_descriptor_t service = NULL;
	int res = EXIT_FAILURE;
	char *bundleidentifier = NULL;

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	parse_opts(argc, argv);

	argc -= optind;
	argv += optind;

	if (IDEVICE_E_SUCCESS != idevice_new_with_options(&device, udid, (use_network) ? IDEVICE_LOOKUP_NETWORK : IDEVICE_LOOKUP_USBMUX)) {
		if (udid) {
			fprintf(stderr, "No device found with udid %s.\n", udid);
		} else {
			fprintf(stderr, "No device found.\n");
		}
		return EXIT_FAILURE;
	}

	if (!udid) {
		idevice_get_udid(device, &udid);
	}

	lockdownd_error_t lerr = lockdownd_client_new_with_handshake(device, &client, "ideviceinstaller");
	if (lerr != LOCKDOWN_E_SUCCESS) {
		fprintf(stderr, "Could not connect to lockdownd: %s. Exiting.\n", lockdownd_strerror(lerr));
		goto leave_cleanup;
	}

	if (use_notifier) {
		lerr =lockdownd_start_service(client, "com.apple.mobile.notification_proxy", &service);
		if (lerr != LOCKDOWN_E_SUCCESS) {
			fprintf(stderr,	"Could not start com.apple.mobile.notification_proxy: %s\n", lockdownd_strerror(lerr));
			goto leave_cleanup;
		}

		np_error_t nperr = np_client_new(device, service, &np);

		if (service) {
			lockdownd_service_descriptor_free(service);
		}
		service = NULL;

		if (nperr != NP_E_SUCCESS) {
			fprintf(stderr, "Could not connect to notification_proxy!\n");
			goto leave_cleanup;
		}

		np_set_notify_callback(np, notifier, NULL);

		const char *noties[3] = { NP_APP_INSTALLED, NP_APP_UNINSTALLED, NULL };

		np_observe_notifications(np, noties);
	}

run_again:
	if (service) {
		lockdownd_service_descriptor_free(service);
	}
	service = NULL;

	lerr = lockdownd_start_service(client, "com.apple.mobile.installation_proxy", &service);
	if (lerr != LOCKDOWN_E_SUCCESS) {
		fprintf(stderr, "Could not start com.apple.mobile.installation_proxy: %s\n", lockdownd_strerror(lerr));
		goto leave_cleanup;
	}

	err = instproxy_client_new(device, service, &ipc);

	if (service) {
		lockdownd_service_descriptor_free(service);
	}
	service = NULL;

	if (err != INSTPROXY_E_SUCCESS) {
		fprintf(stderr, "Could not connect to installation_proxy!\n");
		goto leave_cleanup;
	}

	setbuf(stdout, NULL);

	free(last_status);
	last_status = NULL;

	notification_expected = 0;

	if (cmd == CMD_LIST_APPS) {
		plist_t client_opts = instproxy_client_options_new();
		instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);
		plist_t apps = NULL;

		if (opt_list_system && opt_list_user) {
			plist_dict_remove_item(client_opts, "ApplicationType");
		} else if (opt_list_system) {
			instproxy_client_options_add(client_opts, "ApplicationType", "System", NULL);
		} else if (opt_list_user) {
			instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);
		}

		if (bundle_ids) {
			plist_dict_set_item(client_opts, "BundleIDs", plist_copy(bundle_ids));
		}

		if (!output_format && !return_attrs) {
			return_attrs = plist_new_array();
			plist_array_append_item(return_attrs, plist_new_string("CFBundleIdentifier"));
			plist_array_append_item(return_attrs, plist_new_string("CFBundleShortVersionString"));
			plist_array_append_item(return_attrs, plist_new_string("CFBundleDisplayName"));
		}

		if (return_attrs) {
			instproxy_client_options_add(client_opts, "ReturnAttributes", return_attrs, NULL);
		}

		if (output_format) {
			err = instproxy_browse(ipc, client_opts, &apps);

			if (!apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
				fprintf(stderr, "ERROR: instproxy_browse returnd an invalid plist!\n");
				goto leave_cleanup;
			}
			char *buf = NULL;
			uint32_t len = 0;
			if (output_format == FORMAT_XML) {
				plist_err_t perr = plist_to_xml(apps, &buf, &len);
				if (perr != PLIST_ERR_SUCCESS) {
					fprintf(stderr, "ERROR: Failed to convert data to XML format (%d).\n", perr);
				}
			} else if (output_format == FORMAT_JSON) {
				/* for JSON, we need to convert some stuff since it doesn't support PLIST_DATA nodes */
				plist_array_iter aiter = NULL;
				plist_array_new_iter(apps, &aiter);
				plist_t entry = NULL;
				do {
					plist_array_next_item(apps, aiter, &entry);
					if (!entry) break;
					plist_t items = plist_dict_get_item(entry, "UIApplicationShortcutItems");
					plist_array_iter inner = NULL;
					plist_array_new_iter(items, &inner);
					plist_t item = NULL;
					do {
						plist_array_next_item(items, inner, &item);
						if (!item) break;
						plist_t userinfo = plist_dict_get_item(item, "UIApplicationShortcutItemUserInfo");
						if (userinfo) {
							plist_t data_node = plist_dict_get_item(userinfo, "data");

							if (data_node) {
								char *strbuf = NULL;
								uint32_t buflen = 0;
								plist_write_to_string(data_node, &strbuf, &buflen, PLIST_FORMAT_LIMD, PLIST_OPT_NO_NEWLINE);
								plist_set_string_val(data_node, strbuf);
								free(strbuf);
							}
						}
					} while (item);
					free(inner);
				} while (entry);
				free(aiter);
				plist_err_t perr = plist_to_json(apps, &buf, &len, 1);
				if (perr != PLIST_ERR_SUCCESS) {
					fprintf(stderr, "ERROR: Failed to convert data to JSON format (%d).\n", perr);
				}
			}
			if (buf) {
				puts(buf);
				free(buf);
			}
			plist_free(apps);
			res = 0;
			goto leave_cleanup;
		}

		print_apps_header();

		err = instproxy_browse_with_callback(ipc, client_opts, status_cb, NULL);
		if (err == INSTPROXY_E_RECEIVE_TIMEOUT) {
			fprintf(stderr, "NOTE: timeout waiting for device to browse apps, trying again...\n");
		}

		instproxy_client_options_free(client_opts);
		if (err != INSTPROXY_E_SUCCESS) {
			fprintf(stderr, "ERROR: instproxy_browse returned %d\n", err);
			goto leave_cleanup;
		}

		wait_for_command_complete = 1;
		notification_expected = 0;
	} else if (cmd == CMD_INSTALL || cmd == CMD_UPGRADE) {
		plist_t sinf = NULL;
		plist_t meta = NULL;
		char *pkgname = NULL;
		struct stat fst;
		uint64_t af = 0;
		char buf[8192];

		lockdownd_service_descriptor_free(service);
		service = NULL;

		lerr = lockdownd_start_service(client, "com.apple.afc", &service);
		if (lerr != LOCKDOWN_E_SUCCESS) {
			fprintf(stderr, "Could not start com.apple.afc: %s\n", lockdownd_strerror(lerr));
			goto leave_cleanup;
		}

		lockdownd_client_free(client);
		client = NULL;

		if (afc_client_new(device, service, &afc) != AFC_E_SUCCESS) {
			fprintf(stderr, "Could not connect to AFC!\n");
			goto leave_cleanup;
		}

		if (stat(cmdarg, &fst) != 0) {
			fprintf(stderr, "ERROR: stat: %s: %s\n", cmdarg, strerror(errno));
			goto leave_cleanup;
		}

		char **strs = NULL;
		if (afc_get_file_info(afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
			if (afc_make_directory(afc, PKG_PATH) != AFC_E_SUCCESS) {
				fprintf(stderr, "WARNING: Could not create directory '%s' on device!\n", PKG_PATH);
			}
		}
		if (strs) {
			int i = 0;
			while (strs[i]) {
				free(strs[i]);
				i++;
			}
			free(strs);
		}

		plist_t client_opts = instproxy_client_options_new();

		/* open install package */
		int errp = 0;
		struct zip *zf = NULL;

		if ((strlen(cmdarg) > 5) && (strcmp(&cmdarg[strlen(cmdarg)-5], ".ipcc") == 0)) {
			zf = zip_open(cmdarg, 0, &errp);
			if (!zf) {
				fprintf(stderr, "ERROR: zip_open: %s: %d\n", cmdarg, errp);
				goto leave_cleanup;
			}

			char* ipcc = strdup(cmdarg);
			if ((asprintf(&pkgname, "%s/%s", PKG_PATH, basename(ipcc)) > 0) && pkgname) {
				afc_make_directory(afc, pkgname);
			}

			printf("Uploading %s package contents... ", basename(ipcc));

			/* extract the contents of the .ipcc file to PublicStaging/<name>.ipcc directory */
			zip_int64_t numzf = (zip_int64_t)zip_get_num_entries(zf, 0);
			zip_int64_t i = 0;
			for (i = 0; numzf > 0 && i < numzf; i++) {
				const char* zname = zip_get_name(zf, i, 0);
				char* dstpath = NULL;
				if (!zname) continue;
				if (zname[strlen(zname)-1] == '/') {
					// directory
					if ((asprintf(&dstpath, "%s/%s/%s", PKG_PATH, basename(ipcc), zname) > 0) && dstpath) {
						afc_make_directory(afc, dstpath);						}
					free(dstpath);
					dstpath = NULL;
				} else {
					// file
					struct zip_file* zfile = zip_fopen_index(zf, i, 0);
					if (!zfile) continue;

					if ((asprintf(&dstpath, "%s/%s/%s", PKG_PATH, basename(ipcc), zname) <= 0) || !dstpath || (afc_file_open(afc, dstpath, AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS)) {
						fprintf(stderr, "ERROR: can't open afc://%s for writing\n", dstpath);
						free(dstpath);
						dstpath = NULL;
						zip_fclose(zfile);
						continue;
					}

					struct zip_stat zs;
					zip_stat_init(&zs);
					if (zip_stat_index(zf, i, 0, &zs) != 0) {
						fprintf(stderr, "ERROR: zip_stat_index %" PRIu64 " failed!\n", i);
						free(dstpath);
						dstpath = NULL;
						zip_fclose(zfile);
						continue;
					}

					free(dstpath);
					dstpath = NULL;

					zip_uint64_t zfsize = 0;
					while (zfsize < zs.size) {
						zip_int64_t amount = zip_fread(zfile, buf, sizeof(buf));
						if (amount == 0) {
							break;
						}

						if (amount > 0) {
							uint32_t written, total = 0;
							while (total < amount) {
								written = 0;
								if (afc_file_write(afc, af, buf, amount, &written) !=
									AFC_E_SUCCESS) {
									fprintf(stderr, "AFC Write error!\n");
									break;
								}
								total += written;
							}
							if (total != amount) {
								fprintf(stderr, "Error: wrote only %d of %" PRIi64 "\n", total, amount);
								afc_file_close(afc, af);
								zip_fclose(zfile);
								free(dstpath);
								goto leave_cleanup;
							}
						}

						zfsize += amount;
					}

					afc_file_close(afc, af);
					af = 0;

					zip_fclose(zfile);
				}
			}
			free(ipcc);
			printf("DONE.\n");

			instproxy_client_options_add(client_opts, "PackageType", "CarrierBundle", NULL);
		} else if (S_ISDIR(fst.st_mode)) {
			/* upload developer app directory */
			instproxy_client_options_add(client_opts, "PackageType", "Developer", NULL);

			if (asprintf(&pkgname, "%s/%s", PKG_PATH, basename(cmdarg)) < 0) {
				fprintf(stderr, "ERROR: Out of memory allocating pkgname!?\n");
				goto leave_cleanup;
			}

			printf("Uploading %s package contents... ", basename(cmdarg));
			afc_upload_dir(afc, cmdarg, pkgname);
			printf("DONE.\n");

			/* extract the CFBundleIdentifier from the package */

			/* construct full filename to Info.plist */
			char *filename = (char*)malloc(strlen(cmdarg)+11+1);
			strcpy(filename, cmdarg);
			strcat(filename, "/Info.plist");

			struct stat st;
			FILE *fp = NULL;

			if (stat(filename, &st) == -1 || (fp = fopen(filename, "r")) == NULL) {
				fprintf(stderr, "ERROR: could not locate %s in app!\n", filename);
				free(filename);
				goto leave_cleanup;
			}
			size_t filesize = st.st_size;
			char *ibuf = malloc(filesize * sizeof(char));
			size_t amount = fread(ibuf, 1, filesize, fp);
			if (amount != filesize) {
				fprintf(stderr, "ERROR: could not read %u bytes from %s\n", (uint32_t)filesize, filename);
				free(filename);
				goto leave_cleanup;
			}
			fclose(fp);
			free(filename);

			plist_t info = NULL;
			if (memcmp(ibuf, "bplist00", 8) == 0) {
				plist_from_bin(ibuf, filesize, &info);
			} else {
				plist_from_xml(ibuf, filesize, &info);
			}
			free(ibuf);

			if (!info) {
				fprintf(stderr, "ERROR: could not parse Info.plist!\n");
				goto leave_cleanup;
			}

			plist_t bname = plist_dict_get_item(info, "CFBundleIdentifier");
			if (bname) {
				plist_get_string_val(bname, &bundleidentifier);
			}
			plist_free(info);
			info = NULL;
		} else {
			zf = zip_open(cmdarg, 0, &errp);
			if (!zf) {
				fprintf(stderr, "ERROR: zip_open: %s: %d\n", cmdarg, errp);
				goto leave_cleanup;
			}

			char *zbuf = NULL;
			uint32_t len = 0;
			plist_t meta_dict = NULL;

			if (extmeta) {
				size_t flen = 0;
				zbuf = buf_from_file(extmeta, &flen);
				if (zbuf && flen) {
					meta = plist_new_data(zbuf, flen);
					if (memcmp(zbuf, "bplist00", 8) == 0) {
						plist_from_bin(zbuf, flen, &meta_dict);
					} else {
						plist_from_xml(zbuf, flen, &meta_dict);
					}
					free(zbuf);
				} else {
					fprintf(stderr, "WARNING: could not load external iTunesMetadata %s!\n", extmeta);
				}
				zbuf = NULL;
			}

			if (!meta && !meta_dict) {
				/* extract iTunesMetadata.plist from package */
				if (zip_get_contents(zf, ITUNES_METADATA_PLIST_FILENAME, 0, &zbuf, &len) == 0) {
					meta = plist_new_data(zbuf, len);
					if (memcmp(zbuf, "bplist00", 8) == 0) {
						plist_from_bin(zbuf, len, &meta_dict);
					} else {
						plist_from_xml(zbuf, len, &meta_dict);
					}
				} else {
					fprintf(stderr, "WARNING: could not locate %s in archive!\n", ITUNES_METADATA_PLIST_FILENAME);
				}
				free(zbuf);
			}

			/* determine .app directory in archive */
			zbuf = NULL;
			len = 0;
			plist_t info = NULL;
			char* filename = NULL;
			char* app_directory_name = NULL;

			if (zip_get_app_directory(zf, &app_directory_name)) {
				fprintf(stderr, "ERROR: Unable to locate .app directory in archive. Make sure it is inside a 'Payload' directory.\n");
				goto leave_cleanup;
			}

			/* construct full filename to Info.plist */
			filename = (char*)malloc(strlen(app_directory_name)+10+1);
			strcpy(filename, app_directory_name);
			free(app_directory_name);
			app_directory_name = NULL;
			strcat(filename, "Info.plist");

			if (zip_get_contents(zf, filename, 0, &zbuf, &len) < 0) {
				fprintf(stderr, "WARNING: could not locate %s in archive!\n", filename);
				free(filename);
				zip_unchange_all(zf);
				zip_close(zf);
				goto leave_cleanup;
			}
			free(filename);
			if (memcmp(zbuf, "bplist00", 8) == 0) {
				plist_from_bin(zbuf, len, &info);
			} else {
				plist_from_xml(zbuf, len, &info);
			}
			free(zbuf);

			if (!info) {
				fprintf(stderr, "Could not parse Info.plist!\n");
				zip_unchange_all(zf);
				zip_close(zf);
				goto leave_cleanup;
			}

			char *bundleexecutable = NULL;

			plist_t bname = plist_dict_get_item(info, "CFBundleExecutable");
			if (bname) {
				plist_get_string_val(bname, &bundleexecutable);
			}

			bname = plist_dict_get_item(info, "CFBundleIdentifier");
			if (bname) {
				plist_get_string_val(bname, &bundleidentifier);
			}
			plist_free(info);
			info = NULL;

			if (!bundleexecutable) {
				fprintf(stderr, "Could not determine value for CFBundleExecutable!\n");
				zip_unchange_all(zf);
				zip_close(zf);
				goto leave_cleanup;
			}

			if (extsinf) {
				size_t flen = 0;
				zbuf = buf_from_file(extsinf, &flen);
				if (zbuf && flen) {
					sinf = plist_new_data(zbuf, flen);
					free(zbuf);
				} else {
					fprintf(stderr, "WARNING: could not load external SINF %s!\n", extsinf);
				}
				zbuf = NULL;
			}

			if (!sinf) {
				char *sinfname = NULL;
				if (asprintf(&sinfname, "Payload/%s.app/SC_Info/%s.sinf", bundleexecutable, bundleexecutable) < 0) {
					fprintf(stderr, "Out of memory!?\n");
					goto leave_cleanup;
				}
				free(bundleexecutable);

				/* extract .sinf from package */
				zbuf = NULL;
				len = 0;
				if (zip_get_contents(zf, sinfname, 0, &zbuf, &len) == 0) {
					sinf = plist_new_data(zbuf, len);
				} else {
					fprintf(stderr, "WARNING: could not locate %s in archive!\n", sinfname);
				}
				free(sinfname);
				free(zbuf);
			}

			/* copy archive to device */
			pkgname = NULL;
			if (asprintf(&pkgname, "%s/%s", PKG_PATH, bundleidentifier) < 0) {
				fprintf(stderr, "Out of memory!?\n");
				goto leave_cleanup;
			}

			printf("Copying '%s' to device... ", cmdarg);

			if (afc_upload_file(afc, cmdarg, pkgname) < 0) {
				printf("FAILED\n");
				free(pkgname);
				goto leave_cleanup;
			}

			printf("DONE.\n");

			if (bundleidentifier) {
				instproxy_client_options_add(client_opts, "CFBundleIdentifier", bundleidentifier, NULL);
			}
			if (sinf) {
				instproxy_client_options_add(client_opts, "ApplicationSINF", sinf, NULL);
			}
			if (meta) {
				instproxy_client_options_add(client_opts, "iTunesMetadata", meta, NULL);
			}
		}
		if (zf) {
			zip_unchange_all(zf);
			zip_close(zf);
		}

		/* perform installation or upgrade */
		if (cmd == CMD_INSTALL) {
			printf("Installing '%s'\n", bundleidentifier);
			instproxy_install(ipc, pkgname, client_opts, status_cb, NULL);
		} else {
			printf("Upgrading '%s'\n", bundleidentifier);
			instproxy_upgrade(ipc, pkgname, client_opts, status_cb, NULL);
		}
		instproxy_client_options_free(client_opts);
		free(pkgname);
		wait_for_command_complete = 1;
		notification_expected = 1;
	} else if (cmd == CMD_UNINSTALL) {
		printf("Uninstalling '%s'\n", cmdarg);
		instproxy_uninstall(ipc, cmdarg, NULL, status_cb, NULL);
		wait_for_command_complete = 1;
		notification_expected = 0;
	} else if (cmd == CMD_LIST_ARCHIVES) {
		plist_t dict = NULL;

		err = instproxy_lookup_archives(ipc, NULL, &dict);
		if (err != INSTPROXY_E_SUCCESS) {
			fprintf(stderr, "ERROR: lookup_archives returned %d\n", err);
			goto leave_cleanup;
		}

		if (!dict) {
			fprintf(stderr, "ERROR: lookup_archives did not return a plist!?\n");
			goto leave_cleanup;
		}

		if (output_format) {
			char *buf = NULL;
			uint32_t len = 0;
			if (output_format == FORMAT_XML) {
				plist_err_t perr = plist_to_xml(dict, &buf, &len);
				if (perr != PLIST_ERR_SUCCESS) {
					fprintf(stderr, "ERROR: Failed to convert data to XML format (%d).\n", perr);
				}
			} else if (output_format == FORMAT_JSON) {
				plist_err_t perr = plist_to_json(dict, &buf, &len, 1);
				if (perr != PLIST_ERR_SUCCESS) {
					fprintf(stderr, "ERROR: Failed to convert data to JSON format (%d).\n", perr);
				}
			}
			if (buf) {
				puts(buf);
				free(buf);
			}
			plist_free(dict);
			goto leave_cleanup;
		}
		plist_dict_iter iter = NULL;
		plist_t node = NULL;
		char *key = NULL;

		printf("Total: %d archived apps\n", plist_dict_get_size(dict));
		plist_dict_new_iter(dict, &iter);
		if (!iter) {
			plist_free(dict);
			fprintf(stderr, "ERROR: Could not create plist_dict_iter!\n");
			goto leave_cleanup;
		}
		do {
			key = NULL;
			node = NULL;
			plist_dict_next_item(dict, iter, &key, &node);
			if (key && (plist_get_node_type(node) == PLIST_DICT)) {
				char *s_dispName = NULL;
				char *s_version = NULL;
				plist_t dispName =
					plist_dict_get_item(node, "CFBundleDisplayName");
				plist_t version =
					plist_dict_get_item(node, "CFBundleShortVersionString");
				if (dispName) {
					plist_get_string_val(dispName, &s_dispName);
				}
				if (version) {
					plist_get_string_val(version, &s_version);
				}
				if (!s_dispName) {
					s_dispName = strdup(key);
				}
				if (s_version) {
					printf("%s - %s %s\n", key, s_dispName, s_version);
					free(s_version);
				} else {
					printf("%s - %s\n", key, s_dispName);
				}
				free(s_dispName);
				free(key);
			}
		}
		while (node);
		plist_free(dict);
	} else if (cmd == CMD_ARCHIVE) {
		plist_t client_opts = NULL;

		if (skip_uninstall || app_only || docs_only) {
			client_opts = instproxy_client_options_new();
			if (skip_uninstall) {
				instproxy_client_options_add(client_opts, "SkipUninstall", 1, NULL);
			}
			if (app_only) {
				instproxy_client_options_add(client_opts, "ArchiveType", "ApplicationOnly", NULL);
			} else if (docs_only) {
				instproxy_client_options_add(client_opts, "ArchiveType", "DocumentsOnly", NULL);
			}
		}

		if (copy_path) {
			struct stat fst;
			if (stat(copy_path, &fst) != 0) {
				fprintf(stderr, "ERROR: stat: %s: %s\n", copy_path, strerror(errno));
				goto leave_cleanup;
			}

			if (!S_ISDIR(fst.st_mode)) {
				fprintf(stderr, "ERROR: '%s' is not a directory as expected.\n", copy_path);
				goto leave_cleanup;
			}

			if (service) {
				lockdownd_service_descriptor_free(service);
			}
			service = NULL;

			if ((lockdownd_start_service(client, "com.apple.afc", &service) != LOCKDOWN_E_SUCCESS) || !service) {
				fprintf(stderr, "Could not start com.apple.afc!\n");
				goto leave_cleanup;
			}

			lockdownd_client_free(client);
			client = NULL;

			if (afc_client_new(device, service, &afc) != AFC_E_SUCCESS) {
				fprintf(stderr, "Could not connect to AFC!\n");
				goto leave_cleanup;
			}
		}

		instproxy_archive(ipc, cmdarg, client_opts, status_cb, NULL);

		instproxy_client_options_free(client_opts);
		wait_for_command_complete = 1;
		if (skip_uninstall) {
			notification_expected = 0;
		} else {
			notification_expected = 1;
		}

		idevice_wait_for_command_to_complete();

		if (copy_path) {
			if (err_occurred) {
				afc_client_free(afc);
				afc = NULL;
				goto leave_cleanup;
			}
			FILE *f = NULL;
			uint64_t af = 0;
			/* local filename */
			char *localfile = NULL;
			if (asprintf(&localfile, "%s/%s.ipa", copy_path, cmdarg) < 0) {
				fprintf(stderr, "Out of memory!?\n");
				goto leave_cleanup;
			}

			f = fopen(localfile, "wb");
			if (!f) {
				fprintf(stderr, "ERROR: fopen: %s: %s\n", localfile, strerror(errno));
				free(localfile);
				goto leave_cleanup;
			}

			/* remote filename */
			char *remotefile = NULL;
			if (asprintf(&remotefile, "%s/%s.zip", APPARCH_PATH, cmdarg) < 0) {
				fprintf(stderr, "Out of memory!?\n");
				goto leave_cleanup;
			}

			uint32_t fsize = 0;
			char **fileinfo = NULL;
			if ((afc_get_file_info(afc, remotefile, &fileinfo) != AFC_E_SUCCESS) || !fileinfo) {
				fprintf(stderr, "ERROR getting AFC file info for '%s' on device!\n", remotefile);
				fclose(f);
				free(remotefile);
				free(localfile);
				goto leave_cleanup;
			}

			int i;
			for (i = 0; fileinfo[i]; i+=2) {
				if (!strcmp(fileinfo[i], "st_size")) {
					fsize = atoi(fileinfo[i+1]);
					break;
				}
			}
			i = 0;
			while (fileinfo[i]) {
				free(fileinfo[i]);
				i++;
			}
			free(fileinfo);

			if (fsize == 0) {
				fprintf(stderr, "Hm... remote file length could not be determined. Cannot copy.\n");
				fclose(f);
				free(remotefile);
				free(localfile);
				goto leave_cleanup;
			}

			if ((afc_file_open(afc, remotefile, AFC_FOPEN_RDONLY, &af) != AFC_E_SUCCESS) || !af) {
				fclose(f);
				fprintf(stderr, "ERROR: could not open '%s' on device for reading!\n", remotefile);
				free(remotefile);
				free(localfile);
				goto leave_cleanup;
			}

			/* copy file over */
			printf("Copying '%s' --> '%s'... ", remotefile, localfile);
			free(remotefile);
			free(localfile);

			uint32_t amount = 0;
			uint32_t total = 0;
			char buf[8192];

			do {
				if (afc_file_read(afc, af, buf, sizeof(buf), &amount) != AFC_E_SUCCESS) {
					fprintf(stderr, "AFC Read error!\n");
					break;
				}

				if (amount > 0) {
					size_t written = fwrite(buf, 1, amount, f);
					if (written != amount) {
						fprintf(stderr, "Error when writing %d bytes to local file!\n", amount);
						break;
					}
					total += written;
				}
			} while (amount > 0);

			afc_file_close(afc, af);
			fclose(f);

			printf("DONE.\n");

			if (total != fsize) {
				fprintf(stderr, "WARNING: remote and local file sizes don't match (%d != %d)\n", fsize, total);
				if (remove_after_copy) {
					fprintf(stderr, "NOTE: archive file will NOT be removed from device\n");
					remove_after_copy = 0;
				}
			}

			if (remove_after_copy) {
				/* remove archive if requested */
				printf("Removing '%s'\n", cmdarg);
				cmd = CMD_REMOVE_ARCHIVE;
				if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(device, &client, "ideviceinstaller")) {
					fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
					goto leave_cleanup;
				}
				goto run_again;
			}
		}
		goto leave_cleanup;
	} else if (cmd == CMD_RESTORE) {
		instproxy_restore(ipc, cmdarg, NULL, status_cb, NULL);
		wait_for_command_complete = 1;
		notification_expected = 1;
	} else if (cmd == CMD_REMOVE_ARCHIVE) {
		instproxy_remove_archive(ipc, cmdarg, NULL, status_cb, NULL);
		wait_for_command_complete = 1;
	} else {
		printf("ERROR: no command selected?! This should not be reached!\n");
		res = 2;
		goto leave_cleanup;
	}

	/* not needed anymore */
	lockdownd_client_free(client);
	client = NULL;

	idevice_wait_for_command_to_complete();
	res = 0;

leave_cleanup:
	np_client_free(np);
	instproxy_client_free(ipc);
	afc_client_free(afc);
	lockdownd_client_free(client);
	idevice_free(device);

	free(udid);
	free(copy_path);
	free(extsinf);
	free(extmeta);
	free(bundleidentifier);
	plist_free(bundle_ids);
	plist_free(return_attrs);

	if (err_occurred && !res) {
		res = 128;
	}

	return res;
}
