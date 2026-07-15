// SPDX-License-Identifier: GPL-2.0
/*
 * MB (memory bandwidth) control emulation test (ARM MPAM)
 *
 * Exercises info/MB/resource_schemata/ layout, legacy/native mode switching,
 * and schemata visibility for emulated MB/MB_NODE controls.
 */
#include <fcntl.h>
#include <limits.h>

#include "resctrl.h"

#define MB_INFO			INFO_PATH "/MB"
#define MB_SCHEMATA_DIR		MB_INFO "/resource_schemata"
#define MB_MODE_PATH		MB_SCHEMATA_DIR "/mode"
#define MB_CTRL_PATH		MB_SCHEMATA_DIR "/MB"
#define MB_NODE_CTRL_PATH	MB_SCHEMATA_DIR "/MB_NODE"
#define MB_NODE_NESTED_PATH	MB_CTRL_PATH "/MB_NODE"
#define ROOT_SCHEMATA_PATH	RESCTRL_PATH "/schemata"

struct mb_ctrl_paths {
	char mb_status[PATH_MAX];
	char mb_node_status[PATH_MAX];
	bool mb_node_nested;
};

static int read_file(const char *path, char *buf, size_t buflen)
{
	int fd, n;
	ssize_t ret;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -errno;

	ret = read(fd, buf, buflen - 1);
	close(fd);
	if (ret < 0)
		return -errno;

	n = (int)ret;
	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
		n--;
	buf[n] = '\0';

	return 0;
}

static bool path_is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return false;

	return S_ISDIR(st.st_mode);
}

static int mb_resolve_ctrl_paths(struct mb_ctrl_paths *paths)
{
	if (path_is_dir(MB_NODE_NESTED_PATH)) {
		snprintf(paths->mb_status, sizeof(paths->mb_status),
			 "%s/status", MB_CTRL_PATH);
		snprintf(paths->mb_node_status, sizeof(paths->mb_node_status),
			 "%s/status", MB_NODE_NESTED_PATH);
		paths->mb_node_nested = true;
		return 0;
	}

	if (path_is_dir(MB_NODE_CTRL_PATH)) {
		snprintf(paths->mb_status, sizeof(paths->mb_status),
			 "%s/status", MB_CTRL_PATH);
		snprintf(paths->mb_node_status, sizeof(paths->mb_node_status),
			 "%s/status", MB_NODE_CTRL_PATH);
		paths->mb_node_nested = false;
		return 0;
	}

	return -ENOENT;
}

static int mb_read_status(const char *path, char *status, size_t len)
{
	char buf[32];
	int ret;

	ret = read_file(path, buf, sizeof(buf));
	if (ret)
		return ret;

	if (!strcmp(buf, "enabled") || !strcmp(buf, "disabled")) {
		strncpy(status, buf, len);
		status[len - 1] = '\0';
		return 0;
	}

	ksft_print_msg("unexpected status in %s: '%s'\n", path, buf);
	return -EINVAL;
}

static int mb_read_mode(char *buf, size_t len)
{
	return read_file(MB_MODE_PATH, buf, len);
}

static int mb_write_mode(const char *mode)
{
	char msg[32];
	int fd, ret, len;

	len = snprintf(msg, sizeof(msg), "%s\n", mode);
	if (len < 0 || len >= (int)sizeof(msg))
		return -EINVAL;

	fd = open(MB_MODE_PATH, O_WRONLY);
	if (fd < 0)
		return -errno;

	ret = write(fd, msg, len) == len ? 0 : -errno;
	close(fd);

	return ret;
}

static bool mb_mode_active_is(const char *mode_text, const char *mode)
{
	char pattern[32];

	snprintf(pattern, sizeof(pattern), "[%s]", mode);
	return strstr(mode_text, pattern) != NULL;
}

static int mb_read_root_schemata(char *buf, size_t len)
{
	return read_file(ROOT_SCHEMATA_PATH, buf, len);
}

/*
 * resctrl right-aligns schemata names with leading whitespace, so a control
 * line looks like "             MB:0=100...". Treat prefix as matching when it
 * appears at the start of a line, ignoring any leading spaces or tabs.
 */
static const char *schemata_find_line(const char *schemata, const char *prefix)
{
	const char *p;

	if (!schemata || !prefix)
		return NULL;

	for (p = strstr(schemata, prefix); p; p = strstr(p + 1, prefix)) {
		const char *q = p;

		while (q > schemata && (q[-1] == ' ' || q[-1] == '\t'))
			q--;

		if (q == schemata || q[-1] == '\n')
			return p;
	}

	return NULL;
}

