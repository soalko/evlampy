#!/bin/sh
set -eu

TARGET_EXEC="/usr/local/bin/target_exec"
GUI_EXEC="/usr/local/bin/messenger_gui"
TEST_DIR="/usr/local/lib/evlampy-tests"

run_tests() {
	if [ ! -d "$TEST_DIR" ]; then
		echo "Tests are not available in this image" >&2
		exit 1
	fi
	status=0
	if [ "$#" -gt 0 ]; then
		for name in "$@"; do
			test_bin="$TEST_DIR/$name"
			if [ ! -x "$test_bin" ]; then
				echo "Test binary not found: $name" >&2
				status=1
				continue
			fi
			"$test_bin" || status=$?
		done
	else
		for test_bin in "$TEST_DIR"/*; do
			[ -x "$test_bin" ] || continue
			"$test_bin" || status=$?
		done
	fi
	exit "$status"
}

case "${1:-}" in
	test|tests)
		shift || true
		run_tests "$@"
		;;
	gui|messenger_gui)
		shift || true
		if [ ! -x "$GUI_EXEC" ]; then
			echo "GUI binary is not available in this image" >&2
			exit 1
		fi
		if [ -z "${DISPLAY:-}" ] && [ -z "${QT_QPA_PLATFORM:-}" ]; then
			export QT_QPA_PLATFORM=offscreen
		fi
		exec "$GUI_EXEC" "$@"
		;;
	target_exec|console|cli)
		shift || true
		exec "$TARGET_EXEC" "$@"
		;;
	-h|--help|help)
		cat <<'USAGE'
Usage:
  docker run --rm IMAGE              # run the console demo
  docker run --rm IMAGE --message X  # forward args to target_exec
  docker run --rm IMAGE gui          # run Qt GUI if built
  docker run --rm IMAGE test         # run all bundled tests
USAGE
		;;
	"")
		exec "$TARGET_EXEC"
		;;
	*)
		exec "$TARGET_EXEC" "$@"
		;;
esac

