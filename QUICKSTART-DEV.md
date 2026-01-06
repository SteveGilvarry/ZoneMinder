# 🚀 Quick Start - ZoneMinder Development (M4 Mac)

## Prerequisites
- Docker Desktop for Mac (Apple Silicon)
- 4GB+ RAM allocated to Docker

## One-Line Start

```bash
docker-compose -f docker-compose.dev.yml up -d
```

**That's it!** Access at http://localhost:8080

## Using the Makefile (Easier)

```bash
# Start everything
make -f Makefile.dev dev-up

# View logs
make -f Makefile.dev dev-logs

# Stop everything
make -f Makefile.dev dev-down

# See all commands
make -f Makefile.dev help
```

## Edit PHP Files

1. Edit any file in `./web/`
2. Refresh your browser
3. Changes appear immediately!

No rebuild needed for PHP changes ✨

## Common Tasks

| Task | Command |
|------|---------|
| Start | `make -f Makefile.dev dev-up` |
| Stop | `make -f Makefile.dev dev-down` |
| Logs | `make -f Makefile.dev dev-logs` |
| Shell | `make -f Makefile.dev dev-shell` |
| MySQL | `make -f Makefile.dev dev-mysql` |
| Screenshot | `make -f Makefile.dev dev-test-ui` |

## URLs

- **ZoneMinder UI**: http://localhost:8080
- **PHPMyAdmin**: http://localhost:8081
  - User: `zmuser`
  - Pass: `zmpass`

## For Claude: Testing Workflow

After making PHP changes:

```bash
# 1. Changes are already live (web dir is mounted)
# 2. Capture screenshot to verify
make -f Makefile.dev dev-test-ui

# 3. Check logs for PHP errors
make -f Makefile.dev dev-apache-logs
```

## Need More Info?

See [DOCKER-DEV.md](DOCKER-DEV.md) for complete documentation.

---

**First time?** The initial build takes ~10-15 minutes to compile ZoneMinder for ARM64. Subsequent starts are fast (<30s).