static bool schemata_has_prefix(const char *schemata, const char *prefix)
{
	return schemata_find_line(schemata, prefix) != NULL;
}

/*
 * Copy the value portion (everything after the first '=') of the schemata line
 * that starts with @prefix into @out, stopping at the end of that line.
 */
static int schemata_line_value(const char *schemata, const char *prefix,
			       char *out, size_t len)
{
	const char *line, *eq, *end;
	size_t n;

	line = schemata_find_line(schemata, prefix);
	if (!line)
		return -ENOENT;

	eq = strchr(line, '=');
	if (!eq)
		return -EINVAL;

	eq++;
	end = strchr(eq, '\n');
	n = end ? (size_t)(end - eq) : strlen(eq);
	if (n >= len)
		return -ENOSPC;

	memcpy(out, eq, n);
	out[n] = '\0';

	return 0;
}

static int mb_expect_hierarchy(bool nested, const char *why)
{
	bool have_nested = path_is_dir(MB_NODE_NESTED_PATH);
	bool have_sibling = path_is_dir(MB_NODE_CTRL_PATH);

	if (nested) {
		if (!have_nested || have_sibling) {
			ksft_print_msg("%s: expected MB_NODE nested, nested=%d sibling=%d\n",
					 why, have_nested, have_sibling);
			return -EINVAL;
		}
		return 0;
	}

	if (!have_sibling || have_nested) {
		ksft_print_msg("%s: expected MB_NODE sibling of MB, nested=%d sibling=%d\n",
				 why, have_nested, have_sibling);
		return -EINVAL;
	}

	return 0;
}

static int mb_check_layout(const char *mode, const struct mb_ctrl_paths *paths)
{
	char mb_status[16];
	bool native = !strcmp(mode, "native");
	bool expect_nested;
	int ret;

	if (mb_read_status(paths->mb_status, mb_status, sizeof(mb_status)))
		return -errno;

	expect_nested = !native && !strcmp(mb_status, "disabled");

	ret = mb_expect_hierarchy(expect_nested, mode);
	if (ret)
		return ret;

	ksft_print_msg("switched to %s: hierarchy=%s\n", mode,
		       expect_nested ? "nested (MB_NODE under MB)" :
				       "flat (MB_NODE sibling of MB)");
	return 0;
}

static int mb_test_mode_read(void)
{
	char mode[64];

	if (mb_read_mode(mode, sizeof(mode)))
		return -errno;

	if (!mb_mode_active_is(mode, "legacy") && !mb_mode_active_is(mode, "native")) {
		ksft_print_msg("mode file has no active mode: '%s'\n", mode);
		return -EINVAL;
	}

	ksft_print_msg("mode file: %s\n", mode);
	return 0;
}

static int mb_test_schemata_visibility(bool expect_mb_line, const char *mode)
{
	char schemata[4096];
	bool has_mb, has_mb_node;

	if (mb_read_root_schemata(schemata, sizeof(schemata)))
		return -errno;

	has_mb = schemata_has_prefix(schemata, "MB:");
	has_mb_node = schemata_has_prefix(schemata, "MB_NODE:");

	if (expect_mb_line && !has_mb) {
		ksft_print_msg("%s: expected MB: line in schemata\n", mode);
		return -EINVAL;
	}
	if (!expect_mb_line && has_mb) {
		ksft_print_msg("%s: MB: line should be hidden in schemata\n", mode);
		return -EINVAL;
	}
	if (!has_mb_node) {
		ksft_print_msg("%s: expected MB_NODE: line in schemata\n", mode);
		return -EINVAL;
	}

	ksft_print_msg("switched to %s: schemata MB:%d MB_NODE:%d\n",
		       mode, has_mb, has_mb_node);
	return 0;
}

static int mb_test_schemata_mirror_legacy(void)
{
	char schemata[4096];
	char mb_val[1024];
	char node_val[1024];

	if (mb_read_root_schemata(schemata, sizeof(schemata)))
		return -errno;

	if (schemata_line_value(schemata, "MB:", mb_val, sizeof(mb_val)) ||
	    schemata_line_value(schemata, "MB_NODE:", node_val, sizeof(node_val))) {
		ksft_print_msg("legacy mirror: missing MB: or MB_NODE: line\n");
		return -EINVAL;
	}

	if (strcmp(mb_val, node_val)) {
		ksft_print_msg("legacy mirror: MB:%s != MB_NODE:%s\n", mb_val, node_val);
		return -EINVAL;
	}

	ksft_print_msg("switched to legacy: MB mirrors MB_NODE as '%s'\n", mb_val);
	return 0;
}

