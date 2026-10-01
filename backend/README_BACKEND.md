# UniTimeTableConflictOptimizer FastAPI backend

This backend connects the existing React frontend to the same MySQL database used by `scheduler.c`.

## Important architecture note

The repository's scheduler is **C**, not C++. FastAPI does not directly call C++/C functions. Both the C scheduler and FastAPI use the same MySQL database:

React -> FastAPI -> MySQL
                  ^
                  |
              scheduler.c

The existing `scheduler.c` asks for the MySQL password interactively, so it cannot be cleanly launched by FastAPI as-is. The `/api/timetable/generate` endpoint therefore mirrors the existing C conflict graph + backtracking coloring logic in Python for now. The database remains the shared source of truth.

## Setup

From the project root:

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

Edit `.env`:

```env
DB_HOST=localhost
DB_PORT=3306
DB_USER=root
DB_PASSWORD=YOUR_MYSQL_PASSWORD
DB_NAME=timetableminiproject
```

Start the API:

```bash
uvicorn app:app --reload --port 8000
```

Test:

```text
http://localhost:8000/docs
http://localhost:8000/health
```

## Connect the React frontend

In `frontend/.env`:

```env
VITE_API_BASE_URL=http://localhost:8000/api
VITE_USE_MOCKS=false
```

Then restart Vite:

```bash
npm run dev
```

Keep both terminals running:

Terminal 1:
```bash
cd backend
source .venv/bin/activate
uvicorn app:app --reload --port 8000
```

Terminal 2:
```bash
cd frontend
npm run dev
```
