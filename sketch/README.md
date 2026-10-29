# M5Dial UserDemo - Arduino port

Arduino-sketch port of the ESP-IDF M5Dial UserDemo (`../main`). Everything lives
under this folder; the ESP-IDF project is untouched.

- Toolchain and first-time setup: [docs/SETUP.md](docs/SETUP.md)
- Pinned versions: [tools/deps.txt](tools/deps.txt)
- Everyday commands (after `tools/setup.sh`):
  - `tools/build.sh examples/Smoke`
  - `tools/upload.sh examples/Smoke`
  - `tools/monitor.sh`
  - `tools/.venv/bin/python tools/device_test.py examples/Smoke --expect 'boot: smoke'`
  - `tools/.venv/bin/python -m unittest discover tools/tests`

**Safety:** run `tools/backup_flash.sh` once before the first upload; `upload.sh`
refuses to run without a backup in `backup/`.