static int mb_restore_mode(const char *orig_mode)
{
	if (mb_mode_active_is(orig_mode, "legacy"))
		return mb_write_mode("legacy");
	if (mb_mode_active_is(orig_mode, "native"))
		return mb_write_mode("native");

	return 0;
}

/* Confirm the mode file reports @mode as the active ([bracketed]) selection. */
static int mb_verify_active_mode(const char *mode)
{
	char buf[64];

	if (mb_read_mode(buf, sizeof(buf)))
		return -errno;

	if (!mb_mode_active_is(buf, mode)) {
		ksft_print_msg("mode switch to %s failed, mode file: '%s'\n",
			       mode, buf);
		return -EINVAL;
	}

	ksft_print_msg("switched to %s: mode file='%s'\n", mode, buf);
	return 0;
}

/*
 * Switch to @mode and verify the mode file, the info/ resource_schemata
 * hierarchy, and root schemata visibility all reflect the new mode.
 */
static int mb_switch_and_check(const char *mode, const struct mb_ctrl_paths *paths,
			       bool mb_disabled)
{
	bool legacy = !strcmp(mode, "legacy");
	int ret;

	ksft_print_msg("--- switching to %s mode ---\n", mode);

	ret = mb_write_mode(mode);
	if (ret) {
		ksft_print_msg("failed to write %s mode\n", mode);
		return ret;
	}

	ret = mb_verify_active_mode(mode);
	if (ret)
		return ret;

	ret = mb_check_layout(mode, paths);
	if (ret)
		return ret;

	if (!mb_disabled) {
		ksft_print_msg("switched to %s: schemata checks skipped (MB enabled)\n",
			       mode);
		return 0;
	}

	ret = mb_test_schemata_visibility(legacy, mode);
	if (ret)
		return ret;

	if (legacy)
		ret = mb_test_schemata_mirror_legacy();

	return ret;
}

static int mb_emulation_run_test(const struct resctrl_test *test,
				 const struct user_params *uparams)
{
	struct mb_ctrl_paths paths;
	char orig_mode[64];
	char mb_status[16];
	bool mb_disabled;
	int ret;

	(void)test;
	(void)uparams;

	if (mb_resolve_ctrl_paths(&paths)) {
		ksft_print_msg("MB_NODE control not found under resource_schemata\n");
		return -ENOENT;
	}

	if (mb_read_mode(orig_mode, sizeof(orig_mode))) {
		ksft_print_msg("failed to read %s\n", MB_MODE_PATH);
		return -errno;
	}

	if (mb_read_status(paths.mb_status, mb_status, sizeof(mb_status))) {
		ksft_print_msg("failed to read %s\n", paths.mb_status);
		return -errno;
	}

	ksft_print_msg("MB status=%s MB_NODE nested=%d (initial)\n",
		       mb_status, paths.mb_node_nested);

	ret = mb_test_mode_read();
	if (ret)
		goto out_restore;

	mb_disabled = !strcmp(mb_status, "disabled");

	/* Cycle legacy -> native -> legacy, checking each transition. */
	ret = mb_switch_and_check("legacy", &paths, mb_disabled);
	if (ret)
		goto out_restore;

	ret = mb_switch_and_check("native", &paths, mb_disabled);
	if (ret)
		goto out_restore;

	ret = mb_switch_and_check("legacy", &paths, mb_disabled);
	if (ret)
		goto out_restore;

out_restore:
	if (mb_restore_mode(orig_mode))
		ksft_print_msg("warning: failed to restore original mode\n");

	return ret;
}

static bool mb_emulation_feature_check(const struct resctrl_test *test)
{
	(void)test;

	if (!resctrl_resource_exists("MB"))
		return false;

	if (!path_is_dir(MB_SCHEMATA_DIR))
		return false;

	if (!resource_info_file_exists("MB", "resource_schemata/mode"))
		return false;

	return path_is_dir(MB_NODE_CTRL_PATH) || path_is_dir(MB_NODE_NESTED_PATH);
}

struct resctrl_test mb_emulation_test = {
	.name = "mb_emulation",
	.group = "mb",
	.resource = "MB",
	.feature_check = mb_emulation_feature_check,
	.run_test = mb_emulation_run_test,
};
