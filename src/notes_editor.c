// lspdiag

#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <common.h>
#include <notes_editor.h>

#define NOTES_PATH_BUF 4096

/* Create @p name (a mkstemps() template) in @p dir; -1 on failure, logged. */
static int open_tmpfile_in(const char *dir, const char *name, int suffix_len,
	char *path, size_t path_cap)
{
	int n = snprintf(path, path_cap, "%s/%s", dir, name);
	if (n < 0 || (size_t)n >= path_cap) {
		LWARN("notes_editor: temp dir path too long: %s", dir);
		return -1;
	}
	int fd = mkstemps(path, suffix_len);
	if (fd < 0)
		LWARN("notes_editor: cannot create a temp file in %s: %s", dir, strerror(errno));
	return fd;
}

/* write() all of @p len bytes, retrying short writes and EINTR. */
static int write_all(int fd, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t w = write(fd, buf, len);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return RT_ERROR;
		}
		buf += w;
		len -= (size_t)w;
	}
	return RT_SUCCESS;
}

/* @p name is a mkstemps() template basename ("..._XXXXXX" + @p suffix_len
   trailing characters kept after the Xs). The file goes in $TMPDIR, or in
   /tmp if $TMPDIR is unset or unusable. */
static int write_tmpfile(const char *name, int suffix_len, const char *text,
	char *out_path, size_t path_cap)
{
	RETURN_ERR_IF(out_path == NULL, "notes_editor_write_tmpfile: out_path is NULL");

	const char *tmpdir = getenv("TMPDIR");
	char path[NOTES_PATH_BUF];
	int fd = -1;
	if (tmpdir != NULL && tmpdir[0] != '\0')
		fd = open_tmpfile_in(tmpdir, name, suffix_len, path, sizeof(path));
	if (fd < 0)
		fd = open_tmpfile_in("/tmp", name, suffix_len, path, sizeof(path));
	RETURN_ERR_IF(fd < 0, "notes_editor_write_tmpfile: no usable temp directory");

	int rc = RT_SUCCESS;
	if (text != NULL && write_all(fd, text, strlen(text)) != RT_SUCCESS) {
		LERR("notes_editor_write_tmpfile: write %s failed: %s", path, strerror(errno));
		rc = RT_ERROR;
	}
	/* close() can report a deferred write error (e.g. a full disk on NFS). */
	if (close(fd) != 0 && rc == RT_SUCCESS) {
		LERR("notes_editor_write_tmpfile: close %s failed: %s", path, strerror(errno));
		rc = RT_ERROR;
	}
	if (rc == RT_SUCCESS && (size_t)snprintf(out_path, path_cap, "%s", path) >= path_cap) {
		LERR("notes_editor_write_tmpfile: out_path buffer too small");
		rc = RT_ERROR;
	}
	if (rc != RT_SUCCESS)
		unlink(path);
	return rc;
}

int notes_editor_write_tmpfile(const char *text, char *out_path, size_t path_cap)
{
	return write_tmpfile("todo_notes_XXXXXX", 0, text, out_path, path_cap);
}

int notes_editor_keep_unsaved(const char *text, char *out_msg, size_t msg_cap)
{
	RETURN_ERR_IF(out_msg == NULL || msg_cap == 0, "notes_editor_keep_unsaved: invalid arguments");

	char path[NOTES_PATH_BUF];
	if (write_tmpfile("todo_unsaved_notes_XXXXXX.txt", 4, text, path, sizeof(path))
			!= RT_SUCCESS) {
		snprintf(out_msg, msg_cap,
			"Notes not saved, and keeping them in a file failed too; see the log.");
		return RT_ERROR;
	}
	snprintf(out_msg, msg_cap, "Notes not saved; see the log.\nKept in %s", path);
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
	bool failed = ferror(f) || read_n != (size_t)size;
	fclose(f);
	if (failed) {
		LERR("notes_editor_read_tmpfile: read %s failed after %zu of %ld bytes",
			path, read_n, size);
		free(buf);
		return RT_ERROR;
	}

	/* Notes are stored as C strings, so a NUL byte would silently cut off
	   everything after it; drop NULs instead. */
	size_t j = 0;
	for (size_t i = 0; i < read_n; i++)
		if (buf[i] != '\0')
			buf[j++] = buf[i];
	if (j != read_n)
		LWARN("notes_editor_read_tmpfile: dropped %zu NUL bytes from %s", read_n - j, path);
	buf[j] = '\0';

	*out_text = buf;
	return RT_SUCCESS;
}

/* Add @p line to @p msg as a second status-line row. */
static void append_line(char *msg, size_t msg_cap, const char *line)
{
	size_t used = strlen(msg);
	if (used + 1 < msg_cap)
		snprintf(msg + used, msg_cap - used, "\n%s", line);
}

typedef enum {
	EDITOR_DONE,      /* exited 0 */
	EDITOR_CANCELLED, /* exited non-zero, e.g. vim's :cq */
	EDITOR_FAILED,    /* could not be started, or was killed by a signal */
} editor_result_t;

/* Run $EDITOR (or vi) on @p path. On EDITOR_FAILED, @p out_msg gets a
   one-line status message; the caller may add a second line. */
