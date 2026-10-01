# UniTimeTable Conflict Optimizer — Frontend Starter

This is a presentation-quality React + Express frontend for the current C + MySQL project.

## Architecture

React / Vite → Express API → MySQL

`scheduler.c` remains the DS scheduling engine. The web UI does **not** reimplement graph coloring or backtracking.

## Included UI

- Demo faculty selection: Prathm, Vrukshita, Shreya
- Dashboard
- My Timetable
- Notifications
- Accept / decline notification flow
- Cancel Class
- Available Room Search
- Makeup Requests
- Responsive dark university dashboard
- Demo fallback data when the API is not connected

## 1. Frontend

```bash
cd frontend
npm install
npm run dev
```

Open:

`http://localhost:5173`

## 2. API server

In another terminal:

```bash
cd server
npm install
cp .env.example .env
npm run dev
```

The API runs on:

`http://localhost:3001`

The Vite dev server proxies `/api` to the API.

## 3. Start with demo mode

The example `.env` has:

```env
DEMO_MODE=true
```

This lets you explore the frontend before wiring MySQL.

## 4. Connect to your actual MySQL database

Set:

```env
DB_HOST=localhost
DB_USER=root
DB_PASSWORD=YOUR_MYSQL_PASSWORD
DB_NAME=timetableminiproject
DB_PORT=3306
DEMO_MODE=false
```

The API uses parameterized SQL queries and keeps credentials on the server, not in React.

## 5. C scheduler bridge

Your current `scheduler.c` is interactive, so it asks for the MySQL password and then asks the user to select a faculty. A browser request cannot safely treat that interactive CLI as a normal API process.

The `/api/timetable/generate` endpoint therefore expects an optional `SCHEDULER_EXECUTABLE` environment variable pointing to a **non-interactive wrapper** for the C scheduler. The wrapper should print one JSON object containing the generated timetable.

Until that bridge is added, the frontend stays usable in demo mode and the C project continues to run independently.

## Suggested next refactor

To make web timetable generation fully live, refactor the C scheduler into:

1. a reusable scheduling function that accepts database/config inputs without `scanf()`;
2. a small CLI wrapper for your current college demo;
3. a JSON-output wrapper executable for the Express API.

That keeps the DS algorithms in C while giving the frontend a clean integration boundary.
