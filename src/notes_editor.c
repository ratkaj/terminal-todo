// lspdiag

#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <notes_editor.h>

#define NOTES_PATH_BUF 4096

int notes_editor_write_tmpfile(const char *text, char *out_path, size_t path_cap)
{
	RETURN_ERR_IF(out_path == NULL, "notes_editor_write_tmpfile: out_path is NULL");

	const char *tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL || tmpdir[0] == '\0')
		tmpdir = "/tmp";

	char path[NOTES_PATH_BUF];
	int n = snprintf(path, sizeof(path), "%s/todo_notes_XXXXXX", tmpdir);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(path),
		"notes_editor_write_tmpfile: tmpdir path too long");

	int fd = mkstemp(path);
	RETURN_ERR_IF(fd < 0, "notes_editor_write_tmpfile: mkstemp failed: %s", strerror(errno));

	if (text != NULL && text[0] != '\0') {
		size_t len = strlen(text);
		ssize_t written = write(fd, text, len);
		if (written < 0 || (size_t)written != len) {
			LERR("notes_editor_write_tmpfile: write failed: %s", strerror(errno));
			close(fd);
			unlink(path);
			return RT_ERROR;
		}
	}
	close(fd);

	if ((size_t)snprintf(out_path, path_cap, "%s", path) >= path_cap) {
		LERR("notes_editor_write_tmpfile: out_path buffer too small");
		unlink(path);
		return RT_ERROR;
	}
	return RT_SUCCESS;
}

int notes_editor_read_tmpfile(const char *path, char **out_text)
{
	RETURN_ERR_IF(path == NULL || out_text == NULL,
		"notes_editor_read_tmpfile: invalid arguments");

	FILE *f = fopen(path, "rb");
	RETURN_ERR_IF(f == NULL,
		"notes_editor_read_tmpfile: fopen(%s) failed: %s", path, strerror(errno));

	if (fseek(f, 0, SEEK_END) != 0) {
		LERR("notes_editor_read_tmpfile: fseek failed: %s", strerror(errno));
		fclose(f);
		return RT_ERROR;
	}
	long size = ftell(f);
	if (size < 0) {
		LERR("notes_editor_read_tmpfile: ftell failed: %s", strerror(errno));
		fclose(f);
		return RT_ERROR;
	}
	rewind(f);

	char *buf = malloc((size_t)size + 1);
	if (buf == NULL) {
		LERR("notes_editor_read_tmpfile: out of memory");
		fclose(f);
		return RT_ERROR;
	}
	size_t read_n = fread(buf, 1, (size_t)size, f);
	fclose(f);
	buf[read_n] = '\0';

	*out_text = buf;
	return RT_SUCCESS;
}

int notes_editor_edit(const char *initial_text, char **out_text)
{
	RETURN_ERR_IF(out_text == NULL, "notes_editor_edit: out_text is NULL");

	char path[NOTES_PATH_BUF];
	RETURN_ERR_IF(notes_editor_write_tmpfile(initial_text, path, sizeof(path)) != RT_SUCCESS,
		"notes_editor_edit: writing tmpfile failed");

	const char *editor = getenv("EDITOR");
	if (editor == NULL || editor[0] == '\0')
		editor = "vi";

	char cmd[NOTES_PATH_BUF + 512];
	int n = snprintf(cmd, sizeof(cmd), "%s %s", editor, path);
	if (n < 0 || (size_t)n >= sizeof(cmd)) {
		LERR("notes_editor_edit: command too long");
		unlink(path);
		return RT_ERROR;
	}

	/* Suspend curses so the child editor gets full control of the terminal. */
	def_prog_mode();
	endwin();

	int status = system(cmd);

	reset_prog_mode();
	doupdate();

	if (status != 0) {
		LERR("notes_editor_edit: editor exited with status %d, notes left unchanged", status);
		unlink(path);
		return RT_ERROR;
	}

	int rc = notes_editor_read_tmpfile(path, out_text);
	unlink(path);
	return rc;
}