static editor_result_t run_editor(const char *path, char *out_msg, size_t msg_cap)
{
	const char *editor = getenv("EDITOR");
	if (editor == NULL || editor[0] == '\0')
		editor = "vi";

	/* $EDITOR may carry arguments ("code -w"), so it goes to the shell as
	   is. The path is single-quoted, with each ' written as '\'', so a
	   space or shell metacharacter in $TMPDIR stays part of one argument. */
	char cmd[NOTES_PATH_BUF * 4 + 512];
	int n = snprintf(cmd, sizeof(cmd), "%s '", editor);
	if (n < 0 || (size_t)n >= sizeof(cmd)) {
		LERR("run_editor: command too long");
		snprintf(out_msg, msg_cap, "Could not start the editor: $EDITOR is too long.");
		return EDITOR_FAILED;
	}
	size_t len = (size_t)n;
	for (const char *c = path; *c; c++) {
		const char *piece = (*c == '\'') ? "'\\''" : NULL;
		size_t piece_len = piece ? 4 : 1;
		if (len + piece_len + 2 > sizeof(cmd)) {
			LERR("run_editor: command too long");
			snprintf(out_msg, msg_cap, "Could not start the editor: temp path too long.");
			return EDITOR_FAILED;
		}
		if (piece)
			memcpy(&cmd[len], piece, piece_len);
		else
			cmd[len] = *c;
		len += piece_len;
	}
	cmd[len++] = '\'';
	cmd[len] = '\0';

	/* Suspend curses so the child editor gets full control of the terminal. */
	def_prog_mode();
	endwin();

	int status = system(cmd);

	reset_prog_mode();
	doupdate();

	/* The shell exits 127 for a command it cannot find and 126 for one it
	   cannot execute; without this check a missing editor looks exactly
	   like the user cancelling. */
	if (status == -1 || (WIFEXITED(status)
			&& (WEXITSTATUS(status) == 126 || WEXITSTATUS(status) == 127))) {
		LERR("run_editor: could not run \"%s\" (status %d)", editor, status);
		snprintf(out_msg, msg_cap,
			"Could not run \"%s\"; set $EDITOR to an installed editor.", editor);
		return EDITOR_FAILED;
	}
	/* bash execs a lone command, so a signal reaches system() directly;
	   dash (Debian/Ubuntu /bin/sh) forks it and exits 128+N instead. */
	int sig = 0;
	if (WIFSIGNALED(status))
		sig = WTERMSIG(status);
	else if (WEXITSTATUS(status) > 128 && WEXITSTATUS(status) < 128 + NSIG)
		sig = WEXITSTATUS(status) - 128;
	if (sig != 0) {
		LERR("run_editor: \"%s\" was killed by signal %d", editor, sig);
		snprintf(out_msg, msg_cap, "The editor \"%s\" was killed by signal %d.", editor, sig);
		return EDITOR_FAILED;
	}
	if (WEXITSTATUS(status) != 0) {
		LINFO("run_editor: \"%s\" exited with status %d", editor, WEXITSTATUS(status));
		return EDITOR_CANCELLED;
	}
	return EDITOR_DONE;
}

int notes_editor_edit(const char *initial_text, char **out_text, char *out_msg, size_t msg_cap)
{
	RETURN_ERR_IF(out_text == NULL || out_msg == NULL || msg_cap == 0,
		"notes_editor_edit: invalid arguments");
	out_msg[0] = '\0';

	char path[NOTES_PATH_BUF];
	if (notes_editor_write_tmpfile(initial_text, path, sizeof(path)) != RT_SUCCESS) {
		snprintf(out_msg, msg_cap, "Could not create a temp file for the notes; see the log.");
		return RT_ERROR;
	}

	editor_result_t res = run_editor(path, out_msg, msg_cap);
	if (res != EDITOR_DONE) {
		if (res == EDITOR_CANCELLED)
			LINFO("notes_editor_edit: editor cancelled, notes left unchanged");
		else
			append_line(out_msg, msg_cap, "Notes unchanged.");
		unlink(path);
		return RT_ERROR;
	}

	/* The edited text exists only in this file now: keep it if it cannot
	   be read back, rather than losing the edit. */
	if (notes_editor_read_tmpfile(path, out_text) != RT_SUCCESS) {
		snprintf(out_msg, msg_cap, "Could not read the edited notes; see the log.\nKept in %s",
			path);
		return RT_ERROR;
	}
	unlink(path);
	return RT_SUCCESS;
}

int notes_editor_view(const char *text, const char *name_hint, char *out_msg, size_t msg_cap)
{
	RETURN_ERR_IF(text == NULL || out_msg == NULL || msg_cap == 0,
		"notes_editor_view: invalid arguments");
	out_msg[0] = '\0';

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
	if (write_tmpfile(name, 4, text, path, sizeof(path)) != RT_SUCCESS) {
		snprintf(out_msg, msg_cap, "Could not create a temp file; see the log.");
		return RT_ERROR;
	}

	/* Read-only hand-off: a non-zero exit is not an error. The file is left
	   in $TMPDIR so the export or report can still be opened after the
	   editor closes; the OS cleans it up with the rest of /tmp. */
	if (run_editor(path, out_msg, msg_cap) == EDITOR_FAILED) {
		/* Point to the file so the text can still be opened another way. */
		char line[NOTES_PATH_BUF + 16];
		snprintf(line, sizeof(line), "The text is in %s", path);
		append_line(out_msg, msg_cap, line);
		return RT_ERROR;
	}
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
