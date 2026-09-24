// lspdiag

#include <curses.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <notes_editor.h>

#define NOTES_PATH_BUF 4096

/* @p name is a mkstemps() template basename ("..._XXXXXX" + @p suffix_len
   trailing characters kept after the Xs). */
static int write_tmpfile(const char *name, int suffix_len, const char *text,
	char *out_path, size_t path_cap)
{
	RETURN_ERR_IF(out_path == NULL, "notes_editor_write_tmpfile: out_path is NULL");

	const char *tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL || tmpdir[0] == '\0')
		tmpdir = "/tmp";

	char path[NOTES_PATH_BUF];
	int n = snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(path),
		"notes_editor_write_tmpfile: tmpdir path too long");

	int fd = mkstemps(path, suffix_len);
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

int notes_editor_write_tmpfile(const char *text, char *out_path, size_t path_cap)
{
	return write_tmpfile("todo_notes_XXXXXX", 0, text, out_path, path_cap);
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

/* @return the editor's system() status, or -1 if it could not be started. */
static int run_editor(const char *path)
{
	const char *editor = getenv("EDITOR");
	if (editor == NULL || editor[0] == '\0')
		editor = "vi";

	char cmd[NOTES_PATH_BUF + 512];
	int n = snprintf(cmd, sizeof(cmd), "%s %s", editor, path);
	if (n < 0 || (size_t)n >= sizeof(cmd)) {
		LERR("run_editor: command too long");
		return -1;
	}

	/* Suspend curses so the child editor gets full control of the terminal. */
	def_prog_mode();
	endwin();

	int status = system(cmd);

	reset_prog_mode();
	doupdate();
	return status;
}

int notes_editor_edit(const char *initial_text, char **out_text)
{
	RETURN_ERR_IF(out_text == NULL, "notes_editor_edit: out_text is NULL");

	char path[NOTES_PATH_BUF];
	RETURN_ERR_IF(notes_editor_write_tmpfile(initial_text, path, sizeof(path)) != RT_SUCCESS,
		"notes_editor_edit: writing tmpfile failed");

	int status = run_editor(path);
	if (status != 0) {
		LERR("notes_editor_edit: editor exited with status %d, notes left unchanged", status);
		unlink(path);
		return RT_ERROR;
	}

	int rc = notes_editor_read_tmpfile(path, out_text);
	unlink(path);
	return rc;
}

int notes_editor_view(const char *text, const char *name_hint)
{
	RETURN_ERR_IF(text == NULL, "notes_editor_view: text is NULL");

	/* Name the file after what it shows (e.g. "export_atomrpc") so it's
	   recognisable in the editor and in $TMPDIR afterwards; .txt helps
	   filetype detection. Keep only filename-safe characters. */
	char safe[48] = "";
	size_t k = 0;
	for (const char *c = name_hint; c != NULL && *c && k < sizeof(safe) - 1; c++) {
		unsigned char ch = (unsigned char)*c;
		bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
		safe[k++] = ok ? (char)ch : '_';
	}
	safe[k] = '\0';

	char name[96];
	snprintf(name, sizeof(name), "todo_%s%sXXXXXX.txt", safe, k > 0 ? "_" : "");

	char path[NOTES_PATH_BUF];
	RETURN_ERR_IF(write_tmpfile(name, 4, text, path, sizeof(path)) != RT_SUCCESS,
		"notes_editor_view: writing tmpfile failed");

	/* Read-only hand-off: the editor's exit status doesn't matter. The file
	   is left in $TMPDIR so the export or report can still be opened after
	   the editor closes; the OS cleans it up with the rest of /tmp. */
	run_editor(path);
	return RT_SUCCESS;
}

static const char b64_table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int notes_editor_base64_encode(const unsigned char *data, size_t len, char *out, size_t out_cap)
{
	RETURN_ERR_IF(data == NULL || out == NULL, "notes_editor_base64_encode: invalid arguments");

	size_t needed = 4 * ((len + 2) / 3) + 1;
	RETURN_ERR_IF(out_cap < needed, "notes_editor_base64_encode: out buffer too small");

	size_t i = 0, j = 0;
	while (i + 3 <= len) {
		unsigned int v = ((unsigned int)data[i] << 16) | ((unsigned int)data[i + 1] << 8)
			| (unsigned int)data[i + 2];
		out[j++] = b64_table[(v >> 18) & 0x3F];
		out[j++] = b64_table[(v >> 12) & 0x3F];
		out[j++] = b64_table[(v >> 6) & 0x3F];
		out[j++] = b64_table[v & 0x3F];
		i += 3;
	}

	size_t rem = len - i;
	if (rem == 1) {
		unsigned int v = (unsigned int)data[i] << 16;
		out[j++] = b64_table[(v >> 18) & 0x3F];
		out[j++] = b64_table[(v >> 12) & 0x3F];
		out[j++] = '=';
		out[j++] = '=';
	} else if (rem == 2) {
		unsigned int v = ((unsigned int)data[i] << 16) | ((unsigned int)data[i + 1] << 8);
		out[j++] = b64_table[(v >> 18) & 0x3F];
		out[j++] = b64_table[(v >> 12) & 0x3F];
		out[j++] = b64_table[(v >> 6) & 0x3F];
		out[j++] = '=';
	}
	out[j] = '\0';
	return RT_SUCCESS;
}

int notes_editor_copy_clipboard(const char *text)
{
	RETURN_ERR_IF(text == NULL, "notes_editor_copy_clipboard: text is NULL");

	size_t len = strlen(text);
	size_t cap = 4 * ((len + 2) / 3) + 1;
	char *b64 = malloc(cap);
	RETURN_ERR_IF(b64 == NULL, "notes_editor_copy_clipboard: out of memory");

	if (notes_editor_base64_encode((const unsigned char *)text, len, b64, cap) != RT_SUCCESS) {
		free(b64);
		return RT_ERROR;
	}

	fprintf(stdout, "\033]52;c;%s\a", b64);
	fflush(stdout);

	free(b64);
	return RT_SUCCESS;
}
